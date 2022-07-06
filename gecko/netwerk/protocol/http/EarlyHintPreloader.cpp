/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EarlyHintPreloader.h"

#include "EarlyHintsService.h"
#include "ErrorList.h"
#include "mozilla/CORSMode.h"
#include "mozilla/dom/ReferrerInfo.h"
#include "mozilla/Logging.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsICacheInfoChannel.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIInputStream.h"
#include "nsIReferrerInfo.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"

//
// To enable logging (see mozilla/Logging.h for full details):
//
//    set MOZ_LOG=EarlyHint:5
//    set MOZ_LOG_FILE=earlyhint.log
//
// this enables LogLevel::Debug level information and places all output in
// the file earlyhint.log
//
static mozilla::LazyLogModule gEarlyHintLog("EarlyHint");

#undef LOG
#define LOG(args) MOZ_LOG(gEarlyHintLog, mozilla::LogLevel::Debug, args)

#undef LOG_ENABLED
#define LOG_ENABLED() MOZ_LOG_TEST(gEarlyHintLog, mozilla::LogLevel::Debug)

namespace mozilla::net {

//=============================================================================
// OngoingEarlyHints
//=============================================================================

void OngoingEarlyHints::CancelAllOngoingPreloads() {
  for (auto& el : mOngoingPreloads) {
    el.GetData()->CancelChannel(nsresult::NS_ERROR_ABORT);
  }
}

bool OngoingEarlyHints::Contains(const PreloadHashKey& aKey) {
  return mOngoingPreloads.Contains(aKey);
}

bool OngoingEarlyHints::Add(const PreloadHashKey& aKey,
                            RefPtr<EarlyHintPreloader> aPreloader) {
  return mOngoingPreloads.InsertOrUpdate(aKey, aPreloader);
}

//=============================================================================
// EarlyHintPreloader
//=============================================================================

EarlyHintPreloader::EarlyHintPreloader(nsIURI* aURI) : mURI(aURI) {}

/* static */
Maybe<PreloadHashKey> EarlyHintPreloader::GenerateHashKey(
    ASDestination aAs, nsIURI* aURI, nsIPrincipal* aPrincipal) {
  if (aAs == ASDestination::DESTINATION_IMAGE) {
    return Some(
        PreloadHashKey::CreateAsImage(aURI, aPrincipal, CORSMode::CORS_NONE));
  }
  return Nothing();
}

// static
void EarlyHintPreloader::MaybeCreateAndInsertPreload(
    OngoingEarlyHints* aOngoingEarlyHints, const LinkHeader& aHeader,
    nsIURI* aBaseURI, nsIPrincipal* aTriggeringPrincipal,
    nsICookieJarSettings* aCookieJarSettings) {
  if (!aHeader.mRel.LowerCaseEqualsASCII("preload")) {
    return;
  }

  nsAttrValue as;
  ParseAsValue(aHeader.mAs, as);
  if (as.GetEnumValue() == ASDestination::DESTINATION_INVALID) {
    // return early when it's definitly not an asset type we preload
    // would be caught later as well, e.g. when creating the PreloadHashKey
    return;
  }

  nsCOMPtr<nsIURI> uri;
  // use the base uri
  NS_ENSURE_SUCCESS_VOID(
      NS_NewURI(getter_AddRefs(uri), aHeader.mHref, nullptr, aBaseURI));

  // Only make same origin preloads, the fromPrivateWindow is only read when
  // reportError is enabled, so setting both to false is safe.
  if (NS_FAILED(nsContentUtils::GetSecurityManager()->CheckSameOriginURI(
          aBaseURI, uri, /* reportError */ false,
          /* fromPrivateWindow */ false))) {
    return;
  }

  Maybe<PreloadHashKey> hashKey = GenerateHashKey(
      static_cast<ASDestination>(as.GetEnumValue()), uri, aTriggeringPrincipal);
  if (!hashKey) {
    return;
  }

  if (aOngoingEarlyHints->Contains(*hashKey)) {
    return;
  }

  nsContentPolicyType contentPolicyType = AsValueToContentPolicy(as);
  if (contentPolicyType == nsContentPolicyType::TYPE_INVALID) {
    return;
  }

  dom::ReferrerPolicy referrerPolicy =
      dom::ReferrerInfo::ReferrerPolicyAttributeFromString(
          aHeader.mReferrerPolicy);

  nsCOMPtr<nsIReferrerInfo> referrerInfo =
      new dom::ReferrerInfo(aBaseURI, referrerPolicy);

  RefPtr<EarlyHintPreloader> earlyHintPreloader =
      RefPtr(new EarlyHintPreloader(uri));

  nsSecurityFlags securityFlags =
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT;

  NS_ENSURE_SUCCESS_VOID(earlyHintPreloader->OpenChannel(
      aTriggeringPrincipal, securityFlags, contentPolicyType, referrerInfo,
      aCookieJarSettings));

  DebugOnly<bool> result =
      aOngoingEarlyHints->Add(*hashKey, earlyHintPreloader);
  MOZ_ASSERT(result);
}

nsresult EarlyHintPreloader::OpenChannel(
    nsIPrincipal* aTriggeringPrincipal, nsSecurityFlags aSecurityFlags,
    nsContentPolicyType aContentPolicyType, nsIReferrerInfo* aReferrerInfo,
    nsICookieJarSettings* aCookieJarSettings) {
  MOZ_ASSERT(aContentPolicyType == nsContentPolicyType::TYPE_IMAGE);
  nsresult rv =
      NS_NewChannel(getter_AddRefs(mChannel), mURI, aTriggeringPrincipal,
                    aSecurityFlags, aContentPolicyType, aCookieJarSettings,
                    /* aPerformanceStorage */ nullptr,
                    /* aLoadGroup */ nullptr,
                    /* aCallbacks */ this, nsIRequest::LOAD_NORMAL);

  NS_ENSURE_SUCCESS(rv, rv);

  // configure HTTP specific stuff
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
  if (!httpChannel) {
    mChannel = nullptr;
    return NS_ERROR_ABORT;
  }
  DebugOnly<nsresult> success = httpChannel->SetReferrerInfo(aReferrerInfo);
  MOZ_ASSERT(NS_SUCCEEDED(success));
  success = httpChannel->SetRequestHeader("X-Moz"_ns, "early hint"_ns, false);
  MOZ_ASSERT(NS_SUCCEEDED(success));

  return mChannel->AsyncOpen(this);
}

nsresult EarlyHintPreloader::CancelChannel(nsresult aStatus) {
  // clear redirect channel in case this channel is cleared between the call of
  // EarlyHintPreloader::AsyncOnChannelRedirect and
  // EarlyHintPreloader::OnRedirectResult
  mRedirectChannel = nullptr;
  if (mChannel) {
    mChannel->Cancel(aStatus);
    mChannel = nullptr;
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// EarlyHintPreloader::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(EarlyHintPreloader, nsIRequestObserver, nsIStreamListener,
                  nsIChannelEventSink, nsIInterfaceRequestor,
                  nsIRedirectResultListener)

//-----------------------------------------------------------------------------
// EarlyHintPreloader::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
EarlyHintPreloader::OnStartRequest(nsIRequest* aRequest) {
  LOG(("EarlyHintPreloader::OnStartRequest\n"));

  nsCOMPtr<nsICacheInfoChannel> cacheInfoChannel = do_QueryInterface(aRequest);
  if (!cacheInfoChannel) {
    return NS_ERROR_ABORT;
  }

  // no need to prefetch an asset that is already in the cache
  bool fromCache;
  if (NS_SUCCEEDED(cacheInfoChannel->IsFromCache(&fromCache)) && fromCache) {
    LOG(("document is already in the cache; canceling prefetch\n"));
    return NS_BINDING_ABORTED;
  }
  return NS_OK;
}

NS_IMETHODIMP
EarlyHintPreloader::OnDataAvailable(nsIRequest* aRequest,
                                    nsIInputStream* aStream, uint64_t aOffset,
                                    uint32_t aCount) {
  uint32_t bytesRead = 0;
  nsresult rv =
      aStream->ReadSegments(NS_DiscardSegment, nullptr, aCount, &bytesRead);
  LOG(("prefetched %u bytes [offset=%" PRIu64 "]\n", bytesRead, aOffset));
  return rv;
}

NS_IMETHODIMP
EarlyHintPreloader::OnStopRequest(nsIRequest* aRequest, nsresult aStatus) {
  LOG(("EarlyHintPreloader::OnStopRequest\n"));
  mChannel = nullptr;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// EarlyHintPreloader::nsIChannelEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
EarlyHintPreloader::AsyncOnChannelRedirect(
    nsIChannel* aOldChannel, nsIChannel* aNewChannel, uint32_t aFlags,
    nsIAsyncVerifyRedirectCallback* callback) {
  nsCOMPtr<nsIURI> newURI;
  nsresult rv = NS_GetFinalChannelURI(aNewChannel, getter_AddRefs(newURI));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aNewChannel->GetURI(getter_AddRefs(newURI));
  if (NS_FAILED(rv)) {
    callback->OnRedirectVerifyCallback(rv);
    return NS_OK;
  }

  // abort the request if redirecting to cross origin resource, the
  // fromPrivateWindow is only read when reportError is enabled, so setting both
  // to false is safe.
  if (NS_FAILED(nsContentUtils::GetSecurityManager()->CheckSameOriginURI(
          mURI, newURI, /* reportError */ false,
          /* fromPrivateWindow */ false))) {
    callback->OnRedirectVerifyCallback(NS_ERROR_ABORT);
    return NS_OK;
  }

  // HTTP request headers are not automatically forwarded to the new channel.
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aNewChannel);
  NS_ENSURE_STATE(httpChannel);

  rv = httpChannel->SetRequestHeader("X-Moz"_ns, "early hint"_ns, false);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // Assign to mChannel after we get notification about success of the
  // redirect in OnRedirectResult.
  mRedirectChannel = aNewChannel;

  callback->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// EarlyHintPreloader::nsIRedirectResultListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
EarlyHintPreloader::OnRedirectResult(bool aProceeding) {
  if (aProceeding && mRedirectChannel) {
    mChannel = mRedirectChannel;
  }

  mRedirectChannel = nullptr;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// EarlyHintPreloader::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
EarlyHintPreloader::GetInterface(const nsIID& aIID, void** aResult) {
  if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
    NS_ADDREF_THIS();
    *aResult = static_cast<nsIChannelEventSink*>(this);
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIRedirectResultListener))) {
    NS_ADDREF_THIS();
    *aResult = static_cast<nsIRedirectResultListener*>(this);
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

}  // namespace mozilla::net
