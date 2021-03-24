// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_instance_data_init
#include "js-native-api/test_instance_data/test.js.h"
#include "js-native-api/test_instance_data/test_instance_data.c"

using namespace napitest;

//TODO: [vmoroz] make it work
// TEST_P(NapiTestBase, test_instance_data) {
//   AddNativeModule(
//       "./build/x86/test_instance_data",
//       [](napi_env env, napi_value exports) { return Init(env, exports); });

//   RunTestScript(test_instance_data_test_js);
// }
