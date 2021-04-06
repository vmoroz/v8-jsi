// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// These tests are for NAPI and are not JS engine specific

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#define NAPI_EXPERIMENTAL
#include "js_native_ext_api.h"
#include "lib/modules.h"

extern "C" {
#include "js-native-api/common.h"
}

constexpr napi_property_attributes operator|(
    napi_property_attributes left,
    napi_property_attributes right) {
  return napi_property_attributes(
      static_cast<int>(left) | static_cast<int>(right));
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
  testContext->RunTestScript(   \
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

struct NapiTestBase;
struct NapiTestContext;
struct NapiTestErrorHandler;

struct NapiEnvProvider {
  virtual napi_env CreateEnv() = 0;
  virtual void DeleteEnv(napi_env env) = 0;
};

std::vector<std::shared_ptr<NapiEnvProvider>> NapiEnvProviders();

struct NapiScriptError {
  std::string Name;
  std::string Message;
  std::string Stack;
};

struct NapiAssertionError {
  std::string Method;
  std::string Expected;
  std::string Actual;
  std::string SourceFile;
  int32_t SourceLine;
  std::string ErrorStack;
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

struct ModuleInfo {
  char const *script{nullptr};
  napi_ref module{nullptr};
  std::string file;
  int32_t line{0};
};

struct NapiTestBase
    : ::testing::TestWithParam<std::shared_ptr<NapiEnvProvider>> {
  static void ExecuteNapi(
      std::function<void(NapiTestContext *, napi_env)> code) noexcept;
};

struct NapiTestContext {
  NapiTestContext(napi_env env);
  ~NapiTestContext();

  napi_value RunScript(char const *code, char const *sourceUrl = nullptr);
  napi_value GetModule(char const *moduleName);

  NapiTestErrorHandler
  RunTestScript(char const *script, char const *file, int32_t line);

  NapiTestErrorHandler RunTestScript(TestScriptInfo const &scripInfo);

  void AddNativeModule(
      char const *moduleName,
      std::function<napi_value(napi_env, napi_value)> initModule);

  ModuleInfo const *GetModuleInfo(std::string const &moduleName) noexcept;

  void StartTest();
  void EndTest();
  void RunCallChecks();
  void HandleUnhandledPromiseRejections();

  std::string ProcessStack(
      std::string const &stack,
      std::string const &assertMethod);

  void SetImmediate(napi_ref callback) noexcept;

 private:
  napi_env env;
  napi_env_scope m_envScope{nullptr};
  napi_handle_scope m_handleScope;
  std::map<std::string, std::shared_ptr<ModuleInfo>, std::less<>> m_modules;
  std::map<std::string, std::function<napi_value(napi_env, napi_value)>>
      m_nativeModules;
  std::queue<napi_ref> m_immediateQueue;
};

struct NapiTestErrorHandler {
  NapiTestErrorHandler(
      NapiTestContext *testContext,
      std::exception_ptr const &exception,
      std::string &&script,
      std::string &&file,
      int32_t line,
      int32_t scriptLineOffset) noexcept;
  ~NapiTestErrorHandler() noexcept;
  void Catch(std::function<void(NapiTestException const &)> &&handler) noexcept;
  void Throws(
      std::function<void(NapiTestException const &)> &&handler) noexcept;
  void Throws(
      char const *jsErrorName,
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
  NapiTestContext *m_testContext;
  std::exception_ptr m_exception;
  std::string m_script;
  std::string m_file;
  int32_t m_line;
  int32_t m_scriptLineOffset;
  std::function<void(NapiTestException const &)> m_handler;
  bool m_mustThrow{false};
  std::string m_jsErrorName;
};

} // namespace napitest
