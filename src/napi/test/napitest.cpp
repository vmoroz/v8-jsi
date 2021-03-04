// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#define NAPI_EXPERIMENTAL
#include "napitest.h"
#include <algorithm>
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

  EXPECT_NAPI_OK(
      napi_create_string_utf8(env, "x = 42", NAPI_AUTO_LENGTH, &script));
  EXPECT_NAPI_OK(napi_run_script(env, script, &scriptResult));
  EXPECT_NAPI_OK(napi_get_global(env, &global));
  EXPECT_NAPI_OK(napi_get_named_property(env, global, "x", &xValue));
  EXPECT_NAPI_OK(napi_get_value_int32(env, xValue, &intValue));
  EXPECT_EQ(intValue, 42);
}

TEST_P(NapiTest, StringTest) {
  auto TestLatin1 = [&](napi_value value) {
    char buffer[128];
    size_t bufferSize = 128;
    size_t copied{};
    napi_value result{};

    EXPECT_NAPI_OK(
        napi_get_value_string_latin1(env, value, buffer, bufferSize, &copied));
    EXPECT_NAPI_OK(napi_create_string_latin1(env, buffer, copied, &result));

    return result;
  };

  auto TestUtf8 = [&](napi_value value) {
    char buffer[128];
    size_t bufferSize = 128;
    size_t copied{};
    napi_value result{};

    EXPECT_NAPI_OK(
        napi_get_value_string_utf8(env, value, buffer, bufferSize, &copied));
    EXPECT_NAPI_OK(napi_create_string_utf8(env, buffer, copied, &result));

    return result;
  };

  auto TestUtf16 = [&](napi_value value) {
    char16_t buffer[128];
    size_t bufferSize = 128;
    size_t copied{};
    napi_value result{};

    EXPECT_NAPI_OK(
        napi_get_value_string_utf16(env, value, buffer, bufferSize, &copied));
    EXPECT_NAPI_OK(napi_create_string_utf16(env, buffer, copied, &result));

    return result;
  };

  auto TestLatin1Insufficient = [&](napi_value value) {
    char buffer[4];
    size_t bufferSize = 4;
    size_t copied{};
    napi_value result{};

    EXPECT_NAPI_OK(
        napi_get_value_string_latin1(env, value, buffer, bufferSize, &copied));
    EXPECT_NAPI_OK(napi_create_string_latin1(env, buffer, copied, &result));

    return result;
  };

  auto TestUtf8Insufficient = [&](napi_value value) {
    char buffer[4];
    size_t bufferSize = 4;
    size_t copied{};
    napi_value result{};

    EXPECT_NAPI_OK(
        napi_get_value_string_utf8(env, value, buffer, bufferSize, &copied));
    EXPECT_NAPI_OK(napi_create_string_utf8(env, buffer, copied, &result));

    return result;
  };

  auto TestUtf16Insufficient = [&](napi_value value) {
    char16_t buffer[4];
    size_t bufferSize = 4;
    size_t copied{};
    napi_value result{};

    EXPECT_NAPI_OK(
        napi_get_value_string_utf16(env, value, buffer, bufferSize, &copied));
    EXPECT_NAPI_OK(napi_create_string_utf16(env, buffer, copied, &result));
    return result;
  };

  auto Utf16Length = [&](napi_value value) {
    size_t length{};
    napi_value result{};
    EXPECT_NAPI_OK(
        napi_get_value_string_utf16(env, value, nullptr, 0, &length));
    EXPECT_NAPI_OK(napi_create_uint32(env, (uint32_t)length, &result));
    return result;
  };

  auto Utf8Length = [&](napi_value value) {
    size_t length{};
    napi_value result{};
    EXPECT_NAPI_OK(napi_get_value_string_utf8(env, value, nullptr, 0, &length));
    EXPECT_NAPI_OK(napi_create_uint32(env, (uint32_t)length, &result));
    return result;
  };

  Eval(R"(
    empty = '';
    str1 = 'hello world';
    str2 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
    str3 = '?!@#$%^&*()_+-=[]{}/.,<>\'"\\';
    str4 = '¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿';
    str5 = 'ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþ';
    str6 = '\u{2003}\u{2101}\u{2001}\u{202}\u{2011}';
  )");

  napi_value global{}, empty{}, str1{}, str2{}, str3{}, str4{}, str5{}, str6{};
  EXPECT_NAPI_OK(napi_get_global(env, &global));
  EXPECT_NAPI_OK(napi_get_named_property(env, global, "empty", &empty));
  EXPECT_NAPI_OK(napi_get_named_property(env, global, "str1", &str1));
  EXPECT_NAPI_OK(napi_get_named_property(env, global, "str2", &str2));
  EXPECT_NAPI_OK(napi_get_named_property(env, global, "str3", &str3));
  EXPECT_NAPI_OK(napi_get_named_property(env, global, "str4", &str4));
  EXPECT_NAPI_OK(napi_get_named_property(env, global, "str5", &str5));
  EXPECT_NAPI_OK(napi_get_named_property(env, global, "str6", &str6));

  EXPECT_TRUE(CheckStrictEqual(TestLatin1(empty), "empty"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf8(empty), "empty"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf16(empty), "empty"));
  EXPECT_TRUE(CheckStrictEqual(Utf16Length(empty), "0"));
  EXPECT_TRUE(CheckStrictEqual(Utf8Length(empty), "0"));

  EXPECT_TRUE(CheckStrictEqual(TestLatin1(str1), "str1"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf8(str1), "str1"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf16(str1), "str1"));
  EXPECT_TRUE(
      CheckStrictEqual(TestLatin1Insufficient(str1), "str1.slice(0, 3)"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf8Insufficient(str1), "str1.slice(0, 3)"));
  EXPECT_TRUE(
      CheckStrictEqual(TestUtf16Insufficient(str1), "str1.slice(0, 3)"));
  EXPECT_TRUE(CheckStrictEqual(Utf16Length(str1), "11"));
  EXPECT_TRUE(CheckStrictEqual(Utf8Length(str1), "11"));

  EXPECT_TRUE(CheckStrictEqual(TestLatin1(str2), "str2"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf8(str2), "str2"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf16(str2), "str2"));
  EXPECT_TRUE(
      CheckStrictEqual(TestLatin1Insufficient(str2), "str2.slice(0, 3)"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf8Insufficient(str2), "str2.slice(0, 3)"));
  EXPECT_TRUE(
      CheckStrictEqual(TestUtf16Insufficient(str2), "str2.slice(0, 3)"));
  EXPECT_TRUE(CheckStrictEqual(Utf16Length(str2), "62"));
  EXPECT_TRUE(CheckStrictEqual(Utf8Length(str2), "62"));

  EXPECT_TRUE(CheckStrictEqual(TestLatin1(str3), "str3"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf8(str3), "str3"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf16(str3), "str3"));
  EXPECT_TRUE(
      CheckStrictEqual(TestLatin1Insufficient(str3), "str3.slice(0, 3)"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf8Insufficient(str3), "str3.slice(0, 3)"));
  EXPECT_TRUE(
      CheckStrictEqual(TestUtf16Insufficient(str3), "str3.slice(0, 3)"));
  EXPECT_TRUE(CheckStrictEqual(Utf16Length(str3), "27"));
  EXPECT_TRUE(CheckStrictEqual(Utf8Length(str3), "27"));

  EXPECT_TRUE(CheckStrictEqual(TestLatin1(str4), "str4"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf8(str4), "str4"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf16(str4), "str4"));
  EXPECT_TRUE(
      CheckStrictEqual(TestLatin1Insufficient(str4), "str4.slice(0, 3)"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf8Insufficient(str4), "str4.slice(0, 1)"));
  EXPECT_TRUE(
      CheckStrictEqual(TestUtf16Insufficient(str4), "str4.slice(0, 3)"));
  EXPECT_TRUE(CheckStrictEqual(Utf16Length(str4), "31"));
  EXPECT_TRUE(CheckStrictEqual(Utf8Length(str4), "62"));

  EXPECT_TRUE(CheckStrictEqual(TestLatin1(str5), "str5"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf8(str5), "str5"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf16(str5), "str5"));
  EXPECT_TRUE(
      CheckStrictEqual(TestLatin1Insufficient(str5), "str5.slice(0, 3)"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf8Insufficient(str5), "str5.slice(0, 1)"));
  EXPECT_TRUE(
      CheckStrictEqual(TestUtf16Insufficient(str5), "str5.slice(0, 3)"));
  EXPECT_TRUE(CheckStrictEqual(Utf16Length(str5), "63"));
  EXPECT_TRUE(CheckStrictEqual(Utf8Length(str5), "126"));

  EXPECT_TRUE(CheckStrictEqual(TestUtf8(str6), "str6"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf16(str6), "str6"));
  EXPECT_TRUE(CheckStrictEqual(TestUtf8Insufficient(str6), "str6.slice(0, 1)"));
  EXPECT_TRUE(
      CheckStrictEqual(TestUtf16Insufficient(str6), "str6.slice(0, 3)"));
  EXPECT_TRUE(CheckStrictEqual(Utf16Length(str6), "5"));
  EXPECT_TRUE(CheckStrictEqual(Utf8Length(str6), "14"));
}

