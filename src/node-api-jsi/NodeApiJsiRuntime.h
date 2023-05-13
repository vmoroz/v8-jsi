// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once
#ifndef SRC_NODEAPIJSIRUNTIME_H_
#define SRC_NODEAPIJSIRUNTIME_H_

#include <jsi/jsi.h>
#include <functional>
#include <memory>
#include "NodeApi.h"

namespace Microsoft::NodeApiJsi {

std::unique_ptr<facebook::jsi::Runtime>
makeNodeApiJsiRuntime(napi_env env, NodeApi *nodeApi, std::function<void()> onDelete) noexcept;

struct NodeApiEnvScope {
  NodeApiEnvScope(napi_env env) : env_(env) {
    napi_ext_open_env_scope(env, &scope_);
  }

  NodeApiEnvScope(const NodeApiEnvScope &) = delete;
  NodeApiEnvScope &operator=(const NodeApiEnvScope &) = delete;

  ~NodeApiEnvScope() {
    napi_ext_close_env_scope(env_, &scope_);
  }

 private:
  napi_env env_{};
  napi_ext_env_scope scope_{};
};
} // namespace Microsoft::NodeApiJsi

#endif // !SRC_NODEAPIJSIRUNTIME_H_
