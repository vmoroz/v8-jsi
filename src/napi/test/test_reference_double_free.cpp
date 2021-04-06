// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_reference_double_free_init
#include "js-native-api/test_reference_double_free/test.js.h"
#define delete delete1
#include "js-native-api/test_reference_double_free/test_reference_double_free.c"
#undef delete

using namespace napitest;

TEST_P(NapiTestBase, test_reference_double_free) {
  ExecuteNapi([](NapiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_reference_double_free",
        [](napi_env env, napi_value exports) { return Init(env, exports); });
    testContext->RunTestScript(test_reference_double_free_test_js);
  });
}
