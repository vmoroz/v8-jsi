// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_constructor_init
#include "js-native-api/test_constructor/test.js.h"
#include "js-native-api/test_constructor/test2.js.h"
#include "js-native-api/test_constructor/test_constructor.c"

using namespace napitest;

TEST_P(NapiTestBase, test_constructor) {
  AddNativeModule(
      "./build/x86/test_constructor",
      [](napi_env env, napi_value exports) { return Init(env, exports); });

  RunTestScript(test_constructor_test_js);
}

TEST_P(NapiTestBase, test_constructor2) {
  AddNativeModule(
      "./build/x86/test_constructor",
      [](napi_env env, napi_value exports) { return Init(env, exports); });

  RunTestScript(test_constructor_test2_js);
}
