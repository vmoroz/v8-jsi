// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_array_init
#include "js-native-api/test_array/test.js.h"
#include "js-native-api/test_array/test_array.c"

using namespace napitest;

TEST_P(NapiTestBase, test_array) {
  auto testContext = NapiTestContext(this);
  AddNativeModule(
      "./build/x86/test_array",
      [](napi_env env, napi_value exports) { return Init(env, exports); });
  RunTestScript(test_array_test_js);
}
