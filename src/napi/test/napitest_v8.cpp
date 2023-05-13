// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <gtest/gtest.h>
#include "napitest.h"
#include "v8_api.h"

#include <memory>
#include <vector>

namespace napitest {

class V8RuntimeHolder : public IEnvHolder {
 public:
  V8RuntimeHolder() noexcept {
    v8_config config{};
    v8_create_config(&config);
    v8_config_enable_gc_api(config, true);
    v8_create_runtime(config, &runtime_);
    v8_get_node_api_env(runtime_, &env_);
  }

  ~V8RuntimeHolder() {
    v8_delete_runtime(runtime_);
  }

  V8RuntimeHolder(const V8RuntimeHolder &) = delete;
  V8RuntimeHolder &operator=(const V8RuntimeHolder &) = delete;

  napi_env getEnv() override {
    return env_;
  }

 private:
  v8_runtime runtime_{};
  napi_env env_{};
};

std::vector<NapiTestData> NapiEnvFactories() {
  return {{"jsi/napi/test/js-native-api", [] { return std::unique_ptr<IEnvHolder>(new V8RuntimeHolder()); }}};
}

} // namespace napitest
