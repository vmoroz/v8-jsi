// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_bigint_init
#include "js-native-api/test_bigint/test.js.h"
#include "js-native-api/test_bigint/test_bigint.c"

using namespace napitest;

TEST_P(NapiTestBase, test_bigint) {
  AddNativeModule(
      "./build/x86/test_bigint",
      [](napi_env env, napi_value exports) { return Init(env, exports); });

  RunTestScript(test_bigint_test_js);
}
