// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#define NAPI_EXPERIMENTAL
#include "../js_native_test_api.h"
#include "js-native-api/common.h"
#include "js_assert.h"
#include "napitest.h"

//#include "js-native-api/test_function/test_function.c.h"

using namespace napitest;

struct NapiTestFunction2 : NapiTestBase {};

static napi_value Require(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value arg0;
  void *data;
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &arg0, nullptr, &data));

  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");

  // Extract the name of the requested module
  char buffer[128];
  size_t copied;
  NODE_API_CALL(
      env,
      napi_get_value_string_utf8(env, arg0, buffer, sizeof(buffer), &copied));

  napi_value result{};
  if (strncmp("assert", buffer, std::size(buffer)) == 0) {
    auto result =
        static_cast<NapiTestFunction2 *>(data)->RunScript(GetJSAssert());
    return result;
  } else {
    NODE_API_CALL(env, napi_get_undefined(env, &result));
  }
  return result;
}

TEST_P(NapiTestFunction2, CallFunction) {
  napi_value require{}, global{};
  THROW_IF_NOT_OK(napi_get_global(env, &global));
  THROW_IF_NOT_OK(napi_create_function(
      env, "require", NAPI_AUTO_LENGTH, Require, this, &require));
  THROW_IF_NOT_OK(napi_set_named_property(env, global, "require", require));

  try {
    RunScript(R"(

      const assert = require('assert');
      assert.ok(true);
          
    )");
    // auto result = RunScript(GetJSAssert());
    // EXPECT_JS_STRICT_EQ(result, "100");
  } catch (std::exception const &ex) {
    FAIL() << "Exception thrown: " << ex.what();
    return;
  }
}

INSTANTIATE_TEST_SUITE_P(
    NapiEnv,
    NapiTestFunction2,
    ::testing::ValuesIn(NapiEnvProviders()));
