// Note: the file is largely imported directly from WebRTC upstream, so
// comments may not completely apply to Mozilla's usage.
//
// Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
// Borrowed from
// https://code.google.com/p/gperftools/source/browse/src/base/thread_annotations.h
// but adapted for clang attributes instead of the gcc.
//
// This header file contains the macro definitions for thread safety
// annotations that allow the developers to document the locking policies
// of their multi-threaded code. The annotations can also help program
// analysis tools to identify potential thread safety issues.

#ifndef mozilla_ThreadSafety_h
#define mozilla_ThreadSafety_h
#include "mozilla/Attributes.h"

#if defined(__clang__) && (__clang_major__ >= 8) && !defined(SWIG)
#  define THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
// Allow for localized suppression of thread-safety warnings; finer-grained
// than NO_THREAD_SAFETY_ANALYSIS
#  define PUSH_IGNORE_THREAD_SAFETY \
    _Pragma("GCC diagnostic push")  \
        _Pragma("GCC diagnostic ignored \"-Wthread-safety\"")
#  define POP_THREAD_SAFETY _Pragma("GCC diagnostic pop")

#else
#  define THREAD_ANNOTATION_ATTRIBUTE__(x)  // no-op
#  define PUSH_IGNORE_THREAD_SAFETY
#  define POP_THREAD_SAFETY
#endif

// Document if a shared variable/field needs to be protected by a lock.
// GUARDED_BY allows the user to specify a particular lock that should be
// held when accessing the annotated variable, while GUARDED_VAR only
// indicates a shared variable should be guarded (by any lock). GUARDED_VAR
// is primarily used when the client cannot express the name of the lock.
#define GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))
#define GUARDED_VAR THREAD_ANNOTATION_ATTRIBUTE__(guarded_var)

// Document if the memory location pointed to by a pointer should be guarded
// by a lock when dereferencing the pointer. Similar to GUARDED_VAR,
// PT_GUARDED_VAR is primarily used when the client cannot express the name
// of the lock. Note that a pointer variable to a shared memory location
// could itself be a shared variable. For example, if a shared global pointer
// q, which is guarded by mu1, points to a shared memory location that is
// guarded by mu2, q should be annotated as follows:
//     int *q GUARDED_BY(mu1) PT_GUARDED_BY(mu2);
#define PT_GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))
#define PT_GUARDED_VAR THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_var)

// Document the acquisition order between locks that can be held
// simultaneously by a thread. For any two locks that need to be annotated
// to establish an acquisition order, only one of them needs the annotation.
// (i.e. You don't have to annotate both locks with both ACQUIRED_AFTER
// and ACQUIRED_BEFORE.)
#define ACQUIRED_AFTER(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))
#define ACQUIRED_BEFORE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))

// The following three annotations document the lock requirements for
// functions/methods.

// Document if a function expects certain locks to be held before it is called
#define REQUIRES(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(exclusive_locks_required(__VA_ARGS__))

#define REQUIRES_SHARED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(shared_locks_required(__VA_ARGS__))

// Document the locks acquired in the body of the function. These locks
// cannot be held when calling this function (as google3's Mutex locks are
// non-reentrant).
#define EXCLUDES(x) THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(x))

// Document the lock the annotated function returns without acquiring it.
#define RETURN_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

// Document if a class/type is a lockable type (such as the Mutex class).
#define CAPABILITY THREAD_ANNOTATION_ATTRIBUTE__(lockable)

// Document if a class is a scoped lockable type (such as the MutexLock class).
#define SCOPED_CAPABILITY THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

// The following annotations specify lock and unlock primitives.
#define CAPABILITY_ACQUIRE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(exclusive_lock_function(__VA_ARGS__))

#define EXCLUSIVE_RELEASE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))

#define ACQUIRE_SHARED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(shared_lock_function(__VA_ARGS__))

#define TRY_ACQUIRE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(exclusive_trylock_function(__VA_ARGS__))

#define SHARED_TRYLOCK_FUNCTION(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(shared_trylock_function(__VA_ARGS__))

#define CAPABILITY_RELEASE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(unlock_function(__VA_ARGS__))

// An escape hatch for thread safety analysis to ignore the annotated function.
#define NO_THREAD_SAFETY_ANALYSIS \
  THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

// Newer capabilities
#define ASSERT_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))

#define ASSERT_SHARED_CAPABILITY(x) \
  THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(x))

// Additions from current clang assertions.
// Note: new-style definitions, since these didn't exist in the old style
#define RELEASE_SHARED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))

#define RELEASE_GENERIC(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(release_generic_capability(__VA_ARGS__))

// Mozilla additions:

// AutoUnlock is supported by clang currently, but oddly you must use
// EXCLUSIVE_RELEASE() for both the RAII constructor *and* the destructor.
// This hides the ugliness until they fix it upstream.
#define SCOPED_UNLOCK_RELEASE(...) EXCLUSIVE_RELEASE(__VA_ARGS__)
#define SCOPED_UNLOCK_REACQUIRE(...) EXCLUSIVE_RELEASE(__VA_ARGS__)

#endif /* mozilla_ThreadSafety_h */
