/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_IPC_MFMEDIAENGINEPARENT_H_
#define DOM_MEDIA_IPC_MFMEDIAENGINEPARENT_H_

#include "MediaInfo.h"
#include "PlatformDecoderModule.h"
#include "mozilla/PMFMediaEngineParent.h"

namespace mozilla {

class MFMediaEngineStream;
class RemoteDecoderManagerParent;

/**
 * MFMediaEngineParent is a wrapper class for a MediaEngine in the RDD process.
 * It's responsible to create the media engine and its related classes, such as
 * a custom media source, media engine extension, media engine notify...e.t.c
 * It communicates with MFMediaEngineChild in the content process to receive
 * commands and direct them to the media engine.
 * https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/nn-mfmediaengine-imfmediaengine
 */
class MFMediaEngineParent final : public PMFMediaEngineParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MFMediaEngineParent);
  explicit MFMediaEngineParent(RemoteDecoderManagerParent* aManager);

  using TrackType = TrackInfo::TrackType;

  static MFMediaEngineParent* GetMediaEngineById(uint64_t aId);

  MFMediaEngineStream* GetMediaEngineStream(TrackType aType,
                                            const CreateDecoderParams& aParam);

  uint64_t Id() const { return mMediaEngineId; }

  // Methods for PMFMediaEngineParent
  mozilla::ipc::IPCResult RecvInitMediaEngine(
      const MediaEngineInfoIPDL& aInfo, InitMediaEngineResolver&& aResolver);
  mozilla::ipc::IPCResult RecvNotifyMediaInfo(const MediaInfoIPDL& aInfo);
  mozilla::ipc::IPCResult RecvPlay();
  mozilla::ipc::IPCResult RecvPause();
  mozilla::ipc::IPCResult RecvSeek(double aTargetTimeInSecond);
  mozilla::ipc::IPCResult RecvSetVolume(double aVolume);
  mozilla::ipc::IPCResult RecvSetPlaybackRate(double aPlaybackRate);
  mozilla::ipc::IPCResult RecvSetLooping(bool aLooping);
  mozilla::ipc::IPCResult RecvNotifyEndOfStream(TrackInfo::TrackType aType);
  mozilla::ipc::IPCResult RecvShutdown();

  void Destroy();

 private:
  ~MFMediaEngineParent();

  void AssertOnManagerThread() const;

  // This generates unique id for each MFMediaEngineParent instance, and it
  // would be increased monotonically.
  static inline uint64_t sMediaEngineIdx = 0;

  const uint64_t mMediaEngineId;

  // The life cycle of this class is determined by the actor in the content
  // process, we would hold a reference until the content actor asks us to
  // destroy.
  RefPtr<MFMediaEngineParent> mIPDLSelfRef;

  const RefPtr<RemoteDecoderManagerParent> mManager;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_IPC_MFMEDIAENGINEPARENT_H_
