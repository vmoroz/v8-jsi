// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_general_init
#include "js-native-api/test_general/test.js.h"
#include "js-native-api/test_general/test_general.c"

using namespace napitest;

//TODO: [vmoroz] Add more scripts
TEST_P(NapiTestBase, test_general) {
  AddNativeModule(
      "./build/x86/test_general",
      [](napi_env env, napi_value exports) { return Init(env, exports); });

  RunTestScript(test_general_test_js);
}
