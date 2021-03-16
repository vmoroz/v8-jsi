// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// These tests are for NAPI and are not JS engine specific

#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <gtest/gtest.h>
#include "../js_native_api.h"

namespace napitest {

struct NapiEnvProvider {
  virtual napi_env CreateEnv() = 0;
  virtual void DeleteEnv() = 0;
};

std::vector<std::shared_ptr<NapiEnvProvider>> NapiEnvProviders();
} // namespace napitest

//=============================================================================
// From nodejs\node\test\js-native-api\common.h
//=============================================================================

// Empty value so that macros here are able to return nullptr or void
#define NAPI_RETVAL_NOTHING // Intentionally blank #define

#define GET_AND_THROW_LAST_ERROR(env)                               \
  do {                                                              \
    const napi_extended_error_info *error_info;                     \
    napi_get_last_error_info((env), &error_info);                   \
    bool is_pending;                                                \
    napi_is_exception_pending((env), &is_pending);                  \
    /* If an exception is already pending, don't rethrow it */      \
    if (!is_pending) {                                              \
      const char *error_message = error_info->error_message != NULL \
          ? error_info->error_message                               \
          : "empty error message";                                  \
      napi_throw_error((env), NULL, error_message);                 \
    }                                                               \
  } while (0)

#define NODE_API_CALL_BASE(env, the_call, ret_val) \
  do {                                             \
    if ((the_call) != napi_ok) {                   \
      GET_AND_THROW_LAST_ERROR((env));             \
      return ret_val;                              \
    }                                              \
  } while (0)

// Returns NULL if the_call doesn't return napi_ok.
#define NODE_API_CALL(env, the_call) NODE_API_CALL_BASE(env, the_call, NULL)

