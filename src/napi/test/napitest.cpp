// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#define NAPI_EXPERIMENTAL
#include "napitest.h"
#include <limits>

// Empty value so that macros here are able to return nullptr or void
#define NAPI_RETVAL_NOTHING // Intentionally blank #define

#define GET_AND_THROW_LAST_ERROR(env)                                  \
  do {                                                                 \
    const napi_extended_error_info *error_info;                        \
    napi_get_last_error_info((env), &error_info);                      \
    bool is_pending;                                                   \
    napi_is_exception_pending((env), &is_pending);                     \
    /* If an exception is already pending, don't rethrow it */         \
    if (!is_pending) {                                                 \
      const char *error_message = error_info->error_message != nullptr \
          ? error_info->error_message                                  \
          : "empty error message";                                     \
      napi_throw_error((env), nullptr, error_message);                 \
    }                                                                  \
  } while (0)

#define NAPI_ASSERT_BASE(env, assertion, message, ret_val)                \
  do {                                                                    \
    if (!(assertion)) {                                                   \
      napi_throw_error(                                                   \
          (env), nullptr, "assertion (" #assertion ") failed: " message); \
      return ret_val;                                                     \
    }                                                                     \
  } while (0)

// Returns nullptr on failed assertion.
// This is meant to be used inside napi_callback methods.
#define NAPI_ASSERT(env, assertion, message) \
  NAPI_ASSERT_BASE(env, assertion, message, nullptr)

// Returns empty on failed assertion.
// This is meant to be used inside functions with void return type.
#define NAPI_ASSERT_RETURN_VOID(env, assertion, message) \
  NAPI_ASSERT_BASE(env, assertion, message, NAPI_RETVAL_NOTHING)

#define NAPI_CALL_BASE(env, the_call, ret_val) \
  do {                                         \
    if ((the_call) != napi_ok) {               \
      GET_AND_THROW_LAST_ERROR((env));         \
      return ret_val;                          \
    }                                          \
  } while (0)

// Returns nullptr if the_call doesn't return napi_ok.
#define NAPI_CALL(env, the_call) NAPI_CALL_BASE(env, the_call, nullptr)

// Returns empty if the_call doesn't return napi_ok.
#define NAPI_CALL_RETURN_VOID(env, the_call) \
  NAPI_CALL_BASE(env, the_call, NAPI_RETVAL_NOTHING)

#define DECLARE_NAPI_PROPERTY(name, func) \
  { (name), nullptr, (func), nullptr, nullptr, nullptr, napi_default, nullptr }

#define DECLARE_NAPI_GETTER(name, func) \
  { (name), nullptr, nullptr, (func), nullptr, nullptr, napi_default, nullptr }

void add_returned_status(
    napi_env env,
    const char *key,
    napi_value object,
    const char *expected_message,
    napi_status expected_status,
    napi_status actual_status);

void add_last_status(napi_env env, const char *key, napi_value return_value);

void add_returned_status(
    napi_env env,
    const char *key,
    napi_value object,
    const char *expected_message,
    napi_status expected_status,
    napi_status actual_status) {
  char napi_message_string[100] = "";
  napi_value prop_value;

  if (actual_status != expected_status) {
    snprintf(
        napi_message_string,
        sizeof(napi_message_string),
        "Invalid status [%d]",
        actual_status);
  }

  NAPI_CALL_RETURN_VOID(
      env,
      napi_create_string_utf8(
          env,
          (actual_status == expected_status ? expected_message
                                            : napi_message_string),
          NAPI_AUTO_LENGTH,
          &prop_value));
  NAPI_CALL_RETURN_VOID(
      env, napi_set_named_property(env, object, key, prop_value));
}

void add_last_status(napi_env env, const char *key, napi_value return_value) {
  napi_value prop_value;
  const napi_extended_error_info *p_last_error;
  NAPI_CALL_RETURN_VOID(env, napi_get_last_error_info(env, &p_last_error));

  NAPI_CALL_RETURN_VOID(
      env,
      napi_create_string_utf8(
          env,
          (p_last_error->error_message == nullptr
               ? "napi_ok"
               : p_last_error->error_message),
          NAPI_AUTO_LENGTH,
          &prop_value));
  NAPI_CALL_RETURN_VOID(
      env, napi_set_named_property(env, return_value, key, prop_value));
}

// Check condition and crash process if it fails.
#define CHECK_ELSE_CRASH(condition, message)               \
  do {                                                     \
    if (!(condition)) {                                    \
      assert(false && "Failed: " #condition && (message)); \
      *((int *)0) = 1;                                     \
    }                                                      \
  } while (false)

#define EXPECT_NAPI_OK(expr)                         \
  do {                                               \
    napi_status temp_status_ = (expr);               \
    if (temp_status_ != napi_status::napi_ok) {      \
      AssertNapiException(env, temp_status_, #expr); \
    }                                                \
  } while (false)

#define EXPECT_NAPI_NOT_OK(expr, msg)           \
  do {                                          \
    napi_status temp_status_ = (expr);          \
    if (temp_status_ == napi_status::napi_ok) { \
      FAIL() << msg << " " << #expr;            \
    } else {                                    \
      ClearNapiException(env);                  \
    }                                           \
  } while (false)

#define EXPECT_CALL_TRUE(args, jsExpr)                              \
  do {                                                              \
    std::string argsStr = #args;                                    \
    std::replace(argsStr.begin(), argsStr.end(), '{', '(');         \
    std::replace(argsStr.begin(), argsStr.end(), '}', ')');         \
    EXPECT_TRUE(CallBoolFunction(args, argsStr + " => " + jsExpr)); \
  } while (false)

#define EXPECT_STRICT_EQ(left, right) EXPECT_TRUE(CheckStrictEqual(left, right))

#define EXPECT_DEEP_STRICT_EQ(left, right) \
  EXPECT_TRUE(CheckDeepStrictEqual(left, right))

#define EXPECT_JS_THROW(expr) EXPECT_TRUE(CheckThrow(expr, ""))

#define EXPECT_JS_THROW_MSG(expr, msgRegex) \
  EXPECT_TRUE(CheckThrow(expr, msgRegex))

#define EXPECT_JS_TRUE(expr) EXPECT_TRUE(CheckEqual(expr, "true"))

namespace napitest {

void AssertNapiException(
    napi_env env,
    napi_status errorCode,
    const char *exprStr) {
  napi_value error{}, errorMessage{};
  size_t messageSize{};
  const napi_extended_error_info *extendedErrorInfo{};
  napi_get_last_error_info(env, &extendedErrorInfo);
  CHECK_ELSE_CRASH(
      napi_get_and_clear_last_exception(env, &error) == napi_ok,
      "Cannot retrieve JS exception.");
  napi_get_named_property(env, error, "message", &errorMessage);
  napi_get_value_string_utf8(env, errorMessage, nullptr, 0, &messageSize);
  std::string messageStr(messageSize, '\0');
  napi_get_value_string_utf8(
      env, errorMessage, &messageStr[0], messageSize + 1, nullptr);
  FAIL() << exprStr << "\n message: " << messageStr
         << "\n error code: " << errorCode
         << "\n code message: " << extendedErrorInfo->error_message;
}

void ClearNapiException(napi_env env) {
  napi_value error{};
  CHECK_ELSE_CRASH(
      napi_get_and_clear_last_exception(env, &error) == napi_ok,
      "Cannot retrieve JS exception.");
}

napi_value NapiTestBase::Eval(const char *code) {
  napi_value result{}, global{}, func{}, undefined{}, codeStr{};
  EXPECT_NAPI_OK(napi_get_global(env, &global));
  EXPECT_NAPI_OK(napi_get_named_property(env, global, "eval", &func));
  EXPECT_NAPI_OK(napi_get_undefined(env, &undefined));
  EXPECT_NAPI_OK(
      napi_create_string_utf8(env, code, NAPI_AUTO_LENGTH, &codeStr));
  EXPECT_NAPI_OK(
      napi_call_function(env, undefined, func, 1, &codeStr, &result));
  return result;
}

napi_value NapiTestBase::Function(const std::string &code) {
  return Eval(("(" + code + ")").c_str());
}

napi_value NapiTestBase::CallFunction(
    std::initializer_list<napi_value> args,
    const std::string &code) {
  napi_value result{}, undefined{};
  napi_value func = Function(code);
  EXPECT_NAPI_OK(napi_get_undefined(env, &undefined));
  EXPECT_NAPI_OK(napi_call_function(
      env, undefined, func, args.size(), args.begin(), &result));
  return result;
}

bool NapiTestBase::CallBoolFunction(
    std::initializer_list<napi_value> args,
    const std::string &code) {
  bool result{};
  napi_value booleanResult = CallFunction(args, code);
  EXPECT_NAPI_OK(napi_get_value_bool(env, booleanResult, &result));
  return result;
}

bool NapiTestBase::CheckEqual(napi_value value, const std::string &jsValue) {
  return CallBoolFunction(
      {value}, "function(value) { return value == " + jsValue + "; }");
}

bool NapiTestBase::CheckEqual(
    const std::string &left,
    const std::string &right) {
  return CallBoolFunction(
      {}, "function() { return " + left + " == " + right + "; }");
}

bool NapiTestBase::CheckStrictEqual(
    napi_value value,
    const std::string &jsValue) {
  return CallBoolFunction(
      {value}, "function(value) { return value === " + jsValue + "; }");
}

bool NapiTestBase::CheckStrictEqual(
    const std::string &left,
    const std::string &right) {
  return CallBoolFunction(
      {}, "function() { return " + left + " === " + right + "; }");
}

static const std::string deepEqualFunc = R"JS(function(left, right) {
    function check(left, right) {
      if (left === right) {
        return true;
      }
      if (typeof left !== typeof right) {
        return false;
      }
      if (Array.isArray(left)) {
        return Array.isArray(right) && checkArray(left, right);
      }
      if (typeof left === 'number') {
        return isNaN(left) && isNaN(right);
      }
      if (typeof left === 'object') {
        return checkObject(left, right);
      }
      return false;
    }

    function checkArray(left, right) {
      if (left.length !== right.length) {
        return false;
      }
      for (let i = 0; i < left.length; ++i) {
        if (!check(left[i], right[i])) {
          return false;
        }
      }
      return true;
    }

    function checkObject(left, right) {
      const leftNames = Object.getOwnPropertyNames(left);
      const rightNames = Object.getOwnPropertyNames(right);
      if (leftNames.length !== rightNames.length) {
        return false;
      }
      for (let i = 0; i < leftNames.length; ++i) {
        if (!check(left[leftNames[i]], right[leftNames[i]])) {
          return false;
        }
      }
      const leftSymbols = Object.getOwnPropertySymbols(left);
      const rightSymbols = Object.getOwnPropertySymbols(right);
      if (leftSymbols.length !== rightSymbols.length) {
        return false;
      }
      for (let i = 0; i < leftSymbols.length; ++i) {
        if (!check(left[leftSymbols[i]], right[leftSymbols[i]])) {
          return false;
        }
      }
      return check(Object.getPrototypeOf(left), Object.getPrototypeOf(right));
    }

    return check(left, right);
  })JS";

bool NapiTestBase::CheckDeepStrictEqual(
    napi_value value,
    const std::string &jsValue) {
  return CallBoolFunction(
      {value},
      "function(value) { return " + deepEqualFunc + "(value, " + jsValue +
          "); }");
}

bool NapiTestBase::CheckDeepStrictEqual(
    const std::string &left,
    const std::string &right) {
  return CallBoolFunction(
      {},
      "function() { return " + deepEqualFunc + "(" + left + ", " + right +
          "); }");
}

bool NapiTestBase::CheckThrow(
    const std::string &expr,
    const std::string &msgRegex) {
  char jsScript[255] = {};
  snprintf(
      jsScript,
      sizeof(jsScript),
      R"(() => {
        'use strict';
        try {
          %s;
          return false;
        } catch (error) {
          return %s;
        }
      })",
      expr.c_str(),
      msgRegex.empty() ? "true" : (msgRegex + ".test(error.message)").c_str());
  return CallBoolFunction({}, jsScript);
}

