/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NetworkLoadHandler.h"
#include "CacheLoadHandler.h"  // CachePromiseHandler

#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIPrincipal.h"
#include "nsIScriptError.h"
#include "nsNetUtil.h"

#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/dom/InternalResponse.h"
#include "mozilla/dom/ServiceWorkerBinding.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/WorkerScope.h"

#include "mozilla/dom/workerinternals/ScriptLoader.h"  // WorkerScriptLoader

using mozilla::ipc::PrincipalInfo;

namespace mozilla {
namespace dom {

namespace workerinternals::loader {

NS_IMPL_ISUPPORTS(NetworkLoadHandler, nsIStreamLoaderObserver,
                  nsIRequestObserver)

NetworkLoadHandler::NetworkLoadHandler(WorkerScriptLoader* aLoader,
                                       ScriptLoadInfo& aLoadInfo)
    : mLoader(aLoader),
      mWorkerPrivate(aLoader->mWorkerPrivate),
      mLoadInfo(aLoadInfo) {
  MOZ_ASSERT(mLoader);
}

NS_IMETHODIMP
NetworkLoadHandler::OnStreamComplete(nsIStreamLoader* aLoader,
                                     nsISupports* aContext, nsresult aStatus,
                                     uint32_t aStringLen,
                                     const uint8_t* aString) {
  nsresult rv = DataReceivedFromNetwork(aLoader, aStatus, aStringLen, aString);
  return mLoader->OnStreamComplete(mLoadInfo, rv);
}

nsresult NetworkLoadHandler::DataReceivedFromNetwork(nsIStreamLoader* aLoader,
                                                     nsresult aStatus,
                                                     uint32_t aStringLen,
                                                     const uint8_t* aString) {
  AssertIsOnMainThread();

  if (!mLoadInfo.mChannel) {
    return NS_BINDING_ABORTED;
  }

  mLoadInfo.mChannel = nullptr;

  if (NS_FAILED(aStatus)) {
    return aStatus;
  }

  NS_ASSERTION(aString, "This should never be null!");

  nsCOMPtr<nsIRequest> request;
  nsresult rv = aLoader->GetRequest(getter_AddRefs(request));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  MOZ_ASSERT(channel);

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  NS_ASSERTION(ssm, "Should never be null!");

  nsCOMPtr<nsIPrincipal> channelPrincipal;
  rv =
      ssm->GetChannelResultPrincipal(channel, getter_AddRefs(channelPrincipal));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsIPrincipal* principal = mWorkerPrivate->GetPrincipal();
  if (!principal) {
    WorkerPrivate* parentWorker = mWorkerPrivate->GetParent();
    MOZ_ASSERT(parentWorker, "Must have a parent!");
    principal = parentWorker->GetPrincipal();
  }

#ifdef DEBUG
  if (mLoader->IsMainWorkerScript()) {
    nsCOMPtr<nsIPrincipal> loadingPrincipal =
        mWorkerPrivate->GetLoadingPrincipal();
    // if we are not in a ServiceWorker, and the principal is not null, then
    // the loading principal must subsume the worker principal if it is not a
    // nullPrincipal (sandbox).
    MOZ_ASSERT(!loadingPrincipal || loadingPrincipal->GetIsNullPrincipal() ||
               principal->GetIsNullPrincipal() ||
               loadingPrincipal->Subsumes(principal));
  }
#endif

  // We don't mute the main worker script becase we've already done
  // same-origin checks on them so we should be able to see their errors.
  // Note that for data: url, where we allow it through the same-origin check
  // but then give it a different origin.
  mLoadInfo.mMutedErrorFlag.emplace(!mLoader->IsMainWorkerScript() &&
                                    !principal->Subsumes(channelPrincipal));

  // Make sure we're not seeing the result of a 404 or something by checking
  // the 'requestSucceeded' attribute on the http channel.
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request);
  nsAutoCString tCspHeaderValue, tCspROHeaderValue, tRPHeaderCValue;

  if (httpChannel) {
    bool requestSucceeded;
    rv = httpChannel->GetRequestSucceeded(&requestSucceeded);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!requestSucceeded) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    Unused << httpChannel->GetResponseHeader("content-security-policy"_ns,
                                             tCspHeaderValue);

    Unused << httpChannel->GetResponseHeader(
        "content-security-policy-report-only"_ns, tCspROHeaderValue);

    Unused << httpChannel->GetResponseHeader("referrer-policy"_ns,
                                             tRPHeaderCValue);

    nsAutoCString sourceMapURL;
    if (nsContentUtils::GetSourceMapURL(httpChannel, sourceMapURL)) {
      mLoadInfo.mSourceMapURL = Some(NS_ConvertUTF8toUTF16(sourceMapURL));
    }
  }

