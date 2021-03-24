// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "napitestbase.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>
#include "../js_engine_api.h"

extern "C" {
#include "js-native-api/common.c"
}

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
      m_scriptError = std::make_shared<NapiScriptError>();
      m_scriptError->Name = GetPropertyString(env, error, "name");
      m_scriptError->Message = GetPropertyString(env, error, "message");
      m_scriptError->Stack = GetPropertyString(env, error, "stack");
      if (m_scriptError->Name == "AssertionError") {
        m_assertionError = std::make_shared<NapiAssertionError>();
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

void NapiTestBase::AddModule(char const *moduleName, napi_ref module) {
  m_modules.try_emplace(moduleName, module);
}

void NapiTestBase::AddNativeModule(
    char const *moduleName,
    std::function<void(napi_env, napi_value)> initModule) {
  napi_value exports{};
  napi_create_object(env, &exports);
  initModule(env, exports);

  napi_ref moduleRef{};
  napi_create_reference(env, exports, 1, &moduleRef);
  AddModule(moduleName, moduleRef);
}

NapiTestErrorHandler NapiTestBase::RunTestScript(
    char const *script,
    char const *file,
    int32_t line) {
  try {
    RunScript("TestScript", script);
    return NapiTestErrorHandler(std::exception_ptr(), "", file, line);
  } catch (...) {
    return NapiTestErrorHandler(std::current_exception(), script, file, line);
  }
}

NapiTestErrorHandler NapiTestBase::RunTestScript(
    TestScriptInfo const &scriptInfo) {
  return RunTestScript(scriptInfo.script, scriptInfo.file, scriptInfo.line);
}

NapiTestErrorHandler::NapiTestErrorHandler(
    std::exception_ptr const &exception,
    std::string &&script,
    std::string &&file,
    int32_t line) noexcept
    : m_exception(exception),
      m_script(std::move(script)),
      m_file(std::move(file)),
      m_line(line) {}

NapiTestErrorHandler::~NapiTestErrorHandler() noexcept {
  if (m_exception) {
    try {
      std::rethrow_exception(m_exception);
    } catch (NapiTestException const &ex) {
      if (m_handler) {
        m_handler(ex);
      } else if (auto assertionError = ex.AssertionError()) {
        auto sourceFile = assertionError->SourceFile;
        auto sourceLine = assertionError->SourceLine;
        auto sourceCode = std::string("<Source is unavailable>");
        if (sourceFile == "TestScript") {
          sourceFile = m_file;
          const std::string removeFilePrefix = "../../../../jsi/";
          if (sourceFile.find(removeFilePrefix) == 0) {
            sourceFile = sourceFile.substr(removeFilePrefix.length());
          }
          sourceCode = GetSourceCodeSliceForError(sourceLine, 2);
          sourceLine += m_line - 1;
        } else if (sourceFile.empty()) {
          sourceFile = "<Unknown>";
        }

        FAIL_AT(m_file.c_str(), sourceLine)
            << "Exception: " << ex.ScriptError()->Name << "\n"
            << "  Message: " << ex.ScriptError()->Message << "\n"
            << " Expected: " << ex.AssertionError()->Expected << "\n"
            << "   Actual: " << ex.AssertionError()->Actual << "\n"
            << "     File: " << sourceFile << ":" << sourceLine << sourceCode
            << "\n"
            << "Callstack: " << ex.ScriptError()->Stack;
      } else if (ex.ScriptError()) {
        FAIL_AT(m_file.c_str(), m_line)
            << "Exception: " << ex.ScriptError()->Name << "\n"
            << "  Message: " << ex.ScriptError()->Message << "\n"
            << "Callstack: " << ex.ScriptError()->Stack;
      } else {
        FAIL_AT(m_file.c_str(), m_line)
            << "Exception: NapiTestException\n"
            << "     Code: " << ex.ErrorCode() << "\n"
            << "  Message: " << ex.what() << "\n"
            << "     Expr: " << ex.Expr();
      }
    } catch (std::exception const &ex) {
      FAIL_AT(m_file.c_str(), m_line) << "Exception thrown: " << ex.what();
    } catch (...) {
      FAIL_AT(m_file.c_str(), m_line) << "Unexpected test exception.";
    }
  } else if (m_mustThrow) {
    FAIL_AT(m_file.c_str(), m_line)
        << "NapiTestException was expected, but it was not thrown.";
  }
}

void NapiTestErrorHandler::Catch(
    std::function<void(NapiTestException const &)> &&handler) noexcept {
  m_handler = std::move(handler);
}

void NapiTestErrorHandler::Throws(
    std::function<void(NapiTestException const &)> &&handler) noexcept {
  m_handler = std::move(handler);
  m_mustThrow = true;
}

std::string NapiTestErrorHandler::GetSourceCodeSliceForError(
    int32_t lineIndex,
    int32_t extraLineCount) noexcept {
  std::string sourceCode;
  auto sourceStream = std::istringstream(m_script);
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
