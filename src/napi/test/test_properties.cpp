// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_properties_init
#include "js-native-api/test_properties/test.js.h"
#include "js-native-api/test_properties/test_properties.c.h"

using namespace napitest;

TEST_P(NapiTestBase, test_properties) {
  AddNativeModule(
      "./build/x86/test_properties",
      [](napi_env env, napi_value exports) { Init(env, exports); });

  RunTestScript(test_properties_test_js);
}
