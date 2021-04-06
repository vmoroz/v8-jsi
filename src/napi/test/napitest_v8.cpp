// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <gtest/gtest.h>
#include "napitest.h"

namespace napitest {

struct V8NapiEnvProvider : NapiEnvProvider {
  V8NapiEnvProvider() {}

  napi_env CreateEnv() override {
    napi_env env{};
    napi_ext_create_env(napi_ext_env_attribute_enable_gc_api, &env);
    return env;
  }

  void DeleteEnv(napi_env env) override {
    napi_ext_delete_env(env);
  }
};

std::vector<std::shared_ptr<NapiEnvProvider>> NapiEnvProviders() {
  return {std::make_shared<V8NapiEnvProvider>()};
}

} // namespace napitest
