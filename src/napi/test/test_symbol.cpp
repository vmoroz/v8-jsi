// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_symbol_init
#include "js-native-api/test_symbol/test1.js.h"
#include "js-native-api/test_symbol/test2.js.h"
#include "js-native-api/test_symbol/test3.js.h"
#include "js-native-api/test_symbol/test_symbol.c"

using namespace napitest;

TEST_P(NapiTestBase, test_symbol1) {
  auto testContext = NapiTestContext(this, env);
  AddNativeModule(
      "./build/x86/test_symbol",
      [](napi_env env, napi_value exports) { return Init(env, exports); });
  RunTestScript(test_symbol_test1_js);
}

TEST_P(NapiTestBase, test_symbol2) {
  auto testContext = NapiTestContext(this, env);
  AddNativeModule(
      "./build/x86/test_symbol",
      [](napi_env env, napi_value exports) { return Init(env, exports); });
  RunTestScript(test_symbol_test2_js);
}

TEST_P(NapiTestBase, test_symbol3) {
  auto testContext = NapiTestContext(this, env);
  AddNativeModule(
      "./build/x86/test_symbol",
      [](napi_env env, napi_value exports) { return Init(env, exports); });
  RunTestScript(test_symbol_test3_js);
}
