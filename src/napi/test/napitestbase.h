// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// These tests are for NAPI and are not JS engine specific

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#define NAPI_EXPERIMENTAL
#include "../js_native_api.h"

#include "js-native-api/common.h"

#define THROW_IF_NOT_OK(expr)                                               \
  do {                                                                      \
    napi_status temp_status__ = (expr);                                     \
    if (temp_status__ != napi_status::napi_ok) {                            \
      throw NapiTestException(temp_status__, #expr, GetExceptionMessage()); \
    }                                                                       \
  } while (false)

namespace napitest {

struct NapiEnvProvider {
  virtual napi_env CreateEnv() = 0;
  virtual void DeleteEnv() = 0;
};

std::vector<std::shared_ptr<NapiEnvProvider>> NapiEnvProviders();

struct NapiTestException : std::exception {
  NapiTestException(){};
  NapiTestException(napi_status errorCode, const char *expr, std::string message)
      : m_errorCode{errorCode}, m_expr{expr}, m_message(std::move(message)) {}

  const char *what() const noexcept override {
    return m_message.c_str();
  }

  napi_status ErrorCode() const noexcept {
    return m_errorCode;
  }

  std::string const &Expr() const noexcept {
    return m_expr;
  }

 private:
  napi_status m_errorCode{};
  std::string m_expr;
  std::string m_message;
};

struct NapiTestBase
    : ::testing::TestWithParam<std::shared_ptr<NapiEnvProvider>> {
  NapiTestBase();
  ~NapiTestBase();

  std::string GetExceptionMessage();
  napi_value RunScript(const char *code);

 protected:
  std::shared_ptr<NapiEnvProvider> provider;
  napi_env env;
};

} // namespace napitest
