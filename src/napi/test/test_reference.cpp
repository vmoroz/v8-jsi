// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_reference_init
#include "js-native-api/test_reference/test.js.h"
#include "js-native-api/test_reference/test_reference.c"

using namespace napitest;

TEST_P(NapiTestBase, test_reference) {
  auto testContext = NapiTestContext(this, env);
  AddNativeModule(
      "./build/x86/test_reference",
      [](napi_env env, napi_value exports) { return Init(env, exports); });
  RunTestScript(test_reference_test_js);
}