TEST_P(NapiTest, ArrayTest) {
  Eval(R"(
    array = [
      1,
      9,
      48,
      13493,
      9459324,
      { name: 'hello' },
      [
        'world',
        'node',
        'abi'
      ]
    ];
  )");

  napi_value undefined{}, global{}, array{}, element{}, newArray{}, array2{},
      valueFive{};
  napi_valuetype elementType{};
  bool isArray{}, hasElement{}, isDeleted{};
  uint32_t arrayLength{};

  EXPECT_NAPI_OK(napi_get_undefined(env, &undefined));
  EXPECT_NAPI_OK(napi_get_global(env, &global));
  EXPECT_NAPI_OK(napi_get_named_property(env, global, "array", &array));

  EXPECT_NAPI_OK(napi_is_array(env, array, &isArray));
  EXPECT_TRUE(isArray);

  EXPECT_NAPI_OK(napi_get_array_length(env, array, &arrayLength));
  EXPECT_EQ(arrayLength, 7u);

  EXPECT_NAPI_OK(napi_get_element(env, array, arrayLength, &element));
  EXPECT_NAPI_OK(napi_typeof(env, element, &elementType));
  EXPECT_EQ(elementType, napi_valuetype::napi_undefined);

  for (uint32_t i = 0; i < arrayLength; ++i) {
    EXPECT_NAPI_OK(napi_get_element(env, array, i, &element));
    EXPECT_NAPI_OK(napi_typeof(env, element, &elementType));
    EXPECT_NE(elementType, napi_valuetype::napi_undefined);
    EXPECT_TRUE(CheckStrictEqual(element, "array[" + std::to_string(i) + "]"));
  }

  // Clone the array.
  EXPECT_NAPI_OK(napi_create_array(env, &newArray));
  for (uint32_t i = 0; i < arrayLength; ++i) {
    EXPECT_NAPI_OK(napi_get_element(env, array, i, &element));
    EXPECT_NAPI_OK(napi_set_element(env, newArray, i, element));
  }

  // See if all elements of the new array are the same as the old one.
  EXPECT_TRUE(CallBoolFunction({newArray}, R"JS(
    function(newArray) {
      if (array.length !== newArray.length) {
        return false;
      }
      for (let i = 0; i < array.length; ++i) {
        if (array[i] !== newArray[i]) {
          return false;
        }
      }
      return true;
    })JS"));

  EXPECT_NAPI_OK(napi_has_element(env, array, 0, &hasElement));
  EXPECT_TRUE(hasElement);
  EXPECT_NAPI_OK(napi_has_element(env, array, arrayLength, &hasElement));
  EXPECT_FALSE(hasElement);

  EXPECT_NAPI_OK(napi_create_array_with_length(env, 0, &newArray));
  EXPECT_TRUE(CallBoolFunction(
      {newArray}, "function(newArray) { return newArray instanceof Array; }"));
  EXPECT_NAPI_OK(napi_create_array_with_length(env, 1, &newArray));
  EXPECT_TRUE(CallBoolFunction(
      {newArray}, "function(newArray) { return newArray instanceof Array; }"));
  // Check max allowed length for an array 2^32 -1
  EXPECT_NAPI_OK(napi_create_array_with_length(env, 4294967295, &newArray));
  EXPECT_TRUE(CallBoolFunction(
      {newArray}, "function(newArray) { return newArray instanceof Array; }"));

  // Verify that array elements can be deleted.
  array2 = Eval("array2 = ['a', 'b', 'c', 'd']");
  EXPECT_TRUE(CallBoolFunction(
      {array2}, "function(array2) { return array2.length == 4; }"));
  EXPECT_TRUE(
      CallBoolFunction({array2}, "function(array2) { return 2 in array2; }"));

  EXPECT_NAPI_OK(napi_delete_element(env, array2, 2, nullptr));

  EXPECT_TRUE(CallBoolFunction(
      {array2}, "function(array2) { return array2.length == 4; }"));
  EXPECT_TRUE(CallBoolFunction(
      {array2}, "function(array2) { return !(2 in array2); }"));

  EXPECT_NAPI_OK(napi_delete_element(env, array2, 1, &isDeleted));
  EXPECT_TRUE(isDeleted);
  EXPECT_NAPI_OK(napi_delete_element(env, array2, 1, &isDeleted));
  EXPECT_TRUE(
      isDeleted); // deletion succeeded as long as the element is undefined.

  EXPECT_TRUE(
      CallFunction({array2}, "function(array2) { Object.freeze(array2); }"));

  EXPECT_NAPI_OK(napi_delete_element(env, array2, 0, &isDeleted));
  EXPECT_FALSE(isDeleted);
  EXPECT_NAPI_OK(napi_delete_element(env, array2, 1, &isDeleted));
  EXPECT_TRUE(
      isDeleted); // deletion succeeded as long as the element is undefined.

  // Check when (index > int32) max(int32) + 2 = 2,147,483,650
  EXPECT_NAPI_OK(napi_create_int32(env, 5, &valueFive));
  EXPECT_NAPI_OK(napi_set_element(env, array, 2'147'483'650u, valueFive));
  EXPECT_TRUE(CheckStrictEqual(valueFive, "array[2147483650]"));

  EXPECT_NAPI_OK(napi_has_element(env, array, 2'147'483'650u, &hasElement));
  EXPECT_TRUE(hasElement);

  EXPECT_NAPI_OK(napi_get_element(env, array, 2'147'483'650u, &element));
  EXPECT_TRUE(CheckStrictEqual(element, "5"));

  EXPECT_NAPI_OK(napi_delete_element(env, array, 2'147'483'650u, &isDeleted));
  EXPECT_TRUE(isDeleted);
  EXPECT_TRUE(CheckStrictEqual(undefined, "array[2147483650]"));
}

TEST_P(NapiTest, SymbolTest) {
  auto New = [&](const char *value = nullptr) {
    napi_value description{}, symbol{};
    if (value) {
      EXPECT_NAPI_OK(
          napi_create_string_utf8(env, value, NAPI_AUTO_LENGTH, &description));
    }
    EXPECT_NAPI_OK(napi_create_symbol(env, description, &symbol));
    return symbol;
  };

  const napi_value sym = New("test");
  EXPECT_TRUE(CallBoolFunction(
      {sym}, "function(sym) { return sym.toString() === 'Symbol(test)'; }"));

  const napi_value fooSym = New("foo");
  const napi_value otherSym = New("bar");
  CallFunction({fooSym, otherSym}, R"(
    function(fooSym, otherSym) {
      myObj = {};
      myObj.foo = 'bar';
      myObj[fooSym] = 'baz';
      myObj[otherSym] = 'bing';
    })");
  EXPECT_TRUE(
      CallBoolFunction({}, "function() { return myObj.foo === 'bar'; }"));
  EXPECT_TRUE(CallBoolFunction(
      {fooSym}, "function(fooSym) { return myObj[fooSym] === 'baz'; }"));
  EXPECT_TRUE(CallBoolFunction(
      {otherSym}, "function(otherSym) { return myObj[otherSym] === 'bing'; }"));
  EXPECT_TRUE(CallBoolFunction(
      {otherSym}, "function(otherSym) { return myObj[otherSym] === 'bing'; }"));

  const napi_value sym1 = New();
  const napi_value sym2 = New();
  EXPECT_TRUE(CallBoolFunction(
      {sym1, sym2}, "function(sym1, sym2) { return sym1 !== sym2; }"));
  const napi_value fooSym1 = New("foo");
  const napi_value fooSym2 = New("foo");
  EXPECT_TRUE(CallBoolFunction(
      {fooSym1, fooSym2}, "function(sym1, sym2) { return sym1 !== sym2; }"));
  const napi_value barSym = New("bar");
  EXPECT_TRUE(CallBoolFunction(
      {fooSym1, barSym}, "function(sym1, sym2) { return sym1 !== sym2; }"));
}

TEST_P(NapiTest, ObjectTest) {
  int test_value = 3;

  auto New = [&]() {
    napi_value result = CreateObject();
    SetNamedProperty(result, "test_number", CreateInt32(987654321));
    SetNamedProperty(result, "test_string", CreateStringUtf8("test string"));
    return result;
  };

  auto Inflate = [&](napi_value obj) {
    napi_value propertyNames = GetPropertyNames(obj);
    uint32_t length = GetArrayLength(propertyNames);
    for (uint32_t i = 0; i < length; i++) {
      napi_value propertyName = GetElement(propertyNames, i);
      napi_value value = GetProperty(obj, propertyName);
      SetProperty(obj, propertyName, CreateDouble(GetValueDouble(value) + 1));
    }

    return obj;
  };

  auto Wrap = [&](napi_value obj) {
    EXPECT_NAPI_OK(napi_wrap(env, obj, &test_value, nullptr, nullptr, nullptr));
  };

  auto Unwrap = [&](napi_value obj) {
    void *data{};
    EXPECT_NAPI_OK(napi_unwrap(env, obj, &data));

    bool is_expected = (data != nullptr && *(int *)data == 3);
    return is_expected;
  };

  auto TestSetProperty = [&]() {
    napi_status status{};
    napi_value key{};

    napi_value object = CreateObject();
    napi_value value = CreateObject();
    EXPECT_NAPI_OK(napi_create_string_utf8(env, "", NAPI_AUTO_LENGTH, &key));

    status = napi_set_property(nullptr, object, key, value);

    add_returned_status(
        env, "envIsNull", object, "Invalid argument", napi_invalid_arg, status);

    napi_set_property(env, nullptr, key, value);

    add_last_status(env, "objectIsNull", object);

    napi_set_property(env, object, nullptr, value);

    add_last_status(env, "keyIsNull", object);

    napi_set_property(env, object, key, nullptr);

    add_last_status(env, "valueIsNull", object);

    return object;
  };

  auto TestHasProperty = [&]() -> napi_value {
    napi_status status;
    napi_value object, key;
    bool result;

    NAPI_CALL(env, napi_create_object(env, &object));

    NAPI_CALL(env, napi_create_string_utf8(env, "", NAPI_AUTO_LENGTH, &key));

    status = napi_has_property(nullptr, object, key, &result);

    add_returned_status(
        env, "envIsNull", object, "Invalid argument", napi_invalid_arg, status);

    napi_has_property(env, nullptr, key, &result);

    add_last_status(env, "objectIsNull", object);

    napi_has_property(env, object, nullptr, &result);

    add_last_status(env, "keyIsNull", object);

    napi_has_property(env, object, key, nullptr);

    add_last_status(env, "resultIsNull", object);

    return object;
  };

  auto TestGetProperty = [&]() -> napi_value {
    napi_status status;
    napi_value object, key, result;

    NAPI_CALL(env, napi_create_object(env, &object));

    NAPI_CALL(env, napi_create_string_utf8(env, "", NAPI_AUTO_LENGTH, &key));

    NAPI_CALL(env, napi_create_object(env, &result));

    status = napi_get_property(nullptr, object, key, &result);

    add_returned_status(
        env, "envIsNull", object, "Invalid argument", napi_invalid_arg, status);

    napi_get_property(env, nullptr, key, &result);

    add_last_status(env, "objectIsNull", object);

    napi_get_property(env, object, nullptr, &result);

    add_last_status(env, "keyIsNull", object);

    napi_get_property(env, object, key, nullptr);

    add_last_status(env, "resultIsNull", object);

    return object;
  };

  auto NullSetProperty = [&]() {
    napi_value return_value = CreateObject();
    napi_value object = CreateObject();
    napi_value key = CreateStringUtf8("someString");

    add_returned_status(
        env,
        "envIsNull",
        return_value,
        "Invalid argument",
        napi_invalid_arg,
        napi_set_property(nullptr, object, key, object));

    napi_set_property(env, nullptr, key, object);
    add_last_status(env, "objectIsNull", return_value);

    napi_set_property(env, object, nullptr, object);
    add_last_status(env, "keyIsNull", return_value);

    napi_set_property(env, object, key, nullptr);
    add_last_status(env, "valueIsNull", return_value);

    return return_value;
  };

  auto NullGetProperty = [&]() {
    napi_value prop;

    napi_value return_value = CreateObject();
    napi_value object = CreateObject();
    napi_value key = CreateStringUtf8("someString");

    add_returned_status(
        env,
        "envIsNull",
        return_value,
        "Invalid argument",
        napi_invalid_arg,
        napi_get_property(nullptr, object, key, &prop));

    napi_get_property(env, nullptr, key, &prop);
    add_last_status(env, "objectIsNull", return_value);

    napi_get_property(env, object, nullptr, &prop);
    add_last_status(env, "keyIsNull", return_value);

    napi_get_property(env, object, key, nullptr);
    add_last_status(env, "valueIsNull", return_value);

    return return_value;
  };

  auto NullTestBoolValuedPropApi =
      [&](napi_status (*api)(napi_env, napi_value, napi_value, bool *)) {
        bool result;

        napi_value return_value = CreateObject();
        napi_value object = CreateObject();
        napi_value key = CreateStringUtf8("someString");

        add_returned_status(
            env,
            "envIsNull",
            return_value,
            "Invalid argument",
            napi_invalid_arg,
            api(nullptr, object, key, &result));

        api(env, nullptr, key, &result);
        add_last_status(env, "objectIsNull", return_value);

        api(env, object, nullptr, &result);
        add_last_status(env, "keyIsNull", return_value);

        api(env, object, key, nullptr);
        add_last_status(env, "valueIsNull", return_value);

        return return_value;
      };

  auto NullHasProperty = [&]() {
    return NullTestBoolValuedPropApi(napi_has_property);
  };

  auto NullHasOwnProperty = [&]() {
    return NullTestBoolValuedPropApi(napi_has_own_property);
  };

  auto NullDeleteProperty = [&]() {
    return NullTestBoolValuedPropApi(napi_delete_property);
  };

  auto NullSetNamedProperty = [&]() {
    napi_value return_value = CreateObject();
    napi_value object = CreateObject();

    add_returned_status(
        env,
        "envIsNull",
        return_value,
        "Invalid argument",
        napi_invalid_arg,
        napi_set_named_property(nullptr, object, "key", object));

    napi_set_named_property(env, nullptr, "key", object);
    add_last_status(env, "objectIsNull", return_value);

    napi_set_named_property(env, object, nullptr, object);
    add_last_status(env, "keyIsNull", return_value);

    napi_set_named_property(env, object, "key", nullptr);
    add_last_status(env, "valueIsNull", return_value);

    return return_value;
  };

  auto NullGetNamedProperty = [&]() {
    napi_value prop;

    napi_value return_value = CreateObject();
    napi_value object = CreateObject();

    add_returned_status(
        env,
        "envIsNull",
        return_value,
        "Invalid argument",
        napi_invalid_arg,
        napi_get_named_property(nullptr, object, "key", &prop));

    napi_get_named_property(env, nullptr, "key", &prop);
    add_last_status(env, "objectIsNull", return_value);

    napi_get_named_property(env, object, nullptr, &prop);
    add_last_status(env, "keyIsNull", return_value);

    napi_get_named_property(env, object, "key", nullptr);
    add_last_status(env, "valueIsNull", return_value);

    return return_value;
  };

  auto NullHasNamedProperty = [&]() {
    bool result;

    napi_value return_value = CreateObject();
    napi_value object = CreateObject();

    add_returned_status(
        env,
        "envIsNull",
        return_value,
        "Invalid argument",
        napi_invalid_arg,
        napi_has_named_property(nullptr, object, "key", &result));

    napi_has_named_property(env, nullptr, "key", &result);
    add_last_status(env, "objectIsNull", return_value);

    napi_has_named_property(env, object, nullptr, &result);
    add_last_status(env, "keyIsNull", return_value);

    napi_has_named_property(env, object, "key", nullptr);
    add_last_status(env, "valueIsNull", return_value);

    return return_value;
  };

  auto NullSetElement = [&]() {
    napi_value return_value = CreateObject();
    napi_value object = CreateObject();

    add_returned_status(
        env,
        "envIsNull",
        return_value,
        "Invalid argument",
        napi_invalid_arg,
        napi_set_element(nullptr, object, 0, object));

    napi_set_element(env, nullptr, 0, object);
    add_last_status(env, "objectIsNull", return_value);

    napi_set_property(env, object, 0, nullptr);
    add_last_status(env, "valueIsNull", return_value);

    return return_value;
  };

  auto NullGetElement = [&]() {
    napi_value prop;

    napi_value return_value = CreateObject();
    napi_value object = CreateObject();

    add_returned_status(
        env,
        "envIsNull",
        return_value,
        "Invalid argument",
        napi_invalid_arg,
        napi_get_element(nullptr, object, 0, &prop));

    napi_get_property(env, nullptr, 0, &prop);
    add_last_status(env, "objectIsNull", return_value);

    napi_get_property(env, object, 0, nullptr);
    add_last_status(env, "valueIsNull", return_value);

    return return_value;
  };

  auto NullTestBoolValuedElementApi =
      [&](napi_status (*api)(napi_env, napi_value, uint32_t, bool *)) {
        bool result;

        napi_value return_value = CreateObject();
        napi_value object = CreateObject();

        add_returned_status(
            env,
            "envIsNull",
            return_value,
            "Invalid argument",
            napi_invalid_arg,
            api(nullptr, object, 0, &result));

        api(env, nullptr, 0, &result);
        add_last_status(env, "objectIsNull", return_value);

        api(env, object, 0, nullptr);
        add_last_status(env, "valueIsNull", return_value);

        return return_value;
      };

  auto NullHasElement = [&]() {
    return NullTestBoolValuedElementApi(napi_has_element);
  };

  auto NullDeleteElement = [&]() {
    return NullTestBoolValuedElementApi(napi_delete_element);
  };

  auto NullDefineProperties = [&]() {
    auto defineProperties = [](napi_env /*env*/,
                               napi_callback_info /*info*/) -> napi_value {
      return nullptr;
    };

    napi_property_descriptor desc = {
        "prop",
        nullptr,
        defineProperties,
        nullptr,
        nullptr,
        nullptr,
        napi_enumerable,
        nullptr};

    napi_value return_value = CreateObject();
    napi_value object = CreateObject();

    add_returned_status(
        env,
        "envIsNull",
        return_value,
        "Invalid argument",
        napi_invalid_arg,
        napi_define_properties(nullptr, object, 1, &desc));

    napi_define_properties(env, nullptr, 1, &desc);
    add_last_status(env, "objectIsNull", return_value);

    napi_define_properties(env, object, 1, nullptr);
    add_last_status(env, "descriptorListIsNull", return_value);

    desc.utf8name = nullptr;
    napi_define_properties(env, object, 1, nullptr);
    add_last_status(env, "utf8nameIsNull", return_value);
    desc.utf8name = "prop";

    desc.method = nullptr;
    napi_define_properties(env, object, 1, nullptr);
    add_last_status(env, "methodIsNull", return_value);
    desc.method = defineProperties;

    return return_value;
  };

  auto NullGetPropertyNames = [&]() {
    napi_value props;

    napi_value return_value = CreateObject();

    add_returned_status(
        env,
        "envIsNull",
        return_value,
        "Invalid argument",
        napi_invalid_arg,
        napi_get_property_names(nullptr, return_value, &props));

    napi_get_property_names(env, nullptr, &props);
    add_last_status(env, "objectIsNull", return_value);

    napi_get_property_names(env, return_value, nullptr);
    add_last_status(env, "valueIsNull", return_value);

    return return_value;
  };

  auto NullGetAllPropertyNames = [&]() {
    napi_value props;

    napi_value return_value = CreateObject();

    add_returned_status(
        env,
        "envIsNull",
        return_value,
        "Invalid argument",
        napi_invalid_arg,
        napi_get_all_property_names(
            nullptr,
            return_value,
            napi_key_own_only,
            napi_key_writable,
            napi_key_keep_numbers,
            &props));

    napi_get_all_property_names(
        env,
        nullptr,
        napi_key_own_only,
        napi_key_writable,
        napi_key_keep_numbers,
        &props);
    add_last_status(env, "objectIsNull", return_value);

    napi_get_all_property_names(
        env,
        return_value,
        napi_key_own_only,
        napi_key_writable,
        napi_key_keep_numbers,
        nullptr);
    add_last_status(env, "valueIsNull", return_value);

    return return_value;
  };

  auto NullGetPrototype = [&]() {
    napi_value proto;

    napi_value return_value = CreateObject();

    add_returned_status(
        env,
        "envIsNull",
        return_value,
        "Invalid argument",
        napi_invalid_arg,
        napi_get_prototype(nullptr, return_value, &proto));

    napi_get_prototype(env, nullptr, &proto);
    add_last_status(env, "objectIsNull", return_value);

    napi_get_prototype(env, return_value, nullptr);
    add_last_status(env, "valueIsNull", return_value);

    return return_value;
  };

  // We create two type tags. They are basically 128-bit UUIDs.
  const napi_type_tag typeTags[2] = {
      {0xdaf987b3cc62481a, 0xb745b0497f299531},
      {0xbb7936c374084d9b, 0xa9548d0762eeedb9}};

  auto TypeTaggedInstance = [&](uint32_t typeIndex) {
    napi_value obj = CreateObject();
    EXPECT_NAPI_OK(napi_type_tag_object(env, obj, &typeTags[typeIndex]));
    return obj;
  };

  auto CheckTypeTag = [&](napi_value obj, uint32_t typeIndex) {
    bool result{};
    EXPECT_NAPI_OK(
        napi_check_object_type_tag(env, obj, &typeTags[typeIndex], &result));
    return result;
  };

  {
    napi_value object = Eval(R"(object = {
      hello : 'world',
      array : [ 1, 94, 'str', 12.321, {test : 'obj in arr'} ],
      newObject : {test : 'obj in obj'}
    })");

    EXPECT_TRUE(CheckStrictEqual(GetProperty(object, "hello"), "'world'"));
    EXPECT_TRUE(CheckStrictEqual(GetNamedProperty(object, "hello"), "'world'"));
    EXPECT_TRUE(CheckDeepStrictEqual(
        GetProperty(object, "array"),
        "[ 1, 94, 'str', 12.321, {test : 'obj in arr'} ]"));
    EXPECT_TRUE(CheckDeepStrictEqual(
        GetProperty(object, "newObject"), "{test : 'obj in obj'}"));

    EXPECT_TRUE(HasProperty(object, "hello"));
    EXPECT_TRUE(HasNamedProperty(object, "hello"));
    EXPECT_TRUE(HasProperty(object, "array"));
    EXPECT_TRUE(HasProperty(object, "newObject"));

    napi_value newObject = New();
    EXPECT_TRUE(HasProperty(newObject, "test_number"));
    EXPECT_CALL_TRUE({newObject}, "newObject.test_number === 987654321");
    EXPECT_CALL_TRUE({newObject}, "newObject.test_string === 'test string'");
  }

  {
    // Verify that napi_get_property() walks the prototype chain.
    napi_value obj = Eval(R"(
      function MyObject() {
        this.foo = 42;
        this.bar = 43;
      }

      MyObject.prototype.bar = 44;
      MyObject.prototype.baz = 45;

      obj = new MyObject();
      )");

    EXPECT_STRICT_EQ(GetProperty(obj, "foo"), "42");
    EXPECT_STRICT_EQ(GetProperty(obj, "bar"), "43");
    EXPECT_STRICT_EQ(GetProperty(obj, "baz"), "45");
    EXPECT_STRICT_EQ(GetProperty(obj, "toString"), "Object.prototype.toString");
  }

  {
    // Verify that napi_has_own_property() fails if property is not a name.
    napi_value notNames =
        Eval("[ true, false, null, undefined, {}, [], 0, 1, () => {} ]");
    uint32_t notNamesLength = GetArrayLength(notNames);
    for (uint32_t i = 0; i < notNamesLength; ++i) {
      bool value{};
      EXPECT_EQ(
          napi_name_expected,
          napi_has_own_property(
              env, CreateObject(), GetElement(notNames, i), &value));
    }
  }

  {
    // Verify that napi_has_own_property() does not walk the prototype chain.
    napi_value symbol1 = Eval("symbol1 = Symbol()");
    napi_value symbol2 = Eval("symbol2 = Symbol()");

    napi_value obj = Eval(R"(
      function MyObject() {
        this.foo = 42;
        this.bar = 43;
        this[symbol1] = 44;
      }

      MyObject.prototype.bar = 45;
      MyObject.prototype.baz = 46;
      MyObject.prototype[symbol2] = 47;

      obj = new MyObject();
      )");

    EXPECT_TRUE(HasOwnProperty(obj, "foo"));
    EXPECT_TRUE(HasOwnProperty(obj, "bar"));
    EXPECT_TRUE(HasOwnProperty(obj, symbol1));
    EXPECT_FALSE(HasOwnProperty(obj, "baz"));
    EXPECT_FALSE(HasOwnProperty(obj, "toString"));
    EXPECT_FALSE(HasOwnProperty(obj, symbol2));
  }

  {
    // test_object.Inflate increases all properties by 1
    napi_value cube = Eval(R"(cube = {
      x : 10,
      y : 10,
      z : 10
    })");

    EXPECT_DEEP_STRICT_EQ(cube, "{x : 10, y : 10, z : 10}");
    EXPECT_DEEP_STRICT_EQ(Inflate(cube), "{x : 11, y : 11, z : 11}");
    EXPECT_DEEP_STRICT_EQ(Inflate(cube), "{x : 12, y : 12, z : 12}");
    EXPECT_DEEP_STRICT_EQ(Inflate(cube), "{x : 13, y : 13, z : 13}");
    Eval("cube.t = 13");
    EXPECT_DEEP_STRICT_EQ(Inflate(cube), "{x : 14, y : 14, z : 14, t : 14}");

    napi_value sym1 = Eval("sym1 = Symbol('1')");
    napi_value sym2 = Eval("sym2 = Symbol('2')");
    // TODO: [vmoroz] napi_value sym3 =
    Eval("sym3 = Symbol('3')");
    napi_value sym4 = Eval("sym4 = Symbol('4')");
    napi_value object2 =
        Eval("object2 = {[sym1] : '@@iterator', [sym2] : sym3}");

    EXPECT_TRUE(HasProperty(object2, sym1));
    EXPECT_TRUE(HasProperty(object2, sym2));
    EXPECT_STRICT_EQ(GetProperty(object2, sym1), "'@@iterator'");
    // TODO: [vmoroz] EXPECT_STRICT_EQ(GetProperty(object2, sym2), sym3);
    SetProperty(object2, "string", CreateStringUtf8("value"));
    SetNamedProperty(object2, "named_string", CreateStringUtf8("value"));
    SetProperty(object2, sym4, CreateInt32(123));
    EXPECT_TRUE(HasProperty(object2, "string"));
    EXPECT_TRUE(HasProperty(object2, "named_string"));
    EXPECT_TRUE(HasProperty(object2, sym4));
    EXPECT_STRICT_EQ(GetProperty(object2, "string"), "'value'");
    EXPECT_STRICT_EQ(GetProperty(object2, sym4), "123");
  }

  {
    // Wrap a pointer in a JS object, then verify the pointer can be unwrapped.
    napi_value wrapper = CreateObject();
    Wrap(wrapper);
    EXPECT_TRUE(Unwrap(wrapper));
  }

  {
    // Verify that wrapping doesn't break an object's prototype chain.
    napi_value wrapper = Eval("wrapper = {}");
    // TODO: [vmoroz] napi_value protoA =
    Eval("protoA = {protoA : true}");
    Eval("Object.setPrototypeOf(wrapper, protoA)");
    Wrap(wrapper);

    EXPECT_TRUE(Unwrap(wrapper));
    EXPECT_STRICT_EQ("wrapper.protoA", "true");
  }

  {
    // Verify the pointer can be unwrapped after inserting in the prototype
    // chain.
    napi_value wrapper = Eval("wrapper = {}");
    // TODO: [vmoroz] napi_value protoA =
    Eval("protoA = {protoA : true}");
    Eval("Object.setPrototypeOf(wrapper, protoA)");
    Wrap(wrapper);

    // TODO: [vmoroz] napi_value protoB =
    Eval("protoB = {protoB : true}");
    Eval("Object.setPrototypeOf(protoB, Object.getPrototypeOf(wrapper))");
    Eval("Object.setPrototypeOf(wrapper, protoB)");

    EXPECT_TRUE(Unwrap(wrapper));
    EXPECT_STRICT_EQ("wrapper.protoA", "true");
    EXPECT_STRICT_EQ("wrapper.protoB", "true");
  }

  {
    // Verify that objects can be type-tagged and type-tag-checked.
    napi_value obj1 = TypeTaggedInstance(0);
    napi_value obj2 = TypeTaggedInstance(1);

    // Verify that type tags are correctly accepted.
    EXPECT_TRUE(CheckTypeTag(obj1, 0));
    EXPECT_TRUE(CheckTypeTag(obj2, 1));

    // Verify that wrongly tagged objects are rejected.
    EXPECT_FALSE(CheckTypeTag(obj2, 0));
    EXPECT_FALSE(CheckTypeTag(obj1, 1));

    // Verify that untagged objects are rejected.
    EXPECT_FALSE(CheckTypeTag(CreateObject(), 0));
    EXPECT_FALSE(CheckTypeTag(CreateObject(), 1));
  }

  {
    // Verify that normal and nonexistent properties can be deleted.
    napi_value sym = Eval("sym = Symbol()");
    napi_value obj = Eval("obj = {foo : 'bar', [sym] : 'baz'}");

    EXPECT_STRICT_EQ("'foo' in obj", "true");
    EXPECT_STRICT_EQ("sym in obj", "true");
    EXPECT_STRICT_EQ("'does_not_exist' in obj", "false");
    EXPECT_TRUE(DeleteProperty(obj, "foo"));
    EXPECT_STRICT_EQ("'foo' in obj", "false");
    EXPECT_STRICT_EQ("sym in obj", "true");
    EXPECT_STRICT_EQ("'does_not_exist' in obj", "false");
    EXPECT_TRUE(DeleteProperty(obj, sym));
    EXPECT_STRICT_EQ("'foo' in obj", "false");
    EXPECT_STRICT_EQ("sym in obj", "false");
    EXPECT_STRICT_EQ("'does_not_exist' in obj", "false");
  }

  {
    // Verify that non-configurable properties are not deleted.
    napi_value obj = Eval("obj = {}");

    Eval("Object.defineProperty(obj, 'foo', {configurable : false})");
    EXPECT_FALSE(DeleteProperty(obj, "foo"));
    EXPECT_STRICT_EQ("'foo' in obj", "true");
  }

  {
    // Verify that prototype properties are not deleted.
    napi_value obj = Eval(R"(
      function Foo() {
        this.foo = 'bar';
      }

      Foo.prototype.foo = 'baz';

      obj = new Foo();
    )");

    EXPECT_STRICT_EQ("obj.foo", "'bar'");
    EXPECT_TRUE(DeleteProperty(obj, "foo"));
    EXPECT_STRICT_EQ("obj.foo", "'baz'");
    EXPECT_TRUE(DeleteProperty(obj, "foo"));
    EXPECT_STRICT_EQ("obj.foo", "'baz'");
  }

  {
    // Verify that napi_get_property_names gets the right set of property names,
    // i.e.: includes prototypes, only enumerable properties, skips symbols,
    // and includes indices and converts them to strings.

    napi_value object = Eval("object = Object.create({inherited : 1})");
    napi_value fooSymbol = Eval("fooSymbol = Symbol('foo')");

    Eval(R"(
      object.normal = 2;
      object[fooSymbol] = 3;
      Object.defineProperty(
        object, 'unenumerable', {value : 4, enumerable : false, writable : true, configurable : true});
      object[5] = 5;
    )");

    EXPECT_DEEP_STRICT_EQ(
        GetPropertyNames(object), "[ '5', 'normal', 'inherited' ]");
    EXPECT_DEEP_STRICT_EQ(GetPropertySymbols(object), "[fooSymbol]");
  }

  // Verify that passing nullptr to napi_set_property() results in the correct
  // error.
  EXPECT_DEEP_STRICT_EQ(TestSetProperty(), R"({
    envIsNull : 'Invalid argument',
    objectIsNull : 'Invalid argument',
    keyIsNull : 'Invalid argument',
    valueIsNull : 'Invalid argument'
  })");

  // Verify that passing nullptr to napi_has_property() results in the correct
  // error.
  EXPECT_DEEP_STRICT_EQ(TestHasProperty(), R"({
    envIsNull : 'Invalid argument',
    objectIsNull : 'Invalid argument',
    keyIsNull : 'Invalid argument',
    resultIsNull : 'Invalid argument'
  })");

  // Verify that passing nullptr to napi_get_property() results in the correct
  // error.
  EXPECT_DEEP_STRICT_EQ(TestGetProperty(), R"({
    envIsNull : 'Invalid argument',
    objectIsNull : 'Invalid argument',
    keyIsNull : 'Invalid argument',
    resultIsNull : 'Invalid argument'
  })");

  {
    napi_value obj = Eval("obj = { x: 'a', y: 'b', z: 'c' }");
    ObjectSeal(obj);
    EXPECT_STRICT_EQ("Object.isSealed(obj)", "true");
    EXPECT_JS_THROW("obj.w = 'd'");
    EXPECT_JS_THROW("delete obj.x");

    // Sealed objects allow updating existing properties, so this should not
    // throw.
    Eval("obj.x = 'd'");
  }

  {
    napi_value obj = Eval("obj = { x: 10, y: 10, z: 10 }");
    ObjectFreeze(obj);
    EXPECT_STRICT_EQ("Object.isFrozen(obj)", "true");
    EXPECT_JS_THROW("obj.x = 10");
    EXPECT_JS_THROW("obj.w = 15");
    EXPECT_JS_THROW("delete obj.x");
  }

  {
    // Test passing nullptr to object-related N-APIs.
    napi_value expectedForProperty = Eval(R"(expectedForProperty = {
      envIsNull : 'Invalid argument',
      objectIsNull : 'Invalid argument',
      keyIsNull : 'Invalid argument',
      valueIsNull : 'Invalid argument'
    })");
    EXPECT_DEEP_STRICT_EQ(NullSetProperty(), "expectedForProperty");
    EXPECT_DEEP_STRICT_EQ(NullGetProperty(), "expectedForProperty");
    EXPECT_DEEP_STRICT_EQ(NullHasProperty(), "expectedForProperty");
    EXPECT_DEEP_STRICT_EQ(NullHasOwnProperty(), "expectedForProperty");
    // It's OK not to want the result of a deletion.
    EXPECT_DEEP_STRICT_EQ(
        NullDeleteProperty(),
        "Object.assign({}, expectedForProperty, {valueIsNull : 'napi_ok'})");
    EXPECT_DEEP_STRICT_EQ(NullSetNamedProperty(), "expectedForProperty");
    EXPECT_DEEP_STRICT_EQ(NullGetNamedProperty(), "expectedForProperty");
    EXPECT_DEEP_STRICT_EQ(NullHasNamedProperty(), "expectedForProperty");

    napi_value expectedForElement = Eval(R"(expectedForElement = {
      envIsNull : 'Invalid argument',
      objectIsNull : 'Invalid argument',
      valueIsNull : 'Invalid argument'
    })");
    EXPECT_DEEP_STRICT_EQ(NullSetElement(), "expectedForElement");
    EXPECT_DEEP_STRICT_EQ(NullGetElement(), "expectedForElement");
    EXPECT_DEEP_STRICT_EQ(NullHasElement(), "expectedForElement");
    // It's OK not to want the result of a deletion.
    EXPECT_DEEP_STRICT_EQ(
        NullDeleteElement(),
        "Object.assign({}, expectedForElement, { valueIsNull: 'napi_ok'})");

    EXPECT_DEEP_STRICT_EQ(NullDefineProperties(), R"({
      envIsNull : 'Invalid argument',
      objectIsNull : 'Invalid argument',
      descriptorListIsNull : 'Invalid argument',
      utf8nameIsNull : 'Invalid argument',
      methodIsNull : 'Invalid argument',
    })");

    // `expectedForElement` also works for the APIs below.
    EXPECT_DEEP_STRICT_EQ(NullGetPropertyNames(), "expectedForElement");
    EXPECT_DEEP_STRICT_EQ(NullGetAllPropertyNames(), "expectedForElement");
    EXPECT_DEEP_STRICT_EQ(NullGetPrototype(), "expectedForElement");
  }
}

TEST_P(NapiTest, ConstructorTest) {
  static double value_ = 1;
  static double static_value_ = 10;

  auto TestDefineClass = [](napi_env env,
                            napi_callback_info info) -> napi_value {
    napi_status status;
    napi_value result, return_value;

    auto NullTestDefineClass = [](napi_env /*env*/,
                                  napi_callback_info /*info*/) -> napi_value {
      return nullptr;
    };

    napi_property_descriptor property_descriptor = {
        "TestDefineClass",
        nullptr,
        NullTestDefineClass,
        nullptr,
        nullptr,
        nullptr,
        napi_property_attributes(napi_enumerable | napi_static),
        nullptr};

    NAPI_CALL(env, napi_create_object(env, &return_value));

    status = napi_define_class(
        nullptr,
        "TrackedFunction",
        NAPI_AUTO_LENGTH,
        NullTestDefineClass,
        nullptr,
        1,
        &property_descriptor,
        &result);

    add_returned_status(
        env,
        "envIsNull",
        return_value,
        "Invalid argument",
        napi_invalid_arg,
        status);

    napi_define_class(
        env,
        nullptr,
        NAPI_AUTO_LENGTH,
        NullTestDefineClass,
        nullptr,
        1,
        &property_descriptor,
        &result);
    add_last_status(env, "nameIsNull", return_value);

    napi_define_class(
        env,
        "TrackedFunction",
        NAPI_AUTO_LENGTH,
        nullptr,
        nullptr,
        1,
        &property_descriptor,
        &result);
    add_last_status(env, "cbIsNull", return_value);

    napi_define_class(
        env,
        "TrackedFunction",
        NAPI_AUTO_LENGTH,
        NullTestDefineClass,
        nullptr,
        1,
        &property_descriptor,
        &result);
    add_last_status(env, "cbDataIsNull", return_value);

    napi_define_class(
        env,
        "TrackedFunction",
        NAPI_AUTO_LENGTH,
        NullTestDefineClass,
        nullptr,
        1,
        nullptr,
        &result);
    add_last_status(env, "propertiesIsNull", return_value);

    napi_define_class(
        env,
        "TrackedFunction",
        NAPI_AUTO_LENGTH,
        NullTestDefineClass,
        nullptr,
        1,
        &property_descriptor,
        nullptr);
    add_last_status(env, "resultIsNull", return_value);

    return return_value;
  };

  auto GetValue = [](napi_env env, napi_callback_info info) -> napi_value {
    size_t argc = 0;
    NAPI_CALL(
        env, napi_get_cb_info(env, info, &argc, nullptr, nullptr, nullptr));

    NAPI_ASSERT(env, argc == 0, "Wrong number of arguments");

    napi_value number;
    NAPI_CALL(env, napi_create_double(env, value_, &number));

    return number;
  };

  auto SetValue = [](napi_env env, napi_callback_info info) -> napi_value {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    NAPI_ASSERT(env, argc == 1, "Wrong number of arguments");

    NAPI_CALL(env, napi_get_value_double(env, args[0], &value_));

    return nullptr;
  };

  auto Echo = [](napi_env env, napi_callback_info info) -> napi_value {
    size_t argc = 1;
    napi_value args[1] = {};
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    NAPI_ASSERT(env, argc == 1, "Wrong number of arguments");

    return args[0];
  };

  auto New = [](napi_env env, napi_callback_info info) -> napi_value {
    napi_value _this;
    NAPI_CALL(
        env, napi_get_cb_info(env, info, nullptr, nullptr, &_this, nullptr));

    return _this;
  };

  auto GetStaticValue = [](napi_env env,
                           napi_callback_info info) -> napi_value {
    size_t argc = 0;
    NAPI_CALL(
        env, napi_get_cb_info(env, info, &argc, nullptr, nullptr, nullptr));

    NAPI_ASSERT(env, argc == 0, "Wrong number of arguments");

    napi_value number;
    NAPI_CALL(env, napi_create_double(env, static_value_, &number));

    return number;
  };

  auto NewExtra = [](napi_env env, napi_callback_info info) -> napi_value {
    napi_value thisArg{};
    NAPI_CALL(
        env, napi_get_cb_info(env, info, nullptr, nullptr, &thisArg, nullptr));
    return thisArg;
  };

  napi_value cons =
      DefineClass("MyObject_Extra", 8, NewExtra, nullptr, 0, nullptr);

  napi_value number = CreateDouble(1);

  napi_property_descriptor properties[] = {
      {"echo",
       nullptr,
       Echo,
       nullptr,
       nullptr,
       nullptr,
       napi_enumerable,
       nullptr},
      {"readwriteValue",
       nullptr,
       nullptr,
       nullptr,
       nullptr,
       number,
       napi_property_attributes(napi_enumerable | napi_writable),
       nullptr},
      {"readonlyValue",
       nullptr,
       nullptr,
       nullptr,
       nullptr,
       number,
       napi_enumerable,
       nullptr},
      {"hiddenValue",
       nullptr,
       nullptr,
       nullptr,
       nullptr,
       number,
       napi_default,
       nullptr},
      {"readwriteAccessor1",
       nullptr,
       nullptr,
       GetValue,
       SetValue,
       nullptr,
       napi_default,
       nullptr},
      {"readwriteAccessor2",
       nullptr,
       nullptr,
       GetValue,
       SetValue,
       nullptr,
       napi_writable,
       nullptr},
      {"readonlyAccessor1",
       nullptr,
       nullptr,
       GetValue,
       nullptr,
       nullptr,
       napi_default,
       nullptr},
      {"readonlyAccessor2",
       nullptr,
       nullptr,
       GetValue,
       nullptr,
       nullptr,
       napi_writable,
       nullptr},
      {"staticReadonlyAccessor1",
       nullptr,
       nullptr,
       GetStaticValue,
       nullptr,
       nullptr,
       napi_property_attributes(napi_default | napi_static),
       nullptr},
      {"constructorName",
       nullptr,
       nullptr,
       nullptr,
       nullptr,
       cons,
       napi_property_attributes(napi_enumerable | napi_static),
       nullptr},
      {"TestDefineClass",
       nullptr,
       TestDefineClass,
       nullptr,
       nullptr,
       nullptr,
       napi_property_attributes(napi_enumerable | napi_static),
       nullptr},
  };

  cons = DefineClass(
      "MyObject",
      NAPI_AUTO_LENGTH,
      New,
      nullptr,
      sizeof(properties) / sizeof(*properties),
      properties);

  // Testing api calls for a constructor that defines properties
  napi_value TestConstructor =
      CallFunction({cons}, "(cons) => TestConstructor = cons");
  napi_value test_object = Eval("test_object = new TestConstructor()");

  EXPECT_STRICT_EQ("test_object.echo('hello')", "'hello'");

  Eval("test_object.readwriteValue = 1");
  EXPECT_STRICT_EQ("test_object.readwriteValue", "1");
  Eval("test_object.readwriteValue = 2");
  EXPECT_STRICT_EQ("test_object.readwriteValue", "2");

  EXPECT_JS_THROW("test_object.readonlyValue = 3");

  EXPECT_JS_TRUE("test_object.hiddenValue");

  // Properties with napi_enumerable attribute should be enumerable.
  Eval(R"(
    propertyNames = [];
    for (const name in test_object) {
      propertyNames.push(name);
    })");

  EXPECT_JS_TRUE("propertyNames.includes('echo')");
  EXPECT_JS_TRUE("propertyNames.includes('readwriteValue')");
  EXPECT_JS_TRUE("propertyNames.includes('readonlyValue')");
  EXPECT_JS_TRUE("!propertyNames.includes('hiddenValue')");
  EXPECT_JS_TRUE("!propertyNames.includes('readwriteAccessor1')");
  EXPECT_JS_TRUE("!propertyNames.includes('readwriteAccessor2')");
  EXPECT_JS_TRUE("!propertyNames.includes('readonlyAccessor1')");
  EXPECT_JS_TRUE("!propertyNames.includes('readonlyAccessor2')");

  // The napi_writable attribute should be ignored for accessors.
  Eval("test_object.readwriteAccessor1 = 1");
  EXPECT_STRICT_EQ("test_object.readwriteAccessor1", "1");
  EXPECT_STRICT_EQ("test_object.readonlyAccessor1", "1");
  EXPECT_JS_THROW("test_object.readonlyAccessor1 = 3");
  Eval("test_object.readwriteAccessor2 = 2");
  EXPECT_STRICT_EQ("test_object.readwriteAccessor2", "2");
  EXPECT_STRICT_EQ("test_object.readonlyAccessor2", "2");
  EXPECT_JS_THROW("test_object.readonlyAccessor2 = 3");

  // Validate that static properties are on the class as opposed to the
  // instance.
  EXPECT_STRICT_EQ("TestConstructor.staticReadonlyAccessor1", "10");
  EXPECT_STRICT_EQ("test_object.staticReadonlyAccessor1", "undefined");

  // Verify that passing NULL to napi_define_class() results in the correct
  // error.
  EXPECT_DEEP_STRICT_EQ("TestConstructor.TestDefineClass()", R"({
    envIsNull: 'Invalid argument',
    nameIsNull: 'Invalid argument',
    cbIsNull: 'Invalid argument',
    cbDataIsNull: 'napi_ok',
    propertiesIsNull: 'Invalid argument',
    resultIsNull: 'Invalid argument'
  })");
}

