// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_conversions_init
#include "js-native-api/test_conversions/test.js.h"
#include "js-native-api/test_conversions/test_conversions.c"
#include "js-native-api/test_conversions/test_null.c"

using namespace napitest;

TEST_P(NapiTestBase, test_conversions) {
  AddNativeModule(
      "./build/x86/test_conversions",
      [](napi_env env, napi_value exports) { return Init(env, exports); });

  RunTestScript(test_conversions_test_js);
}