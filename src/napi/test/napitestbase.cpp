// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>
#include "../js_engine_api.h"
#include "lib/modules.h"

namespace napitest {

static std::string GetJSModuleText(const char *jsModuleCode) {
  std::string result;
  result += R"(
    (function(module) {
      const exports = module.exports;)";
  result += "\n";
  result += jsModuleCode;
  result += R"(
      return module.exports;
    })({exports: {}});)";
  return result;
}

static napi_value JSRequire(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value arg0;
  void *data;
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &arg0, nullptr, &data));

  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");

  // Extract the name of the requested module
  char moduleName[128];
  size_t copied;
  NODE_API_CALL(
      env,
      napi_get_value_string_utf8(
          env, arg0, moduleName, sizeof(moduleName), &copied));

  NapiTestBase *testBase = static_cast<NapiTestBase *>(data);
  return testBase->GetModule(moduleName);
}

NapiTestException::NapiTestException(
    napi_env env,
    napi_status errorCode,
    const char *expr) noexcept
    : m_errorCode{errorCode}, m_expr{expr} {
  bool isExceptionPending;
  napi_is_exception_pending(env, &isExceptionPending);
  if (isExceptionPending) {
    napi_value error{};
    if (napi_get_and_clear_last_exception(env, &error) == napi_ok) {
      m_scriptError = std::make_unique<NapiScriptError>();
      m_scriptError->Name = GetPropertyString(env, error, "name");
      m_scriptError->Message = GetPropertyString(env, error, "message");
      m_scriptError->Stack = GetPropertyString(env, error, "stack");
      if (m_scriptError->Name == "AssertionError") {
        m_assertionError = std::make_unique<NapiAssertionError>();
        m_assertionError->Expected = GetPropertyString(env, error, "expected");
        m_assertionError->Actual = GetPropertyString(env, error, "actual");
        m_assertionError->SourceFile =
            GetPropertyString(env, error, "sourceFile");
        m_assertionError->SourceLine =
            GetPropertyInt32(env, error, "sourceLine");
      }
    }
  }
}

/*static*/ napi_value
NapiTestException::GetProperty(napi_env env, napi_value obj, char const *name) {
  napi_value result{};
  napi_get_named_property(env, obj, name, &result);
  return result;
}

/*static*/ std::string NapiTestException::GetPropertyString(
    napi_env env,
    napi_value obj,
    char const *name) {
  napi_value napiValue = GetProperty(env, obj, name);
  size_t valueSize{};
  napi_get_value_string_utf8(env, napiValue, nullptr, 0, &valueSize);
  std::string value(valueSize, '\0');
  napi_get_value_string_utf8(env, napiValue, &value[0], valueSize + 1, nullptr);
  return value;
}

/*static*/ int32_t NapiTestException::GetPropertyInt32(
    napi_env env,
    napi_value obj,
    char const *name) {
  napi_value napiValue = GetProperty(env, obj, name);
  int32_t value{};
  napi_get_value_int32(env, napiValue, &value);
  return value;
}

NapiTestBase::NapiTestBase()
    : provider(GetParam()),
      env(provider->CreateEnv()),
      m_moduleScripts(module::GetModuleScripts()) {
  napi_value require{}, global{};
  THROW_IF_NOT_OK(napi_get_global(env, &global));
  THROW_IF_NOT_OK(napi_create_function(
      env, "require", NAPI_AUTO_LENGTH, JSRequire, this, &require));
  THROW_IF_NOT_OK(napi_set_named_property(env, global, "require", require));
}

NapiTestBase::~NapiTestBase() {
  provider->DeleteEnv();
}

napi_value NapiTestBase::RunScript(const char *scriptUrl, const char *code) {
  napi_value script{}, scriptResult{};
  THROW_IF_NOT_OK(
      napi_create_string_utf8(env, code, NAPI_AUTO_LENGTH, &script));
  if (code) {
    THROW_IF_NOT_OK(js_run_script(env, script, scriptUrl, &scriptResult));
  } else {
    THROW_IF_NOT_OK(napi_run_script(env, script, &scriptResult));
  }
  return scriptResult;
}

napi_value NapiTestBase::GetModule(char const *moduleName) {
  napi_value result{};
  auto moduleIt = m_modules.find(moduleName);
  if (moduleIt != m_modules.end()) {
    NODE_API_CALL(
        env, napi_get_reference_value(env, moduleIt->second, &result));
    return result;
  }

  auto scriptIt = m_moduleScripts.find(moduleName);
  if (scriptIt == m_moduleScripts.end()) {
    NODE_API_CALL(env, napi_get_undefined(env, &result));
    return result;
  }

  result = RunScript(moduleName, GetJSModuleText(scriptIt->second).c_str());
  napi_ref moduleRef{};
  napi_create_reference(env, result, 1, &moduleRef);
  m_modules.try_emplace(moduleName, moduleRef);
  return result;
}

std::string NapiTestBase::GetSourceCodeSliceForError(
    char const *code,
    int32_t lineIndex,
    int32_t extraLineCount) {
  std::string sourceCode;
  auto sourceStream = std::istringstream(code);
  std::string sourceLine;
  int32_t currentLineIndex = 1; // The line index is 1-based.

  while (std::getline(sourceStream, sourceLine, '\n')) {
    if (currentLineIndex > lineIndex + extraLineCount)
      break;
    if (currentLineIndex >= lineIndex - extraLineCount) {
      sourceCode += "\n";
      sourceCode += currentLineIndex == lineIndex ? "===> " : "     ";
      sourceCode += sourceLine;
    }
    ++currentLineIndex;
  }

  return sourceCode;
}

} // namespace napitest
