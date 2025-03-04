/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_URIUtils_h
#define mozilla_ipc_URIUtils_h

#include "mozilla/ipc/URIParams.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"

namespace mozilla {
namespace ipc {

void
SerializeURI(nsIURI* aURI,
             URIParams& aParams);

void
SerializeURI(nsIURI* aURI,
             OptionalURIParams& aParams);

already_AddRefed<nsIURI>
DeserializeURI(const URIParams& aParams);

already_AddRefed<nsIURI>
DeserializeURI(const OptionalURIParams& aParams);

template<>
struct IPDLParamTraits<nsIURI>
{
  static void Write(IPC::Message* aMsg, IProtocol* aActor, nsIURI* aParam)
  {
    OptionalURIParams params;
    SerializeURI(aParam, params);
    WriteIPDLParam(aMsg, aActor, params);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, RefPtr<nsIURI>* aResult)
  {
    OptionalURIParams params;
    if (!ReadIPDLParam(aMsg, aIter, aActor, &params)) {
      return false;
    }
    *aResult = DeserializeURI(params);
    return true;
  }
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_URIUtils_h
