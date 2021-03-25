// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_handle_scope_init
#include "js-native-api/test_handle_scope/test.js.h"
#include "js-native-api/test_handle_scope/test_handle_scope.c"

using namespace napitest;

TEST_P(NapiTestBase, test_handle_scope) {
  AddNativeModule(
      "./build/x86/test_handle_scope",
      [](napi_env env, napi_value exports) { return Init(env, exports); });

  RunTestScript(test_handle_scope_test_js);
}