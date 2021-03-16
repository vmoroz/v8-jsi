// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#define NAPI_EXPERIMENTAL
#include "../js_native_test_api.h"
#include "napitest.h"

//=============================================================================
// From nodejs\node\test\js-native-api\test_properties\test_properties.c
//=============================================================================

static double value_ = 1;

static napi_value GetValue(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, NULL, NULL, NULL));

  NODE_API_ASSERT(env, argc == 0, "Wrong number of arguments");

  napi_value number;
  NODE_API_CALL(env, napi_create_double(env, value_, &number));

  return number;
}

static napi_value SetValue(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");

  NODE_API_CALL(env, napi_get_value_double(env, args[0], &value_));

  return NULL;
}

static napi_value Echo(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");

  return args[0];
}

static napi_value HasNamedProperty(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 2, "Wrong number of arguments");

  // Extract the name of the property to check
  char buffer[128];
  size_t copied;
  NODE_API_CALL(
      env,
      napi_get_value_string_utf8(
          env, args[1], buffer, sizeof(buffer), &copied));

  // do the check and create the boolean return value
  bool value;
  napi_value result;
  NODE_API_CALL(env, napi_has_named_property(env, args[0], buffer, &value));
  NODE_API_CALL(env, napi_get_boolean(env, value, &result));

  return result;
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_value number;
  NODE_API_CALL(env, napi_create_double(env, value_, &number));

  napi_value name_value;
  NODE_API_CALL(
      env,
      napi_create_string_utf8(
          env, "NameKeyValue", NAPI_AUTO_LENGTH, &name_value));

  napi_value symbol_description;
  napi_value name_symbol;
  NODE_API_CALL(
      env,
      napi_create_string_utf8(
          env, "NameKeySymbol", NAPI_AUTO_LENGTH, &symbol_description));
  NODE_API_CALL(env, napi_create_symbol(env, symbol_description, &name_symbol));

  napi_property_descriptor properties[] = {
      {"echo", 0, Echo, 0, 0, 0, napi_enumerable, 0},
      {"readwriteValue",
       0,
       0,
       0,
       0,
       number,
       napi_property_attributes(napi_enumerable | napi_writable),
       0},
      {"readonlyValue", 0, 0, 0, 0, number, napi_enumerable, 0},
      {"hiddenValue", 0, 0, 0, 0, number, napi_default, 0},
      {NULL, name_value, 0, 0, 0, number, napi_enumerable, 0},
      {NULL, name_symbol, 0, 0, 0, number, napi_enumerable, 0},
      {"readwriteAccessor1", 0, 0, GetValue, SetValue, 0, napi_default, 0},
      {"readwriteAccessor2", 0, 0, GetValue, SetValue, 0, napi_writable, 0},
      {"readonlyAccessor1", 0, 0, GetValue, NULL, 0, napi_default, 0},
      {"readonlyAccessor2", 0, 0, GetValue, NULL, 0, napi_writable, 0},
      {"hasNamedProperty", 0, HasNamedProperty, 0, 0, 0, napi_default, 0},
  };

  NODE_API_CALL(
      env,
      napi_define_properties(
          env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}

//=============================================================================
// ^-- end nodejs\node\test\js-native-api\test_properties\test_properties.c
//=============================================================================

using namespace napitest;

struct NapiTestProperties : NapiTestBase {};

// Testing api calls for defining properties
TEST_P(NapiTestProperties, Test) {
  Init(env, Eval("test_object = {}"));

  EXPECT_JS_CODE_STRICT_EQ("test_object.echo('hello')", "'hello'");

  Eval("test_object.readwriteValue = 1");
  EXPECT_JS_CODE_STRICT_EQ("test_object.readwriteValue", "1");
  Eval("test_object.readwriteValue = 2");
  EXPECT_JS_CODE_STRICT_EQ("test_object.readwriteValue", "2");

  const char *readonlyErrorRE =
      "/^Cannot assign to read only property '.*' of object '#<Object>'$/";
  EXPECT_JS_THROW_MSG("test_object.readonlyValue = 3", readonlyErrorRE);

  EXPECT_JS_TRUE("test_object.hiddenValue");

  // Properties with napi_enumerable attribute should be enumerable.
  Eval(R"(
    propertyNames = [];
    for (const name in test_object) {
      propertyNames.push(name);
    }
    )");
  EXPECT_JS_TRUE("propertyNames.includes('echo')");
  EXPECT_JS_TRUE("propertyNames.includes('readwriteValue')");
  EXPECT_JS_TRUE("propertyNames.includes('readonlyValue')");
  EXPECT_JS_TRUE("!propertyNames.includes('hiddenValue')");
  EXPECT_JS_TRUE("propertyNames.includes('NameKeyValue')");
  EXPECT_JS_TRUE("!propertyNames.includes('readwriteAccessor1')");
  EXPECT_JS_TRUE("!propertyNames.includes('readwriteAccessor2')");
  EXPECT_JS_TRUE("!propertyNames.includes('readonlyAccessor1')");
  EXPECT_JS_TRUE("!propertyNames.includes('readonlyAccessor2')");

  // Validate property created with symbol
  Eval(R"(
    const start = 'Symbol('.length;
    const end = start + 'NameKeySymbol'.length;
    symbolDescription =
      String(Object.getOwnPropertySymbols(test_object)[0]).slice(start, end);
    )");
  EXPECT_JS_CODE_STRICT_EQ("symbolDescription", "'NameKeySymbol'");

  // The napi_writable attribute should be ignored for accessors.
  Eval(R"(
    readwriteAccessor1Descriptor =
      Object.getOwnPropertyDescriptor(test_object, 'readwriteAccessor1');
    readonlyAccessor1Descriptor =
      Object.getOwnPropertyDescriptor(test_object, 'readonlyAccessor1');
    )");
  EXPECT_JS_TRUE("readwriteAccessor1Descriptor.get != null");
  EXPECT_JS_TRUE("readwriteAccessor1Descriptor.set != null");
  EXPECT_JS_TRUE("readwriteAccessor1Descriptor.value === undefined");
  EXPECT_JS_TRUE("readonlyAccessor1Descriptor.get != null");
  EXPECT_JS_TRUE("readonlyAccessor1Descriptor.set === undefined");
  EXPECT_JS_TRUE("readonlyAccessor1Descriptor.value === undefined");
  Eval("test_object.readwriteAccessor1 = 1");
  EXPECT_JS_CODE_STRICT_EQ("test_object.readwriteAccessor1", "1");
  EXPECT_JS_CODE_STRICT_EQ("test_object.readonlyAccessor1", "1");
  const char *getterOnlyErrorRE =
      "/^Cannot set property .* of #<Object> which has only a getter$/";
  EXPECT_JS_THROW_MSG("test_object.readonlyAccessor1 = 3", getterOnlyErrorRE);
  Eval("test_object.readwriteAccessor2 = 2");
  EXPECT_JS_CODE_STRICT_EQ("test_object.readwriteAccessor2", "2");
  EXPECT_JS_CODE_STRICT_EQ("test_object.readonlyAccessor2", "2");
  EXPECT_JS_THROW_MSG("test_object.readonlyAccessor2 = 3", getterOnlyErrorRE);

  EXPECT_JS_CODE_STRICT_EQ(
      "test_object.hasNamedProperty(test_object, 'echo')", "true");
  EXPECT_JS_CODE_STRICT_EQ(
      "test_object.hasNamedProperty(test_object, 'hiddenValue')", "true");
  EXPECT_JS_CODE_STRICT_EQ(
      "test_object.hasNamedProperty(test_object,'doesnotexist')", "false");
}

INSTANTIATE_TEST_SUITE_P(
    NapiEnv,
    NapiTestProperties,
    ::testing::ValuesIn(NapiEnvProviders()));
