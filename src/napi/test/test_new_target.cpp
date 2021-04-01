// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_new_target_init
#include "js-native-api/test_new_target/binding.c"
#include "js-native-api/test_new_target/test.js.h"

using namespace napitest;

TEST_P(NapiTestBase, test_new_target) {
  auto testContext = NapiTestContext(this);
  AddNativeModule("./build/x86/binding", [](napi_env env, napi_value exports) {
    return Init(env, exports);
  });
  RunTestScript(test_new_target_test_js);
}
