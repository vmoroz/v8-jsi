// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <cstdarg>
#include <thread>
#include "napitestbase.h"

#define Init test_general_init
#define printf test_printf
static int test_printf(const char *format, ...);

#include "js-native-api/test_general/test.js.h"
#include "js-native-api/test_general/testEnvCleanup.js.h"
#include "js-native-api/test_general/testFinalizer.js.h"
#include "js-native-api/test_general/testGlobals.js.h"
#include "js-native-api/test_general/testInstanceOf.js.h"
#include "js-native-api/test_general/testNapiRun.js.h"
#include "js-native-api/test_general/testNapiStatus.js.h"
#include "js-native-api/test_general/test_general.c"

static std::string s_output;

static int test_printf(const char *format, ...) {
  va_list args1;
  va_start(args1, format);
  va_list args2;
  va_copy(args2, args1);
  std::vector<char> buf(1 + std::vsnprintf(nullptr, 0, format, args1));
  va_end(args1);
  std::vsnprintf(buf.data(), buf.size(), format, args2);
  va_end(args2);
  s_output += buf.data();
  return buf.size();
}

using namespace napitest;
#if 0
TEST_P(NapiTestBase, test_general) {
  auto testContext = NapiTestContext(this, env);
  AddNativeModule(
      "./build/x86/test_general",
      [](napi_env env, napi_value exports) { return Init(env, exports); });
  RunTestScript(test_general_test_js);
}

TEST_P(NapiTestBase, test_general_NapiStatus) {
  auto testContext = NapiTestContext(this, env);
  AddNativeModule(
      "./build/x86/test_general",
      [](napi_env env, napi_value exports) { return Init(env, exports); });
  RunTestScript(test_general_testNapiStatus_js);
}

TEST_P(NapiTestBase, test_general_NapiRun) {
  auto testContext = NapiTestContext(this, env);
  AddNativeModule(
      "./build/x86/test_general",
      [](napi_env env, napi_value exports) { return Init(env, exports); });
  RunTestScript(test_general_testNapiRun_js);
}

// TODO: [vmoroz] make it work
// TEST_P(NapiTestBase, test_general_InstanceOf) {
// auto testContext = NapiTestContext(this, env);
//   AddNativeModule(
//       "./build/x86/test_general",
//       [](napi_env env, napi_value exports) { return Init(env, exports); });
//   RunTestScript(test_general_testInstanceOf_js);
// }

// TEST_P(NapiTestBase, test_general_Globals) {
//   auto testContext = NapiTestContext(this, env);
//   AddNativeModule(
//       "./build/x86/test_general",
//       [](napi_env env, napi_value exports) { return Init(env, exports); });
//   RunTestScript(test_general_testGlobals_js);
// }
#endif
// // TODO: [vmoroz] make it work
// // TEST_P(NapiTestBase, test_general_Finalizer) {
// // auto testContext = NapiTestContext(this, env);
// //   AddNativeModule(
// //       "./build/x86/test_general",
// //       [](napi_env env, napi_value exports) { return Init(env, exports);
// });
// //   RunTestScript(test_general_testFinalizer_js);
// // }

TEST_P(NapiTestBase, test_general_EnvCleanup) {
  s_output = "";
  auto spawnSyncCallback = [](napi_env env,
                              napi_callback_info /*info*/) -> napi_value {
    auto childThread = std::thread([]() {
      ExecuteNapi([](NapiTestContext *testContext, napi_env env) {
        testContext->AddNativeModule(
            "./build/x86/test_general", [](napi_env env, napi_value exports) {
              return Init(env, exports);
            });

        testContext->RunScript(R"(
          process = { argv:['', '', 'child'] };
        )");

        testContext->RunTestScript(test_general_testEnvCleanup_js);
      });
    });
    childThread.join();

    napi_value child{}, strValue{}, statusValue{};
    THROW_IF_NOT_OK(napi_create_object(env, &child));
    THROW_IF_NOT_OK(napi_create_string_utf8(
        env, s_output.c_str(), s_output.length(), &strValue));
    THROW_IF_NOT_OK(napi_set_named_property(env, child, "stdout", strValue));
    THROW_IF_NOT_OK(napi_create_int32(env, 0, &statusValue));
    THROW_IF_NOT_OK(napi_set_named_property(env, child, "status", statusValue));
    return child;
  };

  ExecuteNapi([&](NapiTestContext *testContext, napi_env env) {
    testContext->AddNativeModule(
        "./build/x86/test_general",
        [](napi_env env, napi_value exports) { return Init(env, exports); });

    testContext->RunScript(R"(
      process = { argv:[] };
      __filename = '';
    )");

    testContext->AddNativeModule(
        "child_process", [&](napi_env env, napi_value exports) {
          napi_value spawnSync{};
          THROW_IF_NOT_OK(napi_create_function(
              env,
              "spawnSync",
              NAPI_AUTO_LENGTH,
              spawnSyncCallback,
              nullptr,
              &spawnSync));
          THROW_IF_NOT_OK(
              napi_set_named_property(env, exports, "spawnSync", spawnSync));
          return exports;
        });

    testContext->RunTestScript(test_general_testEnvCleanup_js);
  });
}
