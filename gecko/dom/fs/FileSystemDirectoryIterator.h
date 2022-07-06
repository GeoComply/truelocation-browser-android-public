/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMDIRECTORYITERATOR_H_
#define DOM_FS_FILESYSTEMDIRECTORYITERATOR_H_

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

class Promise;

class FileSystemDirectoryIterator : public nsISupports, public nsWrapperCache {
 public:
  explicit FileSystemDirectoryIterator(nsIGlobalObject* aGlobal);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(FileSystemDirectoryIterator)

  // WebIDL Boilerplate
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  already_AddRefed<Promise> Next(ErrorResult& aError);

 protected:
  virtual ~FileSystemDirectoryIterator() = default;

  nsCOMPtr<nsIGlobalObject> mGlobal;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_FILESYSTEMDIRECTORYITERATOR_H_
