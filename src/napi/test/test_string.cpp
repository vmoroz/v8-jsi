// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_string_init
#include "js-native-api/test_string/test.js.h"
#include "js-native-api/test_string/test_string.c"

using namespace napitest;

TEST_P(NapiTestBase, test_string) {
  AddNativeModule(
      "./build/x86/test_string",
      [](napi_env env, napi_value exports) { Init(env, exports); });

  RunTestScript(test_string_test_js);
}