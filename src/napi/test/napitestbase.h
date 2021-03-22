// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// These tests are for NAPI and are not JS engine specific

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#define NAPI_EXPERIMENTAL
#include "../js_native_api.h"

#include "js-native-api/common.h"

#define THROW_IF_NOT_OK(expr)                             \
  do {                                                    \
    napi_status temp_status__ = (expr);                   \
    if (temp_status__ != napi_status::napi_ok) {          \
      throw NapiTestException(env, temp_status__, #expr); \
    }                                                     \
  } while (false)

namespace napitest {

struct NapiEnvProvider {
  virtual napi_env CreateEnv() = 0;
  virtual void DeleteEnv() = 0;
};

std::vector<std::shared_ptr<NapiEnvProvider>> NapiEnvProviders();

struct NapiScriptError {
  std::string Name;
  std::string Message;
  std::string Stack;
};

struct NapiAssertionError {
  std::string Expected;
  std::string Actual;
  std::string SourceFile;
  int32_t SourceLine;
};

struct NapiTestException : std::exception {
  NapiTestException() noexcept = default;

  NapiTestException(
      napi_env env,
      napi_status errorCode,
      char const *expr) noexcept;

  const char *what() const noexcept override {
    return m_what.c_str();
  }

  napi_status ErrorCode() const noexcept {
    return m_errorCode;
  }

  std::string const &Expr() const noexcept {
    return m_expr;
  }

  NapiScriptError const *ScriptError() const noexcept {
    return m_scriptError.get();
  }

  NapiAssertionError const *AssertionError() const noexcept {
    return m_assertionError.get();
  }

 private:
  static napi_value GetProperty(napi_env env, napi_value obj, char const *name);

  static std::string
  GetPropertyString(napi_env env, napi_value obj, char const *name);

  static int32_t
  GetPropertyInt32(napi_env env, napi_value obj, char const *name);

 private:
  napi_status m_errorCode{};
  std::string m_expr;
  std::string m_what;
  std::unique_ptr<NapiScriptError> m_scriptError;
  std::unique_ptr<NapiAssertionError> m_assertionError;
};

struct NapiTestBase
    : ::testing::TestWithParam<std::shared_ptr<NapiEnvProvider>> {
  NapiTestBase();
  ~NapiTestBase();

  napi_value RunScript(char const *code, char const *sourceUrl = nullptr);
  napi_value GetModule(char const *moduleName);

  std::string GetSourceCodeSliceForError(
      char const *code,
      int32_t lineIndex,
      int32_t extraLineCount);

 protected:
  std::shared_ptr<NapiEnvProvider> provider;
  napi_env env;

 private:
  std::map<std::string, char const *, std::less<>> m_moduleScripts;
  std::map<std::string, napi_ref, std::less<>> m_modules;
};

} // namespace napitest
