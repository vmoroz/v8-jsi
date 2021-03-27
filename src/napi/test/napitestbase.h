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
#include "js_native_test_api.h"
#include "lib/modules.h"

extern "C" {
#include "js-native-api/common.h"
}

inline napi_property_attributes operator|(
    napi_property_attributes left,
    napi_property_attributes right) {
  return napi_property_attributes((int)left | (int)right);
}

#define THROW_IF_NOT_OK(expr)                             \
  do {                                                    \
    napi_status temp_status__ = (expr);                   \
    if (temp_status__ != napi_status::napi_ok) {          \
      throw NapiTestException(env, temp_status__, #expr); \
    }                                                     \
  } while (false)

// Runs the script with captured file name and the line number.
// The __LINE__ points to the end of the macro call.
// We must adjust the line number to point to the beginning of hte script.
#define RUN_TEST_SCRIPT(script) \
  RunTestScript(                \
      script, __FILE__, (__LINE__ - napitest::GetEndOfLineCount(script)))

#define FAIL_AT(file, line) \
  GTEST_MESSAGE_AT_(        \
      file, line, "Fail", ::testing::TestPartResult::kFatalFailure)

#define ASSERT_NAPI_OK(expr)                                        \
  do {                                                              \
    napi_status result_status__ = (expr);                           \
    if (result_status__ != napi_ok) {                               \
      FAIL() << "NAPI call failed with status: " << result_status__ \
             << "\n Expression: " << #expr;                         \
    }                                                               \
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

  NapiTestException(napi_env env, napi_value error) noexcept;

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
  void ApplyScriptErrorData(napi_env env, napi_value error);

  static napi_value GetProperty(napi_env env, napi_value obj, char const *name);

  static std::string
  GetPropertyString(napi_env env, napi_value obj, char const *name);

  static int32_t
  GetPropertyInt32(napi_env env, napi_value obj, char const *name);

 private:
  napi_status m_errorCode{};
  std::string m_expr;
  std::string m_what;
  std::shared_ptr<NapiScriptError> m_scriptError;
  std::shared_ptr<NapiAssertionError> m_assertionError;
};

struct NapiTestErrorHandler {
  NapiTestErrorHandler(
      std::exception_ptr const &exception,
      std::string &&script,
      std::string &&file,
      int32_t line) noexcept;
  ~NapiTestErrorHandler() noexcept;
  void Catch(std::function<void(NapiTestException const &)> &&handler) noexcept;
  void Throws(
      std::function<void(NapiTestException const &)> &&handler) noexcept;

  NapiTestErrorHandler(NapiTestErrorHandler const &) = delete;
  NapiTestErrorHandler &operator=(NapiTestErrorHandler const &) = delete;

  NapiTestErrorHandler(NapiTestErrorHandler &&) = default;
  NapiTestErrorHandler &operator=(NapiTestErrorHandler &&) = default;

 private:
  std::string GetSourceCodeSliceForError(
      int32_t lineIndex,
      int32_t extraLineCount) noexcept;

 private:
  std::exception_ptr m_exception;
  std::string m_script;
  std::string m_file;
  int32_t m_line;
  std::function<void(NapiTestException const &)> m_handler;
  bool m_mustThrow{false};
};

struct NapiTestBase
    : ::testing::TestWithParam<std::shared_ptr<NapiEnvProvider>> {
  NapiTestBase();
  ~NapiTestBase();

  napi_value RunScript(char const *code, char const *sourceUrl = nullptr);
  napi_value GetModule(char const *moduleName);

  NapiTestErrorHandler
  RunTestScript(char const *script, char const *file, int32_t line);

  NapiTestErrorHandler RunTestScript(TestScriptInfo const &scripInfo);

  void AddModule(char const *moduleName, napi_ref module);
  void AddNativeModule(
      char const *moduleName,
      std::function<napi_value(napi_env, napi_value)> initModule);

  void StartTest();
  void RunCallChecks();
  void HandleUnhandledPromiseRejections();

 protected:
  std::shared_ptr<NapiEnvProvider> provider;
  napi_env env;

 private:
  std::map<std::string, char const *, std::less<>> m_moduleScripts;
  std::map<std::string, napi_ref, std::less<>> m_modules;
};

struct ScopedExposeGC {
  ScopedExposeGC() : m_wasExposed(napi_test_enable_gc_api(true)) {}
  ~ScopedExposeGC() {
    napi_test_enable_gc_api(m_wasExposed);
  }

 private:
  const bool m_wasExposed{false};
};

struct NapiTestContext {
  NapiTestContext(NapiTestBase *testBase);
  ~NapiTestContext();

 private:
  NapiTestBase *m_testBase;
  ScopedExposeGC m_exposeGC;
};

} // namespace napitest
