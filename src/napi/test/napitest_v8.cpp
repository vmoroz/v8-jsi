// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <gtest/gtest.h>
#include "js_engine_api.h"
#include "napitest.h"

namespace napitest {

struct V8NapiEnvProvider : NapiEnvProvider {
  V8NapiEnvProvider() {}

  napi_env CreateEnv() override {
    jse_create_env(jse_env_attribute_none, &m_env);
    return m_env;
  }

  void DeleteEnv() override {
    jse_delete_env(m_env);
  }

 private:
  napi_env m_env;
};

std::vector<std::shared_ptr<NapiEnvProvider>> NapiEnvProviders() {
  return {std::make_shared<V8NapiEnvProvider>()};
}

} // namespace napitest