napi_value NapiTestBase::GetBoolean(bool value) {
  napi_value result{};
  EXPECT_NAPI_OK(napi_get_boolean(env, value, &result));
  return result;
}

napi_value NapiTestBase::CreateInt32(int32_t value) {
  napi_value result{};
  EXPECT_NAPI_OK(napi_create_int32(env, value, &result));
  return result;
}

napi_value NapiTestBase::CreateUInt32(uint32_t value) {
  napi_value result{};
  EXPECT_NAPI_OK(napi_create_uint32(env, value, &result));
  return result;
}

napi_value NapiTestBase::CreateInt64(int64_t value) {
  napi_value result{};
  EXPECT_NAPI_OK(napi_create_int64(env, value, &result));
  return result;
}

napi_value NapiTestBase::CreateDouble(double value) {
  napi_value result{};
  EXPECT_NAPI_OK(napi_create_double(env, value, &result));
  return result;
}

napi_value NapiTestBase::CreateStringUtf8(const char *value) {
  napi_value result{};
  EXPECT_NAPI_OK(
      napi_create_string_utf8(env, value, NAPI_AUTO_LENGTH, &result));
  return result;
};

bool NapiTestBase::GetValueBool(napi_value value) {
  bool result{};
  EXPECT_NAPI_OK(napi_get_value_bool(env, value, &result));
  return result;
}
int32_t NapiTestBase::GetValueInt32(napi_value value) {
  int32_t result{};
  EXPECT_NAPI_OK(napi_get_value_int32(env, value, &result));
  return result;
}

