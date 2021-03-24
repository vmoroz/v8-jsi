// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"

#define Init test_new_target_init
#include "js-native-api/test_new_target/test.js.h"
#include "js-native-api/test_new_target/binding.c"

using namespace napitest;

//TODO: [vmoroz] make it work
// TEST_P(NapiTestBase, test_new_target) {
//   AddNativeModule(
//       "./build/x86/test_new_target",
//       [](napi_env env, napi_value exports) { return Init(env, exports); });

//   RunTestScript(test_new_target_test_js);
// }
