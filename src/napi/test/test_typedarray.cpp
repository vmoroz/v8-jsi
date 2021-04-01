// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_typedarray_init
#include "js-native-api/test_typedarray/test.js.h"
#include "js-native-api/test_typedarray/test_typedarray.c"

using namespace napitest;

TEST_P(NapiTestBase, test_typedarray) {
  auto testContext = NapiTestContext(this);

  AddNativeModule(
      "./build/x86/test_typedarray",
      [](napi_env env, napi_value exports) { return Init(env, exports); });

  RunTestScript(test_typedarray_test_js);
}
