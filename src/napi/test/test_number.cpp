// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_number_init
#include "js-native-api/test_number/test.js.h"
#include "js-native-api/test_number/test_number.c"

using namespace napitest;

TEST_P(NapiTestBase, test_number) {
  AddNativeModule(
      "./build/x86/test_number",
      [](napi_env env, napi_value exports) { return Init(env, exports); });

  RunTestScript(test_number_test_js);
}