TEST_P(NapiTest, ConversionsTest) {
  const char *boolExpected = "/boolean was expected/";
  const char *numberExpected = "/number was expected/";
  const char *stringExpected = "/string was expected/";

  napi_value testSym = Eval("testSym = Symbol('test')");
  EXPECT_STRICT_EQ(AsBool(Eval("false")), "false");
  EXPECT_STRICT_EQ(AsBool(Eval("true")), "true");
  // EXPECT_JS_THROW_MSG(AsBool(Eval("undefined")), boolExpected);
  // assert.throws(() => test.asBool(null), boolExpected);
  // assert.throws(() => test.asBool(Number.NaN), boolExpected);
  // assert.throws(() => test.asBool(0), boolExpected);
  // assert.throws(() => test.asBool(''), boolExpected);
  // assert.throws(() => test.asBool('0'), boolExpected);
  // assert.throws(() => test.asBool(1), boolExpected);
  // assert.throws(() => test.asBool('1'), boolExpected);
  // assert.throws(() => test.asBool('true'), boolExpected);
  // assert.throws(() => test.asBool({}), boolExpected);
  // assert.throws(() => test.asBool([]), boolExpected);
  // assert.throws(() => test.asBool(testSym), boolExpected);
  //
  //[test.asInt32, test.asUInt32, test.asInt64].forEach((asInt) => {
  //  assert.strictEqual(asInt(0), 0);
  //  assert.strictEqual(asInt(1), 1);
  //  assert.strictEqual(asInt(1.0), 1);
  //  assert.strictEqual(asInt(1.1), 1);
  //  assert.strictEqual(asInt(1.9), 1);
  //  assert.strictEqual(asInt(0.9), 0);
  //  assert.strictEqual(asInt(999.9), 999);
  //  assert.strictEqual(asInt(Number.NaN), 0);
  //  assert.throws(() => asInt(undefined), numberExpected);
  //  assert.throws(() => asInt(null), numberExpected);
  //  assert.throws(() => asInt(false), numberExpected);
  //  assert.throws(() => asInt(''), numberExpected);
  //  assert.throws(() => asInt('1'), numberExpected);
  //  assert.throws(() => asInt({}), numberExpected);
  //  assert.throws(() => asInt([]), numberExpected);
  //  assert.throws(() => asInt(testSym), numberExpected);
  //});

  EXPECT_STRICT_EQ(AsInt32(Eval("-1")), "-1");
  EXPECT_STRICT_EQ(AsInt64(Eval("-1")), "-1");
  EXPECT_STRICT_EQ(AsUInt32(Eval("-1")), "Math.pow(2, 32) - 1");

  EXPECT_STRICT_EQ(AsDouble(Eval("0")), "0");
  EXPECT_STRICT_EQ(AsDouble(Eval("1")), "1");
  EXPECT_STRICT_EQ(AsDouble(Eval("1.0")), "1.0");
  EXPECT_STRICT_EQ(AsDouble(Eval("1.1")), "1.1");
  EXPECT_STRICT_EQ(AsDouble(Eval("1.9")), "1.9");
  EXPECT_STRICT_EQ(AsDouble(Eval("0.9")), "0.9");
  EXPECT_STRICT_EQ(AsDouble(Eval("999.9")), "999.9");
  EXPECT_STRICT_EQ(AsDouble(Eval("-1")), "-1");
  //EXPECT_TRUE(isnan(AsDouble(Eval("Number.NaN"))));
  // assert.throws(() => test.asDouble(undefined), numberExpected);
  // assert.throws(() => test.asDouble(null), numberExpected);
  // assert.throws(() => test.asDouble(false), numberExpected);
  // assert.throws(() => test.asDouble(''), numberExpected);
  // assert.throws(() => test.asDouble('1'), numberExpected);
  // assert.throws(() => test.asDouble({}), numberExpected);
  // assert.throws(() => test.asDouble([]), numberExpected);
  // assert.throws(() => test.asDouble(testSym), numberExpected);
  //
  EXPECT_STRICT_EQ(AsString(Eval("''")), "''");
  EXPECT_STRICT_EQ(AsString(Eval("'test'")), "'test'");
  // assert.throws(() => test.asString(undefined), stringExpected);
  // assert.throws(() => test.asString(null), stringExpected);
  // assert.throws(() => test.asString(false), stringExpected);
  // assert.throws(() => test.asString(1), stringExpected);
  // assert.throws(() => test.asString(1.1), stringExpected);
  // assert.throws(() => test.asString(Number.NaN), stringExpected);
  // assert.throws(() => test.asString({}), stringExpected);
  // assert.throws(() => test.asString([]), stringExpected);
  // assert.throws(() => test.asString(testSym), stringExpected);
  //
  EXPECT_STRICT_EQ(ToBool(Eval("true")), "true");
  EXPECT_STRICT_EQ(ToBool(Eval("1")), "true");
  EXPECT_STRICT_EQ(ToBool(Eval("-1")), "true");
  EXPECT_STRICT_EQ(ToBool(Eval("'true'")), "true");
  EXPECT_STRICT_EQ(ToBool(Eval("'false'")), "true");
  EXPECT_STRICT_EQ(ToBool(Eval("a = {}")), "true");
  EXPECT_STRICT_EQ(ToBool(Eval("[]")), "true");
  EXPECT_STRICT_EQ(ToBool(Eval("testSym")), "true");
  EXPECT_STRICT_EQ(ToBool(Eval("false")), "false");
  EXPECT_STRICT_EQ(ToBool(Eval("undefined")), "false");
  EXPECT_STRICT_EQ(ToBool(Eval("null")), "false");
  EXPECT_STRICT_EQ(ToBool(Eval("0")), "false");
  EXPECT_STRICT_EQ(ToBool(Eval("Number.NaN")), "false");
  EXPECT_STRICT_EQ(ToBool(Eval("''")), "false");

  EXPECT_STRICT_EQ(ToNumber(Eval("0")), "0");
  EXPECT_STRICT_EQ(ToNumber(Eval("1")), "1");
  EXPECT_STRICT_EQ(ToNumber(Eval("1.1")), "1.1");
  EXPECT_STRICT_EQ(ToNumber(Eval("-1")), "-1");
  EXPECT_STRICT_EQ(ToNumber(Eval("'0'")), "0");
  EXPECT_STRICT_EQ(ToNumber(Eval("'1'")), "1");
  EXPECT_STRICT_EQ(ToNumber(Eval("'1.1'")), "1.1");
  EXPECT_STRICT_EQ(ToNumber(Eval("[]")), "0");
  EXPECT_STRICT_EQ(ToNumber(Eval("false")), "0");
  EXPECT_STRICT_EQ(ToNumber(Eval("null")), "0");
  EXPECT_STRICT_EQ(ToNumber(Eval("''")), "0");
  // assert.ok(Number.isNaN(test.toNumber(Number.NaN)));
  // assert.ok(Number.isNaN(test.toNumber({})));
  // assert.ok(Number.isNaN(test.toNumber(undefined)));
  // assert.throws(() => test.toNumber(testSym), TypeError);
  //
  // assert.deepStrictEqual({}, test.toObject({}));
  // assert.deepStrictEqual({ 'test': 1 }, test.toObject({ 'test': 1 }));
  // assert.deepStrictEqual([], test.toObject([]));
  // assert.deepStrictEqual([ 1, 2, 3 ], test.toObject([ 1, 2, 3 ]));
  // assert.deepStrictEqual(new Boolean(false), test.toObject(false));
  // assert.deepStrictEqual(new Boolean(true), test.toObject(true));
  // assert.deepStrictEqual(new String(''), test.toObject(''));
  // assert.deepStrictEqual(new Number(0), test.toObject(0));
  // assert.deepStrictEqual(new Number(Number.NaN), test.toObject(Number.NaN));
  // assert.deepStrictEqual(new Object(testSym), test.toObject(testSym));
  // assert.notDeepStrictEqual(test.toObject(false), false);
  // assert.notDeepStrictEqual(test.toObject(true), true);
  // assert.notDeepStrictEqual(test.toObject(''), '');
  // assert.notDeepStrictEqual(test.toObject(0), 0);
  // assert.ok(!Number.isNaN(test.toObject(Number.NaN)));
  //
  EXPECT_STRICT_EQ(ToString(Eval("''")), "''");
  EXPECT_STRICT_EQ(ToString(Eval("'test'")), "'test'");
  EXPECT_STRICT_EQ(ToString(Eval("undefined")), "'undefined'");
  EXPECT_STRICT_EQ(ToString(Eval("null")), "'null'");
  EXPECT_STRICT_EQ(ToString(Eval("false")), "'false'");
  EXPECT_STRICT_EQ(ToString(Eval("true")), "'true'");
  EXPECT_STRICT_EQ(ToString(Eval("0")), "'0'");
  EXPECT_STRICT_EQ(ToString(Eval("1.1")), "'1.1'");
  EXPECT_STRICT_EQ(ToString(Eval("Number.NaN")), "'NaN'");
  EXPECT_STRICT_EQ(ToString(Eval("a = {}")), "'[object Object]'");
  EXPECT_STRICT_EQ(ToString(Eval("a = { toString: () => 'test' }")), "'test'");
  EXPECT_STRICT_EQ(ToString(Eval("[]")), "''");
  EXPECT_STRICT_EQ(ToString(Eval("[ 1, 2, 3 ]")), "'1,2,3'");
  // assert.throws(() => test.toString(testSym), TypeError);

#define GEN_NULL_CHECK_BINDING(binding_name, output_type, api) \
  auto binding_name = [&]() {                                       \
    napi_value return_value = CreateObject();                  \
    output_type result;                                        \
    add_returned_status(                                       \
        env,                                                   \
        "envIsNull",                                           \
        return_value,                                          \
        "Invalid argument",                                    \
        napi_invalid_arg,                                      \
        api(nullptr, return_value, &result));                  \
    api(env, nullptr, &result);                                \
    add_last_status(env, "valueIsNull", return_value);         \
    api(env, return_value, nullptr);                           \
    add_last_status(env, "resultIsNull", return_value);        \
    api(env, return_value, &result);                           \
    add_last_status(env, "inputTypeCheck", return_value);      \
    return return_value;                                       \
  }

  GEN_NULL_CHECK_BINDING(GetValueBool, bool, napi_get_value_bool);
  // GEN_NULL_CHECK_BINDING(GetValueInt32, int32_t, napi_get_value_int32)
  // GEN_NULL_CHECK_BINDING(GetValueUint32, uint32_t, napi_get_value_uint32)
  // GEN_NULL_CHECK_BINDING(GetValueInt64, int64_t, napi_get_value_int64)
  // GEN_NULL_CHECK_BINDING(GetValueDouble, double, napi_get_value_double)
  // GEN_NULL_CHECK_BINDING(CoerceToBool, napi_value, napi_coerce_to_bool)
  // GEN_NULL_CHECK_BINDING(CoerceToObject, napi_value, napi_coerce_to_object)
  // GEN_NULL_CHECK_BINDING(CoerceToString, napi_value, napi_coerce_to_string)

#undef GEN_NULL_CHECK_BINDING

  // auto NullTestBoolValuedPropApi =
  //     [&](napi_status (*api)(napi_env, napi_value, napi_value, bool *)) {
  //       bool result;

  //       napi_value return_value = CreateObject();
  //       napi_value object = CreateObject();
  //       napi_value key = CreateStringUtf8("someString");

  //       add_returned_status(
  //           env,
  //           "envIsNull",
  //           return_value,
  //           "Invalid argument",
  //           napi_invalid_arg,
  //           api(nullptr, object, key, &result));

  //       api(env, nullptr, key, &result);
  //       add_last_status(env, "objectIsNull", return_value);

  //       api(env, object, nullptr, &result);
  //       add_last_status(env, "keyIsNull", return_value);

  //       api(env, object, key, nullptr);
  //       add_last_status(env, "valueIsNull", return_value);

  //       return return_value;
  //     };

  EXPECT_DEEP_STRICT_EQ(GetValueBool(), R"({
   envIsNull: 'Invalid argument',
   valueIsNull: 'Invalid argument',
   resultIsNull: 'Invalid argument',
   inputTypeCheck: 'A boolean was expected'
  })");
  // assert.deepStrictEqual(test.testNull.getValueBool(), {
  //  envIsNull: 'Invalid argument',
  //  valueIsNull: 'Invalid argument',
  //  resultIsNull: 'Invalid argument',
  //  inputTypeCheck: 'A boolean was expected'
  //});
  //
  // assert.deepStrictEqual(test.testNull.getValueInt32(), {
  //  envIsNull: 'Invalid argument',
  //  valueIsNull: 'Invalid argument',
  //  resultIsNull: 'Invalid argument',
  //  inputTypeCheck: 'A number was expected'
  //});
  //
  // assert.deepStrictEqual(test.testNull.getValueUint32(), {
  //  envIsNull: 'Invalid argument',
  //  valueIsNull: 'Invalid argument',
  //  resultIsNull: 'Invalid argument',
  //  inputTypeCheck: 'A number was expected'
  //});
  //
  // assert.deepStrictEqual(test.testNull.getValueInt64(), {
  //  envIsNull: 'Invalid argument',
  //  valueIsNull: 'Invalid argument',
  //  resultIsNull: 'Invalid argument',
  //  inputTypeCheck: 'A number was expected'
  //});
  //
  //
  // assert.deepStrictEqual(test.testNull.getValueDouble(), {
  //  envIsNull: 'Invalid argument',
  //  valueIsNull: 'Invalid argument',
  //  resultIsNull: 'Invalid argument',
  //  inputTypeCheck: 'A number was expected'
  //});
  //
  // assert.deepStrictEqual(test.testNull.coerceToBool(), {
  //  envIsNull: 'Invalid argument',
  //  valueIsNull: 'Invalid argument',
  //  resultIsNull: 'Invalid argument',
  //  inputTypeCheck: 'napi_ok'
  //});
  //
  // assert.deepStrictEqual(test.testNull.coerceToObject(), {
  //  envIsNull: 'Invalid argument',
  //  valueIsNull: 'Invalid argument',
  //  resultIsNull: 'Invalid argument',
  //  inputTypeCheck: 'napi_ok'
  //});
  //
  // assert.deepStrictEqual(test.testNull.coerceToString(), {
  //  envIsNull: 'Invalid argument',
  //  valueIsNull: 'Invalid argument',
  //  resultIsNull: 'Invalid argument',
  //  inputTypeCheck: 'napi_ok'
  //});
  //
  // assert.deepStrictEqual(test.testNull.getValueStringUtf8(), {
  //  envIsNull: 'Invalid argument',
  //  valueIsNull: 'Invalid argument',
  //  wrongTypeIn: 'A string was expected',
  //  bufAndOutLengthIsNull: 'Invalid argument'
  //});
  //
  // assert.deepStrictEqual(test.testNull.getValueStringLatin1(), {
  //  envIsNull: 'Invalid argument',
  //  valueIsNull: 'Invalid argument',
  //  wrongTypeIn: 'A string was expected',
  //  bufAndOutLengthIsNull: 'Invalid argument'
  //});
  //
  // assert.deepStrictEqual(test.testNull.getValueStringUtf16(), {
  //  envIsNull: 'Invalid argument',
  //  valueIsNull: 'Invalid argument',
  //  wrongTypeIn: 'A string was expected',
  //  bufAndOutLengthIsNull: 'Invalid argument'
  //});
}

INSTANTIATE_TEST_SUITE_P(
    NapiEnv,
    NapiTest,
    ::testing::ValuesIn(NapiEnvProviders()));