uint32_t NapiTestBase::GetValueUInt32(napi_value value) {
  uint32_t result{};
  EXPECT_NAPI_OK(napi_get_value_uint32(env, value, &result));
  return result;
}

int64_t NapiTestBase::GetValueInt64(napi_value value) {
  int64_t result{};
  EXPECT_NAPI_OK(napi_get_value_int64(env, value, &result));
  return result;
}

double NapiTestBase::GetValueDouble(napi_value value) {
  double result{};
  EXPECT_NAPI_OK(napi_get_value_double(env, value, &result));
  return result;
}

napi_value NapiTestBase::GetProperty(napi_value object, napi_value key) {
  napi_value result{};
  EXPECT_NAPI_OK(napi_get_property(env, object, key, &result));
  return result;
};

napi_value NapiTestBase::GetProperty(napi_value object, const char *utf8Name) {
  return GetProperty(object, CreateStringUtf8(utf8Name));
}

napi_value NapiTestBase::GetNamedProperty(
    napi_value object,
    const char *utf8Name) {
  napi_value result{};
  EXPECT_NAPI_OK(napi_get_named_property(env, object, utf8Name, &result));
  return result;
};

napi_value NapiTestBase::GetPropertyNames(napi_value object) {
  napi_value result{};
  EXPECT_NAPI_OK(napi_get_property_names(env, object, &result));
  return result;
};

