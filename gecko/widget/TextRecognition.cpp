/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextRecognition.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/ContentChild.h"
#include "nsTextNode.h"
#include "imgIContainer.h"

using namespace mozilla::dom;

namespace mozilla::widget {

auto TextRecognition::FindText(imgIContainer& aImage) -> RefPtr<NativePromise> {
  // TODO: Maybe decode async.
  RefPtr<gfx::SourceSurface> surface = aImage.GetFrame(
      imgIContainer::FRAME_CURRENT,
      imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY);
  if (NS_WARN_IF(!surface)) {
    return NativePromise::CreateAndReject("Failed to get surface"_ns, __func__);
  }
  RefPtr<gfx::DataSourceSurface> dataSurface = surface->GetDataSurface();
  if (NS_WARN_IF(!dataSurface)) {
    return NativePromise::CreateAndReject("Failed to get data surface"_ns,
                                          __func__);
  }
  return FindText(*dataSurface);
}

auto TextRecognition::FindText(gfx::DataSourceSurface& aSurface)
    -> RefPtr<NativePromise> {
  if (XRE_IsContentProcess()) {
    auto* contentChild = ContentChild::GetSingleton();
    auto image = nsContentUtils::SurfaceToIPCImage(aSurface, contentChild);
    if (!image) {
      return NativePromise::CreateAndReject("Failed to share data surface"_ns,
                                            __func__);
    }
    auto promise = MakeRefPtr<NativePromise::Private>(__func__);
    contentChild->SendFindImageText(*image)->Then(
        GetCurrentSerialEventTarget(), __func__,
        [promise](TextRecognitionResultOrError&& aResultOrError) {
          switch (aResultOrError.type()) {
            case TextRecognitionResultOrError::Type::TTextRecognitionResult:
              promise->Resolve(
                  std::move(aResultOrError.get_TextRecognitionResult()),
                  __func__);
              break;
            case TextRecognitionResultOrError::Type::TnsCString:
              promise->Reject(std::move(aResultOrError.get_nsCString()),
                              __func__);
              break;
            default:
              MOZ_ASSERT_UNREACHABLE("Unknown result?");
              promise->Reject("Unknown error"_ns, __func__);
              break;
          }
        },
        [promise](mozilla::ipc::ResponseRejectReason) {
          promise->Reject("IPC rejection"_ns, __func__);
        });
    return promise;
  }
  return DoFindText(aSurface);
}

void TextRecognition::FillShadow(ShadowRoot& aShadow,
                                 const TextRecognitionResult& aResult) {
  auto& doc = *aShadow.OwnerDoc();
  RefPtr<Element> div = doc.CreateHTMLElement(nsGkAtoms::div);
  for (const auto& quad : aResult.quads()) {
    RefPtr<Element> span = doc.CreateHTMLElement(nsGkAtoms::span);
    // TODO: We probably want to position them here and so on. For now, expose
    // the data as attributes so that it's easy to play with the returned values
    // in JS.
    {
      nsAutoString points;
      for (auto& point : quad.points()) {
        points.AppendFloat(point.x);
        points.Append(u',');
        points.AppendFloat(point.y);
        points.Append(u',');
      }
      points.Trim(",");
      span->SetAttribute(u"data-points"_ns, points, IgnoreErrors());
      nsAutoString confidence;
      confidence.AppendFloat(quad.confidence());
      span->SetAttribute(u"data-confidence"_ns, confidence, IgnoreErrors());
    }

    {
      RefPtr<nsTextNode> text = doc.CreateTextNode(quad.string());
      span->AppendChildTo(text, true, IgnoreErrors());
    }
    div->AppendChildTo(span, true, IgnoreErrors());
  }
  aShadow.AppendChildTo(div, true, IgnoreErrors());
}

auto TextRecognition::DoFindText(gfx::DataSourceSurface&)
    -> RefPtr<NativePromise> {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess(),
                     "This should only run in the parent process");
  return NativePromise::CreateAndReject("Text recognition not available"_ns,
                                        __func__);
}

}  // namespace mozilla::widget
