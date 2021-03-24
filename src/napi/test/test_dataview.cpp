// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_dataview_init
#include "js-native-api/test_dataview/test.js.h"
#include "js-native-api/test_dataview/test_dataview.c"

using namespace napitest;

TEST_P(NapiTestBase, test_dataview) {
  AddNativeModule(
      "./build/x86/test_dataview",
      [](napi_env env, napi_value exports) { return Init(env, exports); });

  RunTestScript(test_dataview_test_js);
}
