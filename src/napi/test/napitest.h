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