napi_value NapiTestBase::GetPropertySymbols(napi_value object) {
  napi_value result{};
  EXPECT_NAPI_OK(napi_get_all_property_names(
      env,
      object,
      napi_key_include_prototypes,
      napi_key_skip_strings,
      napi_key_numbers_to_strings,
      &result));
  return result;
};

void NapiTestBase::SetProperty(
    napi_value object,
    napi_value key,
    napi_value value) {
  EXPECT_NAPI_OK(napi_set_property(env, object, key, value));
};

void NapiTestBase::SetProperty(
    napi_value object,
    const char *utf8Name,
    napi_value value) {
  SetProperty(object, CreateStringUtf8(utf8Name), value);
};

void NapiTestBase::SetNamedProperty(
    napi_value object,
    const char *utf8Name,
    napi_value value) {
  EXPECT_NAPI_OK(napi_set_named_property(env, object, utf8Name, value));
};

bool NapiTestBase::HasProperty(napi_value object, napi_value key) {
  bool result{};
  EXPECT_NAPI_OK(napi_has_property(env, object, key, &result));
  return result;
};

bool NapiTestBase::HasProperty(napi_value object, const char *utf8Name) {
  return HasProperty(object, CreateStringUtf8(utf8Name));
};

bool NapiTestBase::HasNamedProperty(napi_value object, const char *utf8Name) {
  bool result{};
  EXPECT_NAPI_OK(napi_has_named_property(env, object, utf8Name, &result));
  return result;
};

bool NapiTestBase::HasOwnProperty(napi_value object, napi_value key) {
  bool result{};
  EXPECT_NAPI_OK(napi_has_own_property(env, object, key, &result));
  return result;
};

bool NapiTestBase::HasOwnProperty(napi_value object, const char *utf8Name) {
  return HasOwnProperty(object, CreateStringUtf8(utf8Name));
};

bool NapiTestBase::DeleteProperty(napi_value object, napi_value key) {
  bool result{};
  EXPECT_NAPI_OK(napi_delete_property(env, object, key, &result));
  return result;
};

