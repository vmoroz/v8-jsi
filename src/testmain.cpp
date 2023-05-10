// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <gtest/gtest.h>
#include <jsi/jsi.h>
#include "jsi/test/testlib.h"
#include "node-api-jsi/NodeApiJsiRuntime.h"
#include "node-api-jsi/V8Api.h"
#include "public/ScriptStore.h"
#include "public/v8_api.h"
#include "public/V8JsiRuntime.h"

using namespace Microsoft::NodeApiJsi;

namespace facebook::jsi {

std::vector<facebook::jsi::RuntimeFactory> runtimeGenerators() {
  return {[]() -> std::unique_ptr<facebook::jsi::Runtime> {
            v8runtime::V8RuntimeArgs args;
            return v8runtime::makeV8Runtime(std::move(args));
          },
          []() -> std::unique_ptr<facebook::jsi::Runtime> {
            V8Api* v8Api = V8Api::fromLib();
            V8Api::setCurrent(v8Api);

            v8_config config{};
            v8_runtime runtime{};
            napi_env env{};
            v8Api->v8_create_config(&config);
            v8Api->v8_create_runtime(config, &runtime);
            v8Api->v8_get_node_api_env(runtime, &env);

            return makeNodeApiJsiRuntime(env, v8Api, [runtime]() {
              V8Api::current()->v8_delete_runtime(runtime);
            });
          }};
}

} // namespace facebook

using namespace facebook::jsi;

TEST(Basic, CreateManyRuntimes) {
  for (size_t i = 0; i < 100; i++) {
    v8runtime::V8RuntimeArgs args;
    args.flags.enableInspector = true;
    auto runtime = v8runtime::makeV8Runtime(std::move(args));

    runtime->evaluateJavaScript(std::make_unique<facebook::jsi::StringBuffer>("x = 1"), "");
    EXPECT_EQ(runtime->global().getProperty(*runtime, "x").getNumber(), 1);
  }
}

TEST(Basic, MultiThreadIsolate) {
  v8runtime::V8RuntimeArgs args;
  args.flags.enableMultiThread = true;
  auto runtime = v8runtime::makeV8Runtime(std::move(args));

  runtime->evaluateJavaScript(
      std::make_unique<facebook::jsi::StringBuffer>("x = {1:2, '3':4, 5:'six', 'seven':['eight', 'nine']}"), "");

  Runtime &rt = *runtime;

  Object x = rt.global().getPropertyAsObject(rt, "x");
  EXPECT_EQ(x.getProperty(rt, "1").getNumber(), 2);

  std::vector<std::thread> vec_thr;
  for (size_t i = 0; i < 10; ++i) {
    vec_thr.push_back(std::thread([&]() {
      EXPECT_EQ(x.getProperty(rt, PropNameID::forAscii(rt, "1")).getNumber(), 2);
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      EXPECT_EQ(x.getProperty(rt, "3").getNumber(), 4);
    }));
  }

  for (size_t i = 0; i < vec_thr.size(); ++i) {
    vec_thr.at(i).join();
  }
}
//TODO: implement
#if 0
TEST(Basic, MultiThreadIsolateNApi) {
  V8Api* v8Api = V8Api::fromLib();
  V8Api::setCurrent(v8Api);

  v8_config config{};
  v8_runtime runtime{};
  napi_env env{};
  v8Api->v8_create_config(&config);
  // settings.flags.enable_gc_api = true;
  // settings.flags.enable_multi_thread = true;
  v8Api->v8_create_runtime(config, &runtime);
  v8Api->v8_get_node_api_env(runtime, &env);

  napi_ext_env_settings settings{};
  settings.this_size = sizeof(napi_ext_env_settings);
  settings.flags.enable_gc_api = true;
  settings.flags.enable_multi_thread = true;
  napi_env env{};
  napi_ext_create_env(&settings, &env);
  auto runtime = Microsoft::JSI::MakeNapiJsiRuntime(env);

  runtime->evaluateJavaScript(
      std::make_unique<facebook::jsi::StringBuffer>("x = {1:2, '3':4, 5:'six', 'seven':['eight', 'nine']}"), "");

  Runtime &rt = *runtime;

  Object x = rt.global().getPropertyAsObject(rt, "x");
  EXPECT_EQ(x.getProperty(rt, "1").getNumber(), 2);

  std::vector<std::thread> vec_thr;
  for (size_t i = 0; i < 10; ++i) {
    vec_thr.push_back(std::thread([&]() {
      EXPECT_EQ(x.getProperty(rt, PropNameID::forAscii(rt, "1")).getNumber(), 2);
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      EXPECT_EQ(x.getProperty(rt, "3").getNumber(), 4);
    }));
  }

  for (size_t i = 0; i < vec_thr.size(); ++i) {
    vec_thr.at(i).join();
  }
}
#endif
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
