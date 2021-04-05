// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_object_init
#include "js-native-api/test_object/test.js.h"
#include "js-native-api/test_object/test_null.c"
#include "js-native-api/test_object/test_null.js.h"
#include "js-native-api/test_object/test_object.c"

using namespace napitest;

TEST_P(NapiTestBase, test_object) {
  auto testContext = NapiTestContext(this, env);
  AddNativeModule(
      "./build/x86/test_object",
      [](napi_env env, napi_value exports) { return Init(env, exports); });
  RunTestScript(test_object_test_js);
}

TEST_P(NapiTestBase, test_object_null) {
  auto testContext = NapiTestContext(this, env);
  AddNativeModule(
      "./build/x86/test_object",
      [](napi_env env, napi_value exports) { return Init(env, exports); });
  RunTestScript(test_object_test_null_js);
}
