// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <gtest/gtest.h>
#include "napitest.h"

namespace napitest {

std::vector<NapiEnvFactory> NapiEnvFactories() {
  return {NapiEnvFactory{[]() -> napi_env { return new napi_env__(); }}};
}

} // namespace napitest