#define NODE_API_ASSERT_BASE(env, assertion, message, ret_val)         \
  do {                                                                 \
    if (!(assertion)) {                                                \
      napi_throw_error(                                                \
          (env), NULL, "assertion (" #assertion ") failed: " message); \
      return ret_val;                                                  \
    }                                                                  \
  } while (0)

// Returns NULL on failed assertion.
// This is meant to be used inside napi_callback methods.
#define NODE_API_ASSERT(env, assertion, message) \
  NODE_API_ASSERT_BASE(env, assertion, message, NULL)

//=============================================================================
// ^--- end nodejs\node\test\js-native-api\common.h
//=============================================================================

#define THROW_IF_NOT_OK(expr)                        \
  do {                                               \
    napi_status temp_status__ = (expr);              \
    if (temp_status__ != napi_status::napi_ok) {     \
      throw NapiTestException(temp_status__, #expr); \
    }                                                \
  } while (false)

#define EXPECT_JS_CODE_STRICT_EQ(left, right) \
  EXPECT_TRUE(CheckStrictEqual(left, right))

#define EXPECT_JS_STRICT_EQ(expr, expectedJSValue)                             \
  do {                                                                         \
    try {                                                                      \
      napi_value actualResult__ = (expr);                                      \
      std::string expectedResult__ = (expectedJSValue);                        \
      if (!CheckStrictEqual(actualResult__, expectedResult__)) {               \
        FAIL() << "Not JavaScript strict equal"                                \
               << "\n Expression: " << #expr                                   \
               << "\n   Expected: " << expectedResult__;                       \
      }                                                                        \
    } catch (NapiTestException const &ex) {                                    \
      std::string error_message__ = GetNapiErrorMessage();                     \
      FAIL() << "NAPI call failed"                                             \
             << "\n  Expression: " << #expr << "\n Failed expr: " << ex.Expr() \
             << "\n  Error code: " << ex.ErrorCode();                          \
    }                                                                          \
  } while (false)

#define EXPECT_JS_CODE_DEEP_STRICT_EQ(left, right) \
  EXPECT_TRUE(CheckDeepStrictEqual(left, right))

#define EXPECT_JS_DEEP_STRICT_EQ(expr, expectedJSValue)                        \
  do {                                                                         \
    try {                                                                      \
      napi_value actualResult__ = (expr);                                      \
      std::string expectedResult__ = (expectedJSValue);                        \
      if (!CheckDeepStrictEqual(actualResult__, expectedResult__)) {           \
        FAIL() << "Not JavaScript deep strict equal"                           \
               << "\n Expression: " << #expr                                   \
               << "\n   Expected: " << expectedResult__;                       \
      }                                                                        \
    } catch (NapiTestException const &ex) {                                    \
      std::string error_message__ = GetNapiErrorMessage();                     \
      FAIL() << "NAPI call failed"                                             \
             << "\n  Expression: " << #expr << "\n Failed expr: " << ex.Expr() \
             << "\n  Error code: " << ex.ErrorCode();                          \
    }                                                                          \
  } while (false)

#define EXPECT_JS_NOT_DEEP_STRICT_EQ(expr, expectedJSValue)                    \
  do {                                                                         \
    try {                                                                      \
      napi_value actualResult__ = (expr);                                      \
      std::string expectedResult__ = (expectedJSValue);                        \
      if (CheckDeepStrictEqual(actualResult__, expectedResult__)) {            \
        FAIL() << "Unexpected JavaScript deep strict equal"                    \
               << "\n Expression: " << #expr                                   \
               << "\n   Expected: " << expectedResult__;                       \
      }                                                                        \
    } catch (NapiTestException const &ex) {                                    \
      std::string error_message__ = GetNapiErrorMessage();                     \
      FAIL() << "NAPI call failed"                                             \
             << "\n  Expression: " << #expr << "\n Failed expr: " << ex.Expr() \
             << "\n  Error code: " << ex.ErrorCode();                          \
    }                                                                          \
  } while (false)

#define EXPECT_JS_THROW(expr) EXPECT_TRUE(CheckThrow(expr, ""))

#define EXPECT_JS_THROW_MSG(expr, msgRegex) \
  EXPECT_TRUE(CheckThrow(expr, msgRegex))

#define EXPECT_JS_TRUE(expr) EXPECT_TRUE(CheckEqual(expr, "true"))

void add_returned_status(
    napi_env env,
    const char *key,
    napi_value object,
    const char *expected_message,
    napi_status expected_status,
    napi_status actual_status);

void add_last_status(napi_env env, const char *key, napi_value return_value);

namespace napitest {

struct NapiTestException : std::exception {
  NapiTestException(){};
  NapiTestException(napi_status errorCode, const char *expr)
      : m_errorCode{errorCode}, m_expr{expr} {}

  const char *what() const noexcept override {
    return "Failed";
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
};

// TODO: [vmoroz] Remove?
struct NapiException : std::exception {
  NapiException() {}
  NapiException(std::string what) : m_what(std::move(what)) {}

  virtual const char *what() const noexcept override {
    return m_what.c_str();
  }

  virtual ~NapiException() {}

 protected:
  std::string m_what;
};

[[noreturn]] void ThrowNapiException(napi_env env, napi_status errorCode);

struct NapiTestBase
    : ::testing::TestWithParam<std::shared_ptr<NapiEnvProvider>> {
  NapiTestBase() : provider(GetParam()), env(provider->CreateEnv()) {}
  ~NapiTestBase() {
    provider->DeleteEnv();
  }
  std::string GetNapiErrorMessage();

  napi_value Eval(const char *code);
  napi_value Value(const std::string &code);
  napi_value Function(const std::string &code);
  napi_value CallFunction(
      std::initializer_list<napi_value> args,
      const std::string &code);
  bool CallBoolFunction(
      std::initializer_list<napi_value> args,
      const std::string &code);
  bool CheckEqual(napi_value value, const std::string &jsValue);
  bool CheckEqual(const std::string &left, const std::string &right);
  bool CheckStrictEqual(napi_value value, const std::string &jsValue);
  bool CheckStrictEqual(const std::string &left, const std::string &right);
  bool CheckDeepStrictEqual(napi_value value, const std::string &jsValue);
  bool CheckDeepStrictEqual(const std::string &left, const std::string &right);
  bool CheckThrow(const std::string &expr, const std::string &msgRegex);
  bool CheckErrorRegExp(
      const std::string &errorMessage,
      const std::string &matchRegex);
  std::string GetErrorMessage(const std::string &expr);
  std::string ValueToStdString(napi_value value);

  napi_value GetBoolean(bool value);
  napi_value CreateInt32(int32_t value);
  napi_value CreateUInt32(uint32_t value);
  napi_value CreateInt64(int64_t value);
  napi_value CreateDouble(double value);
  napi_value CreateStringUtf8(const char *value);
  napi_value CreateObject();

  bool GetValueBool(napi_value value);
  int32_t GetValueInt32(napi_value value);
  uint32_t GetValueUInt32(napi_value value);
  int64_t GetValueInt64(napi_value value);
  double GetValueDouble(napi_value value);

  napi_value AsBool(napi_value value);
  napi_value AsInt32(napi_value value);
  napi_value AsUInt32(napi_value value);
  napi_value AsInt64(napi_value value);
  napi_value AsDouble(napi_value value);
  napi_value AsString(napi_value value);
  napi_value ToBool(napi_value value);
  napi_value ToNumber(napi_value value);
  napi_value ToObject(napi_value value);
  napi_value ToString(napi_value value);

  napi_value GetProperty(napi_value object, napi_value key);
  napi_value GetProperty(napi_value object, const char *utf8Name);
  napi_value GetNamedProperty(napi_value object, const char *utf8Name);
  napi_value GetPropertyNames(napi_value object);
  napi_value GetPropertySymbols(napi_value object);
  void SetProperty(napi_value object, napi_value key, napi_value value);
  void SetProperty(napi_value object, const char *utf8Name, napi_value value);
  void
  SetNamedProperty(napi_value object, const char *utf8Name, napi_value value);
  bool HasProperty(napi_value object, napi_value key);
  bool HasProperty(napi_value object, const char *utf8Name);
  bool HasNamedProperty(napi_value object, const char *utf8Name);
  bool HasOwnProperty(napi_value object, napi_value key);
  bool HasOwnProperty(napi_value object, const char *utf8Name);
  bool DeleteProperty(napi_value object, napi_value key);
  bool DeleteProperty(napi_value object, const char *utf8Name);
  uint32_t GetArrayLength(napi_value value);
  napi_value GetElement(napi_value value, uint32_t index);
  napi_value ObjectFreeze(napi_value object);
  napi_value ObjectSeal(napi_value object);
  napi_value DefineClass(
      const char *utf8Name,
      size_t nameLength,
      napi_callback constructor,
      void *data,
      size_t propertyCount,
      const napi_property_descriptor *properties);

 protected:
  std::shared_ptr<NapiEnvProvider> provider;
  napi_env env;
};

} // namespace napitest