  // May be null.
  Document* parentDoc = mWorkerPrivate->GetDocument();

  // Use the regular ScriptLoader for this grunt work! Should be just fine
  // because we're running on the main thread.
  // Worker scripts are always decoded as UTF-8 per spec. Passing null for a
  // channel and UTF-8 for the hint will always interpret |aString| as UTF-8.
  if (StaticPrefs::dom_worker_script_loader_utf8_parsing_enabled()) {
    mLoadInfo.InitUTF8Script();
    rv = ScriptLoader::ConvertToUTF8(nullptr, aString, aStringLen, u"UTF-8"_ns,
                                     parentDoc, mLoadInfo.mScript.mUTF8,
                                     mLoadInfo.mScriptLength);
  } else {
    mLoadInfo.InitUTF16Script();
    rv = ScriptLoader::ConvertToUTF16(nullptr, aString, aStringLen, u"UTF-8"_ns,
                                      parentDoc, mLoadInfo.mScript.mUTF16,
                                      mLoadInfo.mScriptLength);
  }
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mLoadInfo.ScriptTextIsNull()) {
    if (mLoadInfo.mScriptLength != 0) {
      return NS_ERROR_FAILURE;
    }

    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, "DOM"_ns,
                                    parentDoc, nsContentUtils::eDOM_PROPERTIES,
                                    "EmptyWorkerSourceWarning");
  }

  // Figure out what we actually loaded.
  nsCOMPtr<nsIURI> finalURI;
  rv = NS_GetFinalChannelURI(channel, getter_AddRefs(finalURI));
  NS_ENSURE_SUCCESS(rv, rv);

  if (principal->IsSameOrigin(finalURI)) {
    nsCString filename;
    rv = finalURI->GetSpec(filename);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!filename.IsEmpty()) {
      // This will help callers figure out what their script url resolved to
      // in case of errors.
      mLoadInfo.mURL.Assign(NS_ConvertUTF8toUTF16(filename));
    }
  }

  // Update the principal of the worker and its base URI if we just loaded the
  // worker's primary script.
  if (mLoader->IsMainWorkerScript()) {
    // Take care of the base URI first.
    mWorkerPrivate->SetBaseURI(finalURI);

    // Store the channel info if needed.
    mWorkerPrivate->InitChannelInfo(channel);

    // Our final channel principal should match the loading principal
    // in terms of the origin.  This used to be an assert, but it seems
    // there are some rare cases where this check can fail in practice.
    // Perhaps some browser script setting nsIChannel.owner, etc.
    NS_ENSURE_TRUE(mWorkerPrivate->FinalChannelPrincipalIsValid(channel),
                   NS_ERROR_FAILURE);

    // However, we must still override the principal since the nsIPrincipal
    // URL may be different due to same-origin redirects.  Unfortunately this
    // URL must exactly match the final worker script URL in order to
    // properly set the referrer header on fetch/xhr requests.  If bug 1340694
    // is ever fixed this can be removed.
    rv = mWorkerPrivate->SetPrincipalsAndCSPFromChannel(channel);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIContentSecurityPolicy> csp = mWorkerPrivate->GetCSP();
    // We did inherit CSP in bug 1223647. If we do not already have a CSP, we
    // should get it from the HTTP headers on the worker script.
    if (!csp) {
      rv = mWorkerPrivate->SetCSPFromHeaderValues(tCspHeaderValue,
                                                  tCspROHeaderValue);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      csp->EnsureEventTarget(mWorkerPrivate->MainThreadEventTarget());
    }

    mWorkerPrivate->UpdateReferrerInfoFromHeader(tRPHeaderCValue);

    WorkerPrivate* parent = mWorkerPrivate->GetParent();
    if (parent) {
      // XHR Params Allowed
      mWorkerPrivate->SetXHRParamsAllowed(parent->XHRParamsAllowed());
    }

    nsCOMPtr<nsILoadInfo> chanLoadInfo = channel->LoadInfo();
    if (chanLoadInfo) {
      mLoader->SetController(chanLoadInfo->GetController());
    }

    // If we are loading a blob URL we must inherit the controller
    // from the parent.  This is a bit odd as the blob URL may have
    // been created in a different context with a different controller.
    // For now, though, this is what the spec says.  See:
    //
    // https://github.com/w3c/ServiceWorker/issues/1261
    //
    if (IsBlobURI(mWorkerPrivate->GetBaseURI())) {
      MOZ_DIAGNOSTIC_ASSERT(mLoader->GetController().isNothing());
      mLoader->SetController(mWorkerPrivate->GetParentController());
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
NetworkLoadHandler::OnStartRequest(nsIRequest* aRequest) {
  nsresult rv = PrepareForRequest(aRequest);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRequest->Cancel(rv);
  }

  return rv;
}

nsresult NetworkLoadHandler::PrepareForRequest(nsIRequest* aRequest) {
  AssertIsOnMainThread();

  // If one load info cancels or hits an error, it can race with the start
  // callback coming from another load info.
  if (mLoader->IsCancelled() || !mLoader->GetCacheCreator()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);

  // Checking the MIME type is only required for ServiceWorkers'
  // importScripts, per step 10 of
  // https://w3c.github.io/ServiceWorker/#importscripts
  //
  // "Extract a MIME type from the response’s header list. If this MIME type
  // (ignoring parameters) is not a JavaScript MIME type, return a network
  // error."
  if (mWorkerPrivate->IsServiceWorker()) {
    nsAutoCString mimeType;
    channel->GetContentType(mimeType);

    if (!nsContentUtils::IsJavascriptMIMEType(
            NS_ConvertUTF8toUTF16(mimeType))) {
      const nsCString& scope =
          mWorkerPrivate->GetServiceWorkerRegistrationDescriptor().Scope();

      ServiceWorkerManager::LocalizeAndReportToAllClients(
          scope, "ServiceWorkerRegisterMimeTypeError2",
          nsTArray<nsString>{NS_ConvertUTF8toUTF16(scope),
                             NS_ConvertUTF8toUTF16(mimeType), mLoadInfo.mURL});

      return NS_ERROR_DOM_NETWORK_ERR;
    }
  }

  // Note that importScripts() can redirect.  In theory the main
  // script could also encounter an internal redirect, but currently
  // the assert does not allow that.
  MOZ_ASSERT_IF(mLoader->IsMainScript(), channel == mLoadInfo.mChannel);
  mLoadInfo.mChannel = channel;

  // We synthesize the result code, but its never exposed to content.
  SafeRefPtr<mozilla::dom::InternalResponse> ir =
      MakeSafeRefPtr<mozilla::dom::InternalResponse>(200, "OK"_ns);
  ir->SetBody(mLoadInfo.mCacheReadStream, InternalResponse::UNKNOWN_BODY_SIZE);

  // Drop our reference to the stream now that we've passed it along, so it
  // doesn't hang around once the cache is done with it and keep data alive.
  mLoadInfo.mCacheReadStream = nullptr;

  // Set the channel info of the channel on the response so that it's
  // saved in the cache.
  ir->InitChannelInfo(channel);

  // Save the principal of the channel since its URI encodes the script URI
  // rather than the ServiceWorkerRegistrationInfo URI.
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  NS_ASSERTION(ssm, "Should never be null!");

  nsCOMPtr<nsIPrincipal> channelPrincipal;
  MOZ_TRY(ssm->GetChannelResultPrincipal(channel,
                                         getter_AddRefs(channelPrincipal)));

  UniquePtr<PrincipalInfo> principalInfo(new PrincipalInfo());
  MOZ_TRY(PrincipalToPrincipalInfo(channelPrincipal, principalInfo.get()));

  ir->SetPrincipalInfo(std::move(principalInfo));
  ir->Headers()->FillResponseHeaders(mLoadInfo.mChannel);

  RefPtr<mozilla::dom::Response> response = new mozilla::dom::Response(
      mLoader->GetCacheCreator()->Global(), std::move(ir), nullptr);

  mozilla::dom::RequestOrUSVString request;

  MOZ_ASSERT(!mLoadInfo.mFullURL.IsEmpty());
  request.SetAsUSVString().ShareOrDependUpon(mLoadInfo.mFullURL);

  // This JSContext will not end up executing JS code because here there are
  // no ReadableStreams involved.
  AutoJSAPI jsapi;
  jsapi.Init();

  ErrorResult error;
  RefPtr<Promise> cachePromise = mLoader->GetCacheCreator()->Cache_()->Put(
      jsapi.cx(), request, *response, error);
  error.WouldReportJSException();
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  RefPtr<CachePromiseHandler> promiseHandler =
      new CachePromiseHandler(mLoader, mLoadInfo);
  cachePromise->AppendNativeHandler(promiseHandler);

  mLoadInfo.mCachePromise.swap(cachePromise);
  mLoadInfo.mCacheStatus = ScriptLoadInfo::WritingToCache;

  return NS_OK;
}

}  // namespace workerinternals::loader

}  // namespace dom
}  // namespace mozilla
