#include "js_engine_api.h"
#include "js_native_api_v8.h"

napi_status js_run_script(
    napi_env env,
    napi_value script,
    const char *source_url,
    napi_value *result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, script);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> v8_script = v8impl::V8LocalValueFromJsValue(script);

  if (!v8_script->IsString()) {
    return napi_set_last_error(env, napi_string_expected);
  }

  v8::Local<v8::Context> context = env->context();

  v8::Local<v8::String> urlV8String =
      v8::String::NewFromUtf8(context->GetIsolate(), source_url)
          .ToLocalChecked();
  v8::ScriptOrigin origin(urlV8String);

  auto maybe_script =
      v8::Script::Compile(context, v8::Local<v8::String>::Cast(v8_script), &origin);
  CHECK_MAYBE_EMPTY(env, maybe_script, napi_generic_failure);

  auto script_result = maybe_script.ToLocalChecked()->Run(context);
  CHECK_MAYBE_EMPTY(env, script_result, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(script_result.ToLocalChecked());
  return GET_RETURN_STATUS(env);
}