// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <gtest/gtest.h>
#include "../v8_api.h"
#include "napitest.h"

namespace napitest {

struct V8NapiEnvProvider : NapiEnvProvider {
  V8NapiEnvProvider() {}

  napi_env CreateEnv() override {
    m_env = v8_create_env();
    napi_open_handle_scope(m_env, &m_handleScope);

    return m_env;
  }

  void DeleteEnv() override {
    napi_close_handle_scope(m_env, m_handleScope);
    v8_delete_env(m_env);
  }

 private:
  napi_env m_env;
  napi_handle_scope m_handleScope;
};

std::vector<std::shared_ptr<NapiEnvProvider>> NapiEnvProviders() {
  return {std::make_shared<V8NapiEnvProvider>()};
}

} // namespace napitest
