// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#define NAPI_EXPERIMENTAL
#include "../js_native_test_api.h"
#include "napitest.h"

using namespace napitest;

struct NapiTestFunction : NapiTestBase {
  napi_value TestCallFunction(
      napi_value func,
      std::initializer_list<napi_value> args = {}) {
    napi_valuetype funcValueType;
    THROW_IF_NOT_OK(napi_typeof(env, func, &funcValueType));
    EXPECT_EQ(funcValueType, napi_function) << "Expects as a function";

    napi_value result{}, global{};
    THROW_IF_NOT_OK(napi_get_global(env, &global));
    THROW_IF_NOT_OK(napi_call_function(
        env, global, func, args.size(), args.begin(), &result));

    return result;
  }
};

static napi_value TestFunctionName(
    napi_env /*env*/,
    napi_callback_info /*info*/) {
  return nullptr;
}

static void FinalizeFunction(napi_env env, void *data, void *hint) {
  napi_ref ref = static_cast<napi_ref>(data);

  // Retrieve the JavaScript undefined value.
  napi_value undefined;
  THROW_IF_NOT_OK(napi_get_undefined(env, &undefined));

  // Retrieve the JavaScript function we must call.
  napi_value js_function;
  THROW_IF_NOT_OK(napi_get_reference_value(env, ref, &js_function));

  // Call the JavaScript function to indicate that the generated JavaScript
  // function is about to be gc-ed.
  THROW_IF_NOT_OK(
      napi_call_function(env, undefined, js_function, 0, nullptr, nullptr));

  // Destroy the persistent reference to the function we just called so as to
  // properly clean up.
  THROW_IF_NOT_OK(napi_delete_reference(env, ref));
}

static napi_value MakeTrackedFunction(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value js_finalize_cb;
  napi_valuetype arg_type;

  // Retrieve and validate from the arguments the function we will use to
  // indicate to JavaScript that the function we are about to create is about
  // to be gc-ed.
  THROW_IF_NOT_OK(
      napi_get_cb_info(env, info, &argc, &js_finalize_cb, nullptr, nullptr));
  EXPECT_EQ(argc, 1) << "Wrong number of arguments";
  THROW_IF_NOT_OK(napi_typeof(env, js_finalize_cb, &arg_type));
  EXPECT_EQ(arg_type, napi_function) << "Argument must be a function";

  // Dynamically create a function.
  napi_value result;
  THROW_IF_NOT_OK(napi_create_function(
      env,
      "TrackedFunction",
      NAPI_AUTO_LENGTH,
      TestFunctionName,
      nullptr,
      &result));

  // Create a strong reference to the function we will call when the tracked
  // function is about to be gc-ed.
  napi_ref js_finalize_cb_ref;
  THROW_IF_NOT_OK(
      napi_create_reference(env, js_finalize_cb, 1, &js_finalize_cb_ref));

  // Attach a finalizer to the dynamically created function and pass it the
  // strong reference we created in the previous step.
  THROW_IF_NOT_OK(napi_wrap(
      env, result, js_finalize_cb_ref, FinalizeFunction, nullptr, nullptr));

  return result;
}

TEST_P(NapiTestFunction, CallFunction) {
  Eval(R"(
    function func1() {
      return 1;
    })");

  EXPECT_JS_STRICT_EQ(TestCallFunction(Value("func1")), "1");

  Eval(R"(
    function func2() {
      return null;
    })");

  EXPECT_JS_STRICT_EQ(TestCallFunction(Value("func2")), "null");

  Eval(R"(
    function func3(input) {
      return input + 1;
    })");

  EXPECT_JS_STRICT_EQ(TestCallFunction(Value("func3"), {Value("1")}), "2");

  Eval(R"(
    function func4(input) {
      return func3(input);
    })");

  EXPECT_JS_STRICT_EQ(TestCallFunction(Value("func4"), {Value("1")}), "2");
}

TEST_P(NapiTestFunction, FunctionName) {
  napi_value fn2;
  THROW_IF_NOT_OK(napi_create_function(
      env, "Name", NAPI_AUTO_LENGTH, TestFunctionName, nullptr, &fn2));

  napi_value fn3;
  THROW_IF_NOT_OK(napi_create_function(
      env, "Name_extra", 5, TestFunctionName, nullptr, &fn3));

  napi_value global{};
  THROW_IF_NOT_OK(napi_get_global(env, &global));
  THROW_IF_NOT_OK(napi_set_named_property(env, global, "TestName", fn2));
  THROW_IF_NOT_OK(napi_set_named_property(env, global, "TestNameShort", fn3));

  EXPECT_JS_CODE_STRICT_EQ("TestName.name", "'Name'");
  EXPECT_JS_CODE_STRICT_EQ("TestNameShort.name", "'Name_'");
}

TEST_P(NapiTestFunction, CreateFunctionParameters) {
  napi_status status;
  napi_value result{}, return_value{};

  THROW_IF_NOT_OK(napi_create_object(env, &return_value));

  status = napi_create_function(
      nullptr,
      "TrackedFunction",
      NAPI_AUTO_LENGTH,
      TestFunctionName,
      nullptr,
      &result);

  add_returned_status(
      env,
      "envIsNull",
      return_value,
      "Invalid argument",
      napi_invalid_arg,
      status);

  napi_create_function(
      env, nullptr, NAPI_AUTO_LENGTH, TestFunctionName, nullptr, &result);

  add_last_status(env, "nameIsNull", return_value);

  napi_create_function(
      env, "TrackedFunction", NAPI_AUTO_LENGTH, nullptr, nullptr, &result);

  add_last_status(env, "cbIsNull", return_value);

  napi_create_function(
      env,
      "TrackedFunction",
      NAPI_AUTO_LENGTH,
      TestFunctionName,
      nullptr,
      nullptr);

  add_last_status(env, "resultIsNull", return_value);

  EXPECT_JS_DEEP_STRICT_EQ(return_value, R"({
    envIsNull: 'Invalid argument',
    nameIsNull: 'napi_ok',
    cbIsNull: 'Invalid argument',
    resultIsNull: 'Invalid argument'
  })");
}

struct ScopedExposeGC {
  ScopedExposeGC() : m_wasExposed(napi_test_enable_gc_api(true)) {}
  ~ScopedExposeGC() {
    napi_test_enable_gc_api(m_wasExposed);
  }

 private:
  const bool m_wasExposed{false};
};

TEST_P(NapiTestFunction, MakeTrackedFunction) {
  napi_value fn4;
  THROW_IF_NOT_OK(napi_create_function(
      env,
      "MakeTrackedFunction",
      NAPI_AUTO_LENGTH,
      MakeTrackedFunction,
      nullptr,
      &fn4));

  Eval("gcCount = 0");
  Eval("function incGCCount() {++gcCount;}");
  CallFunction({fn4}, "function(fn4) { tracked_function = fn4(incGCCount); }");
  EXPECT_TRUE(CallBoolFunction({}, "() => Boolean(tracked_function)"));
  Eval("tracked_function = null");
  EXPECT_FALSE(CallBoolFunction({}, "() => Boolean(tracked_function)"));

  ScopedExposeGC exposeGC;
  napi_test_run_gc(env);

  EXPECT_JS_STRICT_EQ(Value("gcCount"), "1");
}

INSTANTIATE_TEST_SUITE_P(
    NapiEnv,
    NapiTestFunction,
    ::testing::ValuesIn(NapiEnvProviders()));
