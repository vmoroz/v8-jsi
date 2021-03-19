// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"
#include <algorithm>
#include <limits>
#include "js_assert_module.h"

namespace napitest {

static std::string GetJSModuleText(const char *jsModuleCode) {
  return std::string("(function(exports){\n") + jsModuleCode +
      "\nreturn exports})({});";
}

static napi_value JSRequire(napi_env env, napi_callback_info info) {
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
    auto result = static_cast<NapiTestBase *>(data)->RunScript(
        GetJSModuleText(js_assert_module).c_str());
    return result;
  } else {
    NODE_API_CALL(env, napi_get_undefined(env, &result));
  }
  return result;
}

NapiTestException::NapiTestException(
    napi_env env,
    napi_status errorCode,
    const char *expr) noexcept
    : m_errorCode{errorCode}, m_expr{expr} {
  bool isExceptionPending;
  napi_is_exception_pending(env, &isExceptionPending);
  if (isExceptionPending) {
    napi_value error{}, errorMessage{};
    size_t messageSize{};
    if (napi_get_and_clear_last_exception(env, &error) == napi_ok) {
      m_scriptError = std::make_unique<NapiScriptError>();
      m_scriptError->Name = GetProperty(env, error, "name");
      m_scriptError->Message = GetProperty(env, error, "message");
      m_scriptError->Stack = GetProperty(env, error, "stack");
      if (m_scriptError->Name == "AssertError") {
        m_assertError = std::make_unique<NapiAssertError>();
        m_assertError->Method = GetProperty(env, error, "method");
        m_assertError->Expected = GetProperty(env, error, "expected");
        m_assertError->Actual = GetProperty(env, error, "actual");
      }
    }
  }
}

/*static*/ std::string
NapiTestException::GetProperty(napi_env env, napi_value obj, char const *name) {
  napi_value napiValue{};
  size_t valueSize{};
  napi_get_named_property(env, obj, name, &napiValue);
  napi_get_value_string_utf8(env, napiValue, nullptr, 0, &valueSize);
  std::string value(valueSize, '\0');
  napi_get_value_string_utf8(env, napiValue, &value[0], valueSize + 1, nullptr);
  return value;
}

NapiTestBase::NapiTestBase()
    : provider(GetParam()), env(provider->CreateEnv()) {
  napi_value require{}, global{};
  THROW_IF_NOT_OK(napi_get_global(env, &global));
  THROW_IF_NOT_OK(napi_create_function(
      env, "require", NAPI_AUTO_LENGTH, JSRequire, this, &require));
  THROW_IF_NOT_OK(napi_set_named_property(env, global, "require", require));
}

NapiTestBase::~NapiTestBase() {
  provider->DeleteEnv();
}

napi_value NapiTestBase::RunScript(const char *code) {
  napi_value script{}, scriptResult{};
  THROW_IF_NOT_OK(
      napi_create_string_utf8(env, code, NAPI_AUTO_LENGTH, &script));
  THROW_IF_NOT_OK(napi_run_script(env, script, &scriptResult));
  return scriptResult;
}

} // namespace napitest