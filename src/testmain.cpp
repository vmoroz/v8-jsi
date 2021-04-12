// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <gtest/gtest.h>
#include "public/NapiJsiRuntime.h"
#include "public/V8JsiRuntime.h"
#include "public/ScriptStore.h"
#include "jsi/test/testlib.h"

namespace facebook {
namespace jsi {

std::vector<facebook::jsi::RuntimeFactory> runtimeGenerators() {
  return {[]() -> std::unique_ptr<facebook::jsi::Runtime> {
    v8runtime::V8RuntimeArgs args;

    return v8runtime::makeV8Runtime(std::move(args));
  }, []() -> std::unique_ptr<facebook::jsi::Runtime> {
    napi_env env{};
    napi_ext_create_env(napi_ext_env_attribute_enable_gc_api, &env);
    napi_ext_env_scope envScope{};
    napi_ext_open_env_scope(env, &envScope);
    napi_handle_scope handleScope{};
    napi_open_handle_scope(env, &handleScope);
    return napijsi::MakeNapiJsiRuntime(env);
  }};
}

}}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
