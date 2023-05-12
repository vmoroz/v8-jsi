// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "V8Api.h"

namespace Microsoft::NodeApiJsi {

namespace {

struct V8ApiNames {
#define V8_FUNC(func) static constexpr const char func[] = #func;
#include "V8ApiFunctions.inc"
};

} // namespace

thread_local V8Api *V8Api::current_{};

V8Api::V8Api(IFuncResolver *funcResolver)
    : NodeApi(funcResolver)
#define V8_FUNC(func) \
  , func(&ApiFuncResolver<NodeApi, decltype(::func) *, V8ApiNames::func, offsetof(V8Api, func)>::stub)
#include "V8ApiFunctions.inc"
{
}

V8Api *V8Api::fromLib() {
  static LibFuncResolver funcResolver("v8jsi");
  static V8Api *libV8Api = new V8Api(&funcResolver);
  return libV8Api;
}

} // namespace Microsoft::NodeApiJsi