bool NapiTestBase::DeleteProperty(napi_value object, const char *utf8Name) {
  return DeleteProperty(object, CreateStringUtf8(utf8Name));
};

napi_value NapiTestBase::CreateObject() {
  napi_value result{};
  EXPECT_NAPI_OK(napi_create_object(env, &result));
  return result;
};

uint32_t NapiTestBase::GetArrayLength(napi_value value) {
  uint32_t result{};
  EXPECT_NAPI_OK(napi_get_array_length(env, value, &result));
  return result;
}

napi_value NapiTestBase::GetElement(napi_value value, uint32_t index) {
  napi_value result{};
  EXPECT_NAPI_OK(napi_get_element(env, value, index, &result));
  return result;
}

napi_value NapiTestBase::ObjectFreeze(napi_value object) {
  EXPECT_NAPI_OK(napi_object_freeze(env, object));
  return object;
}

napi_value NapiTestBase::ObjectSeal(napi_value object) {
  EXPECT_NAPI_OK(napi_object_seal(env, object));
  return object;
}

napi_value NapiTestBase::DefineClass(
    const char *utf8Name,
    size_t nameLength,
    napi_callback constructor,
    void *data,
    size_t propertyCount,
    const napi_property_descriptor *properties) {
  napi_value result{};
  EXPECT_NAPI_OK(napi_define_class(
      env,
      utf8Name,
      nameLength,
      constructor,
      data,
      propertyCount,
      properties,
      &result));
  return result;
}

napi_value NapiTestBase::AsBool(napi_value value) {
  return GetBoolean(GetValueBool(value));
}

napi_value NapiTestBase::AsInt32(napi_value value) {
  return CreateInt32(GetValueInt32(value));
}

napi_value NapiTestBase::AsUInt32(napi_value value) {
  return CreateUInt32(GetValueUInt32(value));
}

napi_value NapiTestBase::AsInt64(napi_value value) {
  return CreateInt64(GetValueInt64(value));
}

napi_value NapiTestBase::AsDouble(napi_value value) {
  return CreateDouble(GetValueDouble(value));
}

napi_value NapiTestBase::AsString(napi_value value) {
  char strValue[100];
  EXPECT_NAPI_OK(napi_get_value_string_utf8(
      env, value, strValue, sizeof(strValue), nullptr));
  return CreateStringUtf8(strValue);
}

napi_value NapiTestBase::ToBool(napi_value value) {
  napi_value result;
  EXPECT_NAPI_OK(napi_coerce_to_bool(env, value, &result));
  return result;
}

napi_value NapiTestBase::ToNumber(napi_value value) {
  napi_value result;
  EXPECT_NAPI_OK(napi_coerce_to_number(env, value, &result));
  return result;
}

napi_value NapiTestBase::ToObject(napi_value value) {
  napi_value result;
  EXPECT_NAPI_OK(napi_coerce_to_object(env, value, &result));
  return result;
}

napi_value NapiTestBase::ToString(napi_value value) {
  napi_value result;
  EXPECT_NAPI_OK(napi_coerce_to_string(env, value, &result));
  return result;
}

} // namespace napitest

using namespace napitest;

struct NapiTest : NapiTestBase {};

TEST_P(NapiTest, RunScriptTest) {
  napi_value script{}, scriptResult{}, global{}, xValue{};
  int intValue{};
  EXPECT_NAPI_OK(napi_create_string_utf8(env, "1", NAPI_AUTO_LENGTH, &script));
  EXPECT_NAPI_OK(napi_run_script(env, script, &scriptResult));
  EXPECT_NAPI_OK(napi_get_value_int32(env, scriptResult, &intValue));
  EXPECT_EQ(intValue, 1);

  EXPECT_NAPI_OK(napi_create_string_utf8(env, "x = 42", NAPI_AUTO_LENGTH, &script));
  EXPECT_NAPI_OK(napi_run_script(env, script, &scriptResult));
  EXPECT_NAPI_OK(napi_get_global(env, &global));
  EXPECT_NAPI_OK(napi_get_named_property(env, global, "x", &xValue));
  EXPECT_NAPI_OK(napi_get_value_int32(env, xValue, &intValue));
  EXPECT_EQ(intValue, 42);
}

INSTANTIATE_TEST_SUITE_P(
    NapiEnv,
    NapiTest,
    ::testing::ValuesIn(NapiEnvProviders()));
