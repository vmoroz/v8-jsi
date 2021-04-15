// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "NapiJsiRuntime.h"

#include <cstring>
#include <limits>
#include <mutex>
#include <sstream>
#include <unordered_set>

#define NAPIJSI_SCOPE(env) EnvScope env_scope_{env};
namespace napijsi {

namespace {

struct EnvScope {
  EnvScope(napi_env env) : env_{env} {
    napi_ext_open_env_scope(env, &env_scope_);
    napi_open_handle_scope(env, &handle_scope_);
  }

  ~EnvScope() {
    napi_close_handle_scope(env_, handle_scope_);
    napi_ext_close_env_scope(env_, env_scope_);
  }

 private:
  napi_env env_{nullptr};
  napi_ext_env_scope env_scope_{nullptr};
  napi_handle_scope handle_scope_{nullptr};
};

struct HostFunctionWrapper final {
  HostFunctionWrapper(facebook::jsi::HostFunctionType &&hostFunction, NapiJsiRuntime &runtime)
      : m_hostFunction(std::move(hostFunction)), m_runtime(runtime) {}

  facebook::jsi::HostFunctionType &GetHostFunction() {
    return m_hostFunction;
  }

  NapiJsiRuntime &GetRuntime() {
    return m_runtime;
  }

 private:
  facebook::jsi::HostFunctionType m_hostFunction;
  NapiJsiRuntime &m_runtime;
};

} // namespace

NapiJsiRuntime::NapiJsiRuntime(napi_env env) noexcept : NapiApi{env}, m_envHolder{env}, m_env{env} {
  NAPIJSI_SCOPE(env);
  m_propertyId.Error = NapiRefHolder{this, GetPropertyIdFromName("Error"_sv)};
  m_propertyId.Object = NapiRefHolder{this, GetPropertyIdFromName("Object"_sv)};
  m_propertyId.Proxy = NapiRefHolder{this, GetPropertyIdFromName("Proxy"_sv)};
  m_propertyId.Symbol = NapiRefHolder{this, GetPropertyIdFromName("Symbol"_sv)};
  m_propertyId.byteLength = NapiRefHolder{this, GetPropertyIdFromName("byteLength"_sv)};
  m_propertyId.configurable = NapiRefHolder{this, GetPropertyIdFromName("configurable"_sv)};
  m_propertyId.enumerable = NapiRefHolder{this, GetPropertyIdFromName("enumerable"_sv)};
  m_propertyId.get = NapiRefHolder{this, GetPropertyIdFromName("get"_sv)};
  m_propertyId.getOwnPropertyDescriptor = NapiRefHolder{this, GetPropertyIdFromName("getOwnPropertyDescriptor"_sv)};
  m_propertyId.hostFunctionSymbol = NapiRefHolder{this, GetPropertyIdFromSymbol("hostFunctionSymbol"_sv)};
  m_propertyId.hostObjectSymbol = NapiRefHolder{this, GetPropertyIdFromSymbol("hostObjectSymbol"_sv)};
  m_propertyId.length = NapiRefHolder{this, GetPropertyIdFromName("length"_sv)};
  m_propertyId.message = NapiRefHolder{this, GetPropertyIdFromName("message"_sv)};
  m_propertyId.ownKeys = NapiRefHolder{this, GetPropertyIdFromName("ownKeys"_sv)};
  m_propertyId.propertyIsEnumerable = NapiRefHolder{this, GetPropertyIdFromName("propertyIsEnumerable"_sv)};
  m_propertyId.prototype = NapiRefHolder{this, GetPropertyIdFromName("prototype"_sv)};
  m_propertyId.set = NapiRefHolder{this, GetPropertyIdFromName("set"_sv)};
  m_propertyId.toString = NapiRefHolder{this, GetPropertyIdFromName("toString"_sv)};
  m_propertyId.value = NapiRefHolder{this, GetPropertyIdFromName("value"_sv)};
  m_propertyId.writable = NapiRefHolder{this, GetPropertyIdFromName("writable"_sv)};

  m_value.Undefined = NapiRefHolder{this, GetUndefined()};
  m_value.Null = NapiRefHolder{this, GetNull()};
  m_value.True = NapiRefHolder{this, GetBoolean(true)};
  m_value.False = NapiRefHolder{this, GetBoolean(false)};
  m_value.Global = NapiRefHolder{this, GetGlobal()};
  m_value.Error = NapiRefHolder{this, GetProperty(m_value.Global, m_propertyId.Error)};
}

NapiJsiRuntime::~NapiJsiRuntime() noexcept {}

napi_value NapiJsiRuntime::CreatePropertyDescriptor(napi_value value, PropertyAttributes attrs) {
  napi_value descriptor = CreateObject();
  SetProperty(descriptor, m_propertyId.value, value);
  if (!(attrs & PropertyAttributes::ReadOnly)) {
    SetProperty(descriptor, m_propertyId.writable, GetBoolean(true));
  }
  if (!(attrs & PropertyAttributes::DontEnum)) {
    SetProperty(descriptor, m_propertyId.enumerable, GetBoolean(true));
  }
  if (!(attrs & PropertyAttributes::DontDelete)) {
    // The JavaScript 'configurable=true' allows property to be deleted.
    SetProperty(descriptor, m_propertyId.configurable, GetBoolean(true));
  }
  return descriptor;
}

#pragma region Functions_inherited_from_Runtime

facebook::jsi::Value NapiJsiRuntime::evaluateJavaScript(
    const std::shared_ptr<const facebook::jsi::Buffer> &buffer,
    const std::string &sourceURL) {
  NAPIJSI_SCOPE(m_env);
  napi_value script{};
  napi_value result{};
  CHECK_NAPI(napi_create_string_utf8(m_env, reinterpret_cast<const char *>(buffer->data()), buffer->size(), &script));
  CHECK_NAPI(napi_run_script(m_env, script, &result));
  return ToJsiValue(result);
}

struct NapiPreparedJavaScript final : facebook::jsi::PreparedJavaScript {
  NapiPreparedJavaScript(
      std::string sourceUrl,
      const std::shared_ptr<const facebook::jsi::Buffer> &sourceBuffer,
      std::unique_ptr<const facebook::jsi::Buffer> byteCode)
      : m_sourceUrl{std::move(sourceUrl)}, m_sourceBuffer{sourceBuffer}, m_byteCode{std::move(byteCode)} {}

  const std::string &SourceUrl() const {
    return m_sourceUrl;
  }

  const facebook::jsi::Buffer &SourceBuffer() const {
    return *m_sourceBuffer;
  }

  const facebook::jsi::Buffer &ByteCode() const {
    return *m_byteCode;
  }

 private:
  std::string m_sourceUrl;
  std::shared_ptr<const facebook::jsi::Buffer> m_sourceBuffer;
  std::unique_ptr<const facebook::jsi::Buffer> m_byteCode;
};

class VectorBuffer : public facebook::jsi::Buffer {
 public:
  VectorBuffer(std::vector<uint8_t> v) : v_(std::move(v)) {}

  size_t size() const override {
    return v_.size();
  }

  uint8_t const *data() const override {
    return v_.data();
  }

 private:
  std::vector<uint8_t> v_;
};

std::unique_ptr<facebook::jsi::Buffer> NapiJsiRuntime::GeneratePreparedScript(
    facebook::jsi::Buffer const &sourceBuffer,
    const char *sourceUrl) {
  napi_value source{};
  CHECK_NAPI(napi_create_string_utf8(
      m_env, reinterpret_cast<const char *>(sourceBuffer.data()), sourceBuffer.size(), &source));
  auto getBuffer = [](napi_env /*env*/, uint8_t const *buffer, size_t buffer_length, void *buffer_hint) -> void {
    auto data = reinterpret_cast<std::vector<uint8_t> *>(buffer_hint);
    data->assign(buffer, buffer + buffer_length);
  };
  std::vector<uint8_t> serialized;
  CHECK_NAPI(napi_ext_serialize_script(m_env, source, sourceUrl, getBuffer, &serialized));
  return std::make_unique<VectorBuffer>(std::move(serialized));
}

std::shared_ptr<const facebook::jsi::PreparedJavaScript> NapiJsiRuntime::prepareJavaScript(
    const std::shared_ptr<const facebook::jsi::Buffer> &sourceBuffer,
    std::string sourceURL) {
  NAPIJSI_SCOPE(m_env);
  return std::make_shared<NapiPreparedJavaScript>(
      std::move(sourceURL), sourceBuffer, GeneratePreparedScript(*sourceBuffer, sourceURL.data()));
}

facebook::jsi::Value NapiJsiRuntime::evaluatePreparedJavaScript(
    const std::shared_ptr<const facebook::jsi::PreparedJavaScript> &preparedJS) {
  NAPIJSI_SCOPE(m_env);
  auto napiPreparedJS = static_cast<const NapiPreparedJavaScript *>(preparedJS.get());
  napi_value source{};
  CHECK_NAPI(napi_create_string_utf8(
      m_env,
      reinterpret_cast<const char *>(napiPreparedJS->SourceBuffer().data()),
      napiPreparedJS->SourceBuffer().size(),
      &source));

  napi_value result{};
  CHECK_NAPI(napi_ext_run_serialized_script(
      m_env,
      source,
      napiPreparedJS->SourceUrl().c_str(),
      napiPreparedJS->ByteCode().data(),
      napiPreparedJS->ByteCode().size(),
      &result));
  return ToJsiValue(result);
}

facebook::jsi::Object NapiJsiRuntime::global() {
  NAPIJSI_SCOPE(m_env);
  return MakePointer<facebook::jsi::Object>(m_value.Global.CloneRef());
}

std::string NapiJsiRuntime::description() {
  return "NapiJsiRuntime";
}

bool NapiJsiRuntime::isInspectable() {
  return false;
}

facebook::jsi::Runtime::PointerValue *NapiJsiRuntime::cloneSymbol(
    const facebook::jsi::Runtime::PointerValue *pointerValue) {
  NAPIJSI_SCOPE(m_env);
  return CloneNapiPointerValue(pointerValue);
}

facebook::jsi::Runtime::PointerValue *NapiJsiRuntime::cloneString(
    const facebook::jsi::Runtime::PointerValue *pointerValue) {
  NAPIJSI_SCOPE(m_env);
  return CloneNapiPointerValue(pointerValue);
}

facebook::jsi::Runtime::PointerValue *NapiJsiRuntime::cloneObject(
    const facebook::jsi::Runtime::PointerValue *pointerValue) {
  NAPIJSI_SCOPE(m_env);
  return CloneNapiPointerValue(pointerValue);
}

facebook::jsi::Runtime::PointerValue *NapiJsiRuntime::clonePropNameID(
    const facebook::jsi::Runtime::PointerValue *pointerValue) {
  NAPIJSI_SCOPE(m_env);
  return CloneNapiPointerValue(pointerValue);
}

facebook::jsi::PropNameID NapiJsiRuntime::createPropNameIDFromAscii(const char *str, size_t length) {
  NAPIJSI_SCOPE(m_env);
  // TODO: [vmoroz] Create internal function for faster conversion.
  napi_value napiStr = CreateStringLatin1({str, length});
  std::string strValue = StringToStdString(napiStr);
  napi_ext_ref propertyId{};
  NapiVerifyJsErrorElseThrow(napi_ext_get_unique_utf8_string_ref(m_env, strValue.data(), strValue.size(), &propertyId));
  return MakePointer<facebook::jsi::PropNameID>(propertyId);
}

facebook::jsi::PropNameID NapiJsiRuntime::createPropNameIDFromUtf8(const uint8_t *utf8, size_t length) {
  NAPIJSI_SCOPE(m_env);
  napi_ext_ref propertyId{};
  NapiVerifyJsErrorElseThrow(
      napi_ext_get_unique_utf8_string_ref(m_env, reinterpret_cast<const char *>(utf8), length, &propertyId));
  return MakePointer<facebook::jsi::PropNameID>(propertyId);
}

facebook::jsi::PropNameID NapiJsiRuntime::createPropNameIDFromString(const facebook::jsi::String &str) {
  NAPIJSI_SCOPE(m_env);
  std::string strValue = StringToStdString(GetNapiValue(str));
  napi_ext_ref propertyId{};
  NapiVerifyJsErrorElseThrow(napi_ext_get_unique_utf8_string_ref(m_env, strValue.data(), strValue.size(), &propertyId));
  return MakePointer<facebook::jsi::PropNameID>(propertyId);
}

std::string NapiJsiRuntime::utf8(const facebook::jsi::PropNameID &id) {
  NAPIJSI_SCOPE(m_env);
  return PropertyIdToStdString(GetNapiValue(id));
}

bool NapiJsiRuntime::compare(const facebook::jsi::PropNameID &lhs, const facebook::jsi::PropNameID &rhs) {
  NAPIJSI_SCOPE(m_env);
  bool result{};
  CHECK_NAPI(napi_strict_equals(m_env, GetNapiValue(lhs), GetNapiValue(rhs), &result));
  return result;
}

std::string NapiJsiRuntime::symbolToString(const facebook::jsi::Symbol &s) {
  NAPIJSI_SCOPE(m_env);
  const napi_value symbol = GetNapiValue(s);
  const napi_value symbolCtor = GetProperty(m_value.Global, m_propertyId.Symbol);
  const napi_value symbolPrototype = GetProperty(symbolCtor, m_propertyId.prototype);
  const napi_value symbolToString = GetProperty(symbolPrototype, m_propertyId.toString);
  const napi_value jsString = CallFunction(symbol, symbolToString, {});
  return StringToStdString(jsString);
}

facebook::jsi::String NapiJsiRuntime::createStringFromAscii(const char *str, size_t length) {
  NAPIJSI_SCOPE(m_env);
  return MakePointer<facebook::jsi::String>(CreateStringLatin1({str, length}));
}

facebook::jsi::String NapiJsiRuntime::createStringFromUtf8(const uint8_t *str, size_t length) {
  NAPIJSI_SCOPE(m_env);
  return MakePointer<facebook::jsi::String>(CreateStringUtf8({reinterpret_cast<char const *>(str), length}));
}

std::string NapiJsiRuntime::utf8(const facebook::jsi::String &str) {
  NAPIJSI_SCOPE(m_env);
  return StringToStdString(GetNapiValue(str));
}

facebook::jsi::Object NapiJsiRuntime::createObject() {
  NAPIJSI_SCOPE(m_env);
  return MakePointer<facebook::jsi::Object>(CreateObject());
}

facebook::jsi::Object NapiJsiRuntime::createObject(std::shared_ptr<facebook::jsi::HostObject> hostObject) {
  // The hostObjectHolder keeps the hostObject as external data.
  // Then the hostObjectHolder is wrapped up by a Proxy object to provide access
  // to hostObject's get, set, and getPropertyNames methods. There is a special
  // symbol property ID 'hostObjectSymbol' to access the hostObjectWrapper from
  // the Proxy.
  NAPIJSI_SCOPE(m_env);
  napi_value hostObjectHolder =
      CreateExternalObject(std::make_unique<std::shared_ptr<facebook::jsi::HostObject>>(std::move(hostObject)));
  napi_value obj = CreateObject();
  SetProperty(obj, m_propertyId.hostObjectSymbol, hostObjectHolder);
  if (!m_value.ProxyConstructor) {
    m_value.ProxyConstructor = NapiRefHolder{this, GetProperty(m_value.Global, m_propertyId.Proxy)};
  }
  napi_value proxy = ConstructObject(m_value.ProxyConstructor, {obj, GetHostObjectProxyHandler()});
  return MakePointer<facebook::jsi::Object>(proxy);
}

std::shared_ptr<facebook::jsi::HostObject> NapiJsiRuntime::getHostObject(const facebook::jsi::Object &obj) {
  NAPIJSI_SCOPE(m_env);
  napi_value hostObjectHolder = GetProperty(GetNapiValue(obj), m_propertyId.hostObjectSymbol);
  if (TypeOf(hostObjectHolder) == napi_valuetype::napi_external) {
    return *static_cast<std::shared_ptr<facebook::jsi::HostObject> *>(GetExternalData(hostObjectHolder));
  } else {
    throw facebook::jsi::JSINativeException("getHostObject() can only be called with HostObjects.");
  }
}

facebook::jsi::HostFunctionType &NapiJsiRuntime::getHostFunction(const facebook::jsi::Function &func) {
  NAPIJSI_SCOPE(m_env);
  napi_value hostFunctionHolder = GetProperty(GetNapiValue(func), m_propertyId.hostFunctionSymbol);
  if (TypeOf(hostFunctionHolder) == napi_valuetype::napi_external) {
    return static_cast<HostFunctionWrapper *>(GetExternalData(hostFunctionHolder))->GetHostFunction();
  } else {
    throw facebook::jsi::JSINativeException("getHostFunction() can only be called with HostFunction.");
  }
}

facebook::jsi::Value NapiJsiRuntime::getProperty(
    const facebook::jsi::Object &obj,
    const facebook::jsi::PropNameID &name) {
  NAPIJSI_SCOPE(m_env);
  return ToJsiValue(GetProperty(GetNapiValue(obj), GetNapiValue(name)));
}

facebook::jsi::Value NapiJsiRuntime::getProperty(const facebook::jsi::Object &obj, const facebook::jsi::String &name) {
  NAPIJSI_SCOPE(m_env);
  return ToJsiValue(GetProperty(GetNapiValue(obj), GetNapiValue(name)));
}

bool NapiJsiRuntime::hasProperty(const facebook::jsi::Object &obj, const facebook::jsi::PropNameID &name) {
  NAPIJSI_SCOPE(m_env);
  return HasProperty(GetNapiValue(obj), GetNapiValue(name));
}

bool NapiJsiRuntime::hasProperty(const facebook::jsi::Object &obj, const facebook::jsi::String &name) {
  NAPIJSI_SCOPE(m_env);
  return HasProperty(GetNapiValue(obj), GetNapiValue(name));
}

void NapiJsiRuntime::setPropertyValue(
    facebook::jsi::Object &object,
    const facebook::jsi::PropNameID &name,
    const facebook::jsi::Value &value) {
  NAPIJSI_SCOPE(m_env);
  SetProperty(GetNapiValue(object), GetNapiValue(name), ToNapiValue(value));
}

void NapiJsiRuntime::setPropertyValue(
    facebook::jsi::Object &object,
    const facebook::jsi::String &name,
    const facebook::jsi::Value &value) {
  NAPIJSI_SCOPE(m_env);
  SetProperty(GetNapiValue(object), GetNapiValue(name), ToNapiValue(value));
}

bool NapiJsiRuntime::isArray(const facebook::jsi::Object &obj) const {
  NAPIJSI_SCOPE(m_env);
  return IsArray(GetNapiValue(obj));
}

bool NapiJsiRuntime::isArrayBuffer(const facebook::jsi::Object &obj) const {
  NAPIJSI_SCOPE(m_env);
  return IsArrayBuffer(GetNapiValue(obj));
}

bool NapiJsiRuntime::isFunction(const facebook::jsi::Object &obj) const {
  NAPIJSI_SCOPE(m_env);
  return IsFunction(GetNapiValue(obj));
}

bool NapiJsiRuntime::isHostObject(const facebook::jsi::Object &obj) const {
  NAPIJSI_SCOPE(m_env);
  napi_value hostObjectHolder = GetProperty(GetNapiValue(obj), m_propertyId.hostObjectSymbol);
  if (TypeOf(hostObjectHolder) == napi_valuetype::napi_external) {
    return GetExternalData(hostObjectHolder) != nullptr;
  } else {
    return false;
  }
}

bool NapiJsiRuntime::isHostFunction(const facebook::jsi::Function &func) const {
  NAPIJSI_SCOPE(m_env);
  napi_value hostFunctionHolder = GetProperty(GetNapiValue(func), m_propertyId.hostFunctionSymbol);
  if (TypeOf(hostFunctionHolder) == napi_valuetype::napi_external) {
    return GetExternalData(hostFunctionHolder) != nullptr;
  } else {
    return false;
  }
}

facebook::jsi::Array NapiJsiRuntime::getPropertyNames(const facebook::jsi::Object &object) {
  NAPIJSI_SCOPE(m_env);
  napi_value properties;
  CHECK_NAPI(napi_get_all_property_names(
      m_env,
      GetNapiValue(object),
      napi_key_collection_mode::napi_key_include_prototypes,
      napi_key_filter(napi_key_enumerable | napi_key_skip_symbols),
      napi_key_numbers_to_strings,
      &properties));
  return MakePointer<facebook::jsi::Object>(properties).asArray(*this);
}

facebook::jsi::WeakObject NapiJsiRuntime::createWeakObject(const facebook::jsi::Object &object) {
  NAPIJSI_SCOPE(m_env);
  napi_ext_ref weakRef{};
  // The Reference with the initial ref count == 0 is a weak pointer
  CHECK_NAPI(napi_ext_create_weak_reference(m_env, GetNapiValue(object), &weakRef));
  return MakePointer<facebook::jsi::WeakObject>(weakRef);
}

facebook::jsi::Value NapiJsiRuntime::lockWeakObject(facebook::jsi::WeakObject &weakObject) {
  NAPIJSI_SCOPE(m_env);
  napi_value value = GetNapiValue(weakObject);
  if (value) {
    return ToJsiValue(value);
  } else {
    return ToJsiValue(GetUndefined());
  }
}

facebook::jsi::Array NapiJsiRuntime::createArray(size_t length) {
  NAPIJSI_SCOPE(m_env);
  napi_value result{};
  CHECK_NAPI(napi_create_array_with_length(m_env, length, &result));
  return MakePointer<facebook::jsi::Object>(result).asArray(*this);
}

size_t NapiJsiRuntime::size(const facebook::jsi::Array &arr) {
  NAPIJSI_SCOPE(m_env);
  size_t result{};
  CHECK_NAPI(napi_get_array_length(m_env, GetNapiValue(arr), reinterpret_cast<uint32_t *>(&result)));
  return result;
}

size_t NapiJsiRuntime::size(const facebook::jsi::ArrayBuffer &arrBuf) {
  NAPIJSI_SCOPE(m_env);
  size_t result{};
  CHECK_NAPI(napi_get_arraybuffer_info(m_env, GetNapiValue(arrBuf), nullptr, &result));
  return result;
}

uint8_t *NapiJsiRuntime::data(const facebook::jsi::ArrayBuffer &arrBuf) {
  NAPIJSI_SCOPE(m_env);
  uint8_t *result{};
  CHECK_NAPI(napi_get_arraybuffer_info(m_env, GetNapiValue(arrBuf), reinterpret_cast<void **>(&result), nullptr));
  return result;
}

facebook::jsi::Value NapiJsiRuntime::getValueAtIndex(const facebook::jsi::Array &arr, size_t index) {
  NAPIJSI_SCOPE(m_env);
  napi_value result{};
  CHECK_NAPI(napi_get_element(m_env, GetNapiValue(arr), static_cast<int32_t>(index), &result));
  return ToJsiValue(result);
}

void NapiJsiRuntime::setValueAtIndexImpl(facebook::jsi::Array &arr, size_t index, const facebook::jsi::Value &value) {
  NAPIJSI_SCOPE(m_env);
  CHECK_NAPI(napi_set_element(m_env, GetNapiValue(arr), static_cast<int32_t>(index), ToNapiValue(value)));
}

facebook::jsi::Function NapiJsiRuntime::createFunctionFromHostFunction(
    const facebook::jsi::PropNameID &name,
    unsigned int paramCount,
    facebook::jsi::HostFunctionType func) {
  NAPIJSI_SCOPE(m_env);
  auto hostFunctionWrapper = std::make_unique<HostFunctionWrapper>(std::move(func), *this);
  napi_value function = CreateExternalFunction(
      GetNapiValue(name), static_cast<int32_t>(paramCount), HostFunctionCall, hostFunctionWrapper.get());

  const auto hostFunctionHolder = CreateExternalObject(std::move(hostFunctionWrapper));
  napi_property_descriptor descriptor{};
  descriptor.name = m_propertyId.hostFunctionSymbol;
  descriptor.value = hostFunctionHolder;
  DefineProperty(function, m_propertyId.hostFunctionSymbol, descriptor);

  return MakePointer<facebook::jsi::Object>(function).getFunction(*this);
}

facebook::jsi::Value NapiJsiRuntime::call(
    const facebook::jsi::Function &func,
    const facebook::jsi::Value &jsThis,
    const facebook::jsi::Value *args,
    size_t count) {
  NAPIJSI_SCOPE(m_env);
  return ToJsiValue(CallFunction(
      ToNapiValue(jsThis), GetNapiValue(func), NapiValueArgs(*this, Span<facebook::jsi::Value const>(args, count))));
}

facebook::jsi::Value
NapiJsiRuntime::callAsConstructor(const facebook::jsi::Function &func, const facebook::jsi::Value *args, size_t count) {
  NAPIJSI_SCOPE(m_env);
  return ToJsiValue(
      ConstructObject(GetNapiValue(func), NapiValueArgs(*this, Span<facebook::jsi::Value const>(args, count))));
}

facebook::jsi::Runtime::ScopeState *NapiJsiRuntime::pushScope() {
  NAPIJSI_SCOPE(m_env);
  napi_handle_scope result{};
  CHECK_NAPI(napi_open_handle_scope(m_env, &result));
  return reinterpret_cast<facebook::jsi::Runtime::ScopeState *>(result);
}

void NapiJsiRuntime::popScope(Runtime::ScopeState *state) {
  NAPIJSI_SCOPE(m_env);
  CHECK_NAPI(napi_close_handle_scope(m_env, reinterpret_cast<napi_handle_scope>(state)));
}

bool NapiJsiRuntime::strictEquals(const facebook::jsi::Symbol &a, const facebook::jsi::Symbol &b) const {
  NAPIJSI_SCOPE(m_env);
  return StrictEquals(GetNapiValue(a), GetNapiValue(b));
}

bool NapiJsiRuntime::strictEquals(const facebook::jsi::String &a, const facebook::jsi::String &b) const {
  NAPIJSI_SCOPE(m_env);
  return StrictEquals(GetNapiValue(a), GetNapiValue(b));
}

bool NapiJsiRuntime::strictEquals(const facebook::jsi::Object &a, const facebook::jsi::Object &b) const {
  NAPIJSI_SCOPE(m_env);
  return StrictEquals(GetNapiValue(a), GetNapiValue(b));
}

bool NapiJsiRuntime::instanceOf(const facebook::jsi::Object &obj, const facebook::jsi::Function &func) {
  NAPIJSI_SCOPE(m_env);
  bool result{};
  CHECK_NAPI(napi_instanceof(m_env, GetNapiValue(obj), GetNapiValue(func), &result));
  return result;
}

#pragma endregion Functions_inherited_from_Runtime

template <typename T>
struct AutoRestore {
  AutoRestore(T *var, T value) : var_(var), value_(*var) {
    *var = value;
  }
  ~AutoRestore() {
    *var_ = value_;
  }
  T *var_;
  T value_;
};

[[noreturn]] void NapiJsiRuntime::ThrowJsExceptionOverride(napi_status errorCode, napi_value jsError) const {
  if (!m_pendingJSError && (errorCode == napi_pending_exception || InstanceOf(jsError, m_value.Error))) {
    AutoRestore<bool> setValue(const_cast<bool *>(&m_pendingJSError), true);
    RewriteErrorMessage(jsError);
    throw facebook::jsi::JSError(*const_cast<NapiJsiRuntime *>(this), ToJsiValue(jsError));
  } else {
    std::ostringstream errorString;
    errorString << "A call to Chakra API returned error code 0x" << std::hex << errorCode << '.';
    throw facebook::jsi::JSINativeException(errorString.str().c_str());
  }
}

[[noreturn]] void NapiJsiRuntime::ThrowNativeExceptionOverride(char const *errorMessage) const {
  throw facebook::jsi::JSINativeException(errorMessage);
}

void NapiJsiRuntime::RewriteErrorMessage(napi_value jsError) const {
  // The code below must work correctly even if the 'message' getter throws.
  // In case when it throws, we ignore that exception.
  napi_value message{};
  napi_status errorCode = napi_get_property(m_env, jsError, m_propertyId.message, &message);
  if (errorCode != napi_ok) {
    // If the 'message' property getter throws, then we clear the exception and
    // ignore it.
    napi_value ignoreJSError{};
    napi_get_and_clear_last_exception(m_env, &ignoreJSError);
  } else if (TypeOf(message) == napi_string) {
    // JSI unit tests expect V8 or JSC like message for stack overflow.
    if (StringToStdString(message) == "Out of stack space") {
      SetProperty(jsError, m_propertyId.message, CreateStringUtf8("RangeError : Maximum call stack size exceeded"_sv));
    }
  }
}

facebook::jsi::Value NapiJsiRuntime::ToJsiValue(napi_value value) const {
  switch (TypeOf(value)) {
    case napi_valuetype::napi_undefined:
      return facebook::jsi::Value::undefined();
    case napi_valuetype::napi_null:
      return facebook::jsi::Value::null();
    case napi_valuetype::napi_boolean:
      return facebook::jsi::Value{GetValueBool(value)};
    case napi_valuetype::napi_number:
      return facebook::jsi::Value{GetValueDouble(value)};
    case napi_valuetype::napi_string:
      return facebook::jsi::Value{MakePointer<facebook::jsi::String>(value)};
    case napi_valuetype::napi_symbol:
      return facebook::jsi::Value{MakePointer<facebook::jsi::Symbol>(value)};
    case napi_valuetype::napi_object:
    case napi_valuetype::napi_function:
    case napi_valuetype::napi_external:
    case napi_valuetype::napi_bigint:
      return facebook::jsi::Value{MakePointer<facebook::jsi::Object>(value)};
    default:
      throw facebook::jsi::JSINativeException("Unexpected value type");
  }
}

napi_value NapiJsiRuntime::ToNapiValue(const facebook::jsi::Value &value) const {
  if (value.isUndefined()) {
    return GetUndefined();
  } else if (value.isNull()) {
    return GetNull();
  } else if (value.isBool()) {
    return GetBoolean(value.getBool());
  } else if (value.isNumber()) {
    return CreateDouble(value.getNumber());
  } else if (value.isSymbol()) {
    return GetNapiValue(value.getSymbol(*const_cast<NapiJsiRuntime *>(this)));
  } else if (value.isString()) {
    return GetNapiValue(value.getString(*const_cast<NapiJsiRuntime *>(this)));
  } else if (value.isObject()) {
    return GetNapiValue(value.getObject(*const_cast<NapiJsiRuntime *>(this)));
  } else {
    throw facebook::jsi::JSINativeException("Unexpected jsi::Value type");
  }
}

napi_value NapiJsiRuntime::CreateExternalFunction(
    napi_value name,
    int32_t paramCount,
    napi_callback nativeFunction,
    void *callbackState) {
  std::string functionName = StringToStdString(name);
  napi_value function = CreateFunction(functionName.data(), functionName.length(), nativeFunction, callbackState);
  napi_property_descriptor descriptor{};
  descriptor.name = m_propertyId.length;
  descriptor.value = CreateInt32(paramCount);
  DefineProperty(function, m_propertyId.length, descriptor);
  return function;
}

/*static*/ napi_value NapiJsiRuntime::HostFunctionCall(napi_env env, napi_callback_info info) noexcept {
  HostFunctionWrapper *hostFuncWraper{};
  size_t argc{};
  // TODO: [vmoroz] check result
  napi_get_cb_info(env, info, &argc, nullptr, nullptr, reinterpret_cast<void **>(&hostFuncWraper));
  NapiVerifyElseCrash(hostFuncWraper, "Cannot fund the host function");
  NapiJsiRuntime &jsiRuntime = hostFuncWraper->GetRuntime();
  return jsiRuntime.HandleCallbackExceptions([&]() {
    SmallBuffer<napi_value, MaxStackArgCount> napiArgs(argc);
    auto thisArg = napi_value{};
    napi_get_cb_info(env, info, &argc, napiArgs.Data(), &thisArg, nullptr);
    const JsiValueView jsiThisArg{&jsiRuntime, thisArg};
    JsiValueViewArgs jsiArgs(&jsiRuntime, {napiArgs.Data(), argc});

    const facebook::jsi::HostFunctionType &hostFunc = hostFuncWraper->GetHostFunction();
    return jsiRuntime.RunInMethodContext("HostFunction", [&]() {
      return jsiRuntime.ToNapiValue(hostFunc(jsiRuntime, jsiThisArg, jsiArgs.Data(), jsiArgs.Size()));
    });
  });
}

/*static*/ napi_value NapiJsiRuntime::HostObjectGetTrap(napi_env env, napi_callback_info info) noexcept {
  NapiJsiRuntime *jsiRuntime{};
  size_t argCount{};
  napi_get_cb_info(env, info, &argCount, nullptr, nullptr, reinterpret_cast<void **>(&jsiRuntime));
  return jsiRuntime->HandleCallbackExceptions([&]() {
    // args[0] - the Proxy target object.
    // args[1] - the name of the property to set.
    // args[2] - the Proxy object (unused).
    CHECK_ELSE_THROW(jsiRuntime, argCount == 3, "HostObjectGetTrap() requires 3 arguments.");
    SmallBuffer<napi_value, MaxStackArgCount> napiArgs{argCount};
    napi_get_cb_info(env, info, &argCount, napiArgs.Data(), nullptr, nullptr);
    const napi_value target = napiArgs.Data()[0];
    const napi_value propertyName = napiArgs.Data()[1];
    napi_valuetype propertyIdType = jsiRuntime->TypeOf(propertyName);
    if (propertyIdType == napi_valuetype::napi_string) {
      napi_value external = jsiRuntime->GetProperty(target, jsiRuntime->m_propertyId.hostObjectSymbol);
      auto const &hostObject =
          *static_cast<std::shared_ptr<facebook::jsi::HostObject> *>(jsiRuntime->GetExternalData(external));
      const PropNameIDView propertyId{jsiRuntime, propertyName};
      return jsiRuntime->RunInMethodContext(
          "HostObject::get", [&]() { return jsiRuntime->ToNapiValue(hostObject->get(*jsiRuntime, propertyId)); });
    } else if (propertyIdType == napi_valuetype::napi_symbol) {
      if (jsiRuntime->StrictEquals(propertyName, jsiRuntime->m_propertyId.hostObjectSymbol)) {
        // The special property to retrieve the target object.
        napi_value external = jsiRuntime->GetProperty(target, jsiRuntime->m_propertyId.hostObjectSymbol);
        return external;
      } else {
        napi_value external = jsiRuntime->GetProperty(target, jsiRuntime->m_propertyId.hostObjectSymbol);
        auto const &hostObject =
            *static_cast<std::shared_ptr<facebook::jsi::HostObject> *>(jsiRuntime->GetExternalData(external));
        const PropNameIDView propertyId{jsiRuntime, propertyName};
        return jsiRuntime->RunInMethodContext(
            "HostObject::get", [&]() { return jsiRuntime->ToNapiValue(hostObject->get(*jsiRuntime, propertyId)); });
      }
    }

    return jsiRuntime->GetUndefined();
  });
}

/*static*/ napi_value NapiJsiRuntime::HostObjectSetTrap(napi_env env, napi_callback_info info) noexcept {
  NapiJsiRuntime *jsiRuntime{};
  size_t argCount{};
  napi_get_cb_info(env, info, &argCount, nullptr, nullptr, reinterpret_cast<void **>(&jsiRuntime));
  return jsiRuntime->HandleCallbackExceptions([&]() {
    // args[0] - the Proxy target object.
    // args[1] - the name of the property to set.
    // args[2] - the new value of the property to set.
    // args[3] - the Proxy object (unused).
    CHECK_ELSE_THROW(jsiRuntime, argCount == 4, "HostObjectSetTrap() requires 4 arguments.");

    SmallBuffer<napi_value, MaxStackArgCount> napiArgs{argCount};
    napi_get_cb_info(env, info, &argCount, napiArgs.Data(), nullptr, nullptr);

    const napi_value target = napiArgs.Data()[0];
    const napi_value propertyName = napiArgs.Data()[1];
    if (jsiRuntime->TypeOf(propertyName) == napi_valuetype::napi_string) {
      napi_value external = jsiRuntime->GetProperty(target, jsiRuntime->m_propertyId.hostObjectSymbol);
      auto const &hostObject =
          *static_cast<std::shared_ptr<facebook::jsi::HostObject> *>(jsiRuntime->GetExternalData(external));
      const PropNameIDView propertyId{jsiRuntime, propertyName};
      const JsiValueView value{jsiRuntime, napiArgs.Data()[2]};
      jsiRuntime->RunInMethodContext("HostObject::set", [&]() { hostObject->set(*jsiRuntime, propertyId, value); });
    }

    return jsiRuntime->GetUndefined();
  });
}

/*static*/ napi_value NapiJsiRuntime::HostObjectOwnKeysTrap(napi_env env, napi_callback_info info) noexcept {
  NapiJsiRuntime *jsiRuntime{};
  size_t argCount{};
  napi_get_cb_info(env, info, &argCount, nullptr, nullptr, reinterpret_cast<void **>(&jsiRuntime));
  return jsiRuntime->HandleCallbackExceptions([&]() {
    // args[0] - the Proxy target object.
    CHECK_ELSE_THROW(jsiRuntime, argCount == 1, "HostObjectOwnKeysTrap() requires 1 arguments.");

    SmallBuffer<napi_value, MaxStackArgCount> napiArgs{argCount};
    napi_get_cb_info(env, info, &argCount, napiArgs.Data(), nullptr, nullptr);

    const napi_value target = napiArgs.Data()[0];
    napi_value external = jsiRuntime->GetProperty(target, jsiRuntime->m_propertyId.hostObjectSymbol);
    auto const &hostObject =
        *static_cast<std::shared_ptr<facebook::jsi::HostObject> *>(jsiRuntime->GetExternalData(external));

    const std::vector<facebook::jsi::PropNameID> ownKeys = jsiRuntime->RunInMethodContext(
        "HostObject::getPropertyNames", [&]() { return hostObject->getPropertyNames(*jsiRuntime); });

    std::unordered_set<napi_ext_ref> dedupedOwnKeys{};
    dedupedOwnKeys.reserve(ownKeys.size());
    for (facebook::jsi::PropNameID const &key : ownKeys) {
      dedupedOwnKeys.insert(jsiRuntime->GetNapiRef(key));
    }

    const napi_value result = jsiRuntime->CreateArray(dedupedOwnKeys.size());
    uint32_t index = 0;
    for (napi_ext_ref ref : dedupedOwnKeys) {
      napi_value key{};
      // TODO: [vmoroz] wrap napi_ext_get_reference_value
      napi_status temp_error_code_ = napi_ext_get_reference_value(env, ref, &key);
      if (temp_error_code_ != napi_status::napi_ok) {
        jsiRuntime->ThrowJsException(temp_error_code_);
      }

      jsiRuntime->SetElement(result, index, key);
      ++index;
    }

    return result;
  });
}

/*static*/ napi_value NapiJsiRuntime::HostObjectGetOwnPropertyDescriptorTrap(
    napi_env env,
    napi_callback_info info) noexcept {
  NapiJsiRuntime *jsiRuntime{};
  size_t argCount{};
  napi_get_cb_info(env, info, &argCount, nullptr, nullptr, reinterpret_cast<void **>(&jsiRuntime));
  return jsiRuntime->HandleCallbackExceptions([&]() {
    // args[0] - the Proxy target object.
    // args[1] - the property
    CHECK_ELSE_THROW(jsiRuntime, argCount == 2, "HostObjectGetOwnPropertyDescriptorTrap() requires 2 arguments.");
    SmallBuffer<napi_value, MaxStackArgCount> napiArgs{argCount};
    napi_get_cb_info(env, info, &argCount, napiArgs.Data(), nullptr, nullptr);
    const napi_value target = napiArgs.Data()[0];
    const napi_value propertyName = napiArgs.Data()[1];
    napi_valuetype propertyIdType = jsiRuntime->TypeOf(propertyName);
    if (propertyIdType == napi_valuetype::napi_string) {
      napi_value external = jsiRuntime->GetProperty(target, jsiRuntime->m_propertyId.hostObjectSymbol);
      auto const &hostObject =
          *static_cast<std::shared_ptr<facebook::jsi::HostObject> *>(jsiRuntime->GetExternalData(external));
      const PropNameIDView propertyId{jsiRuntime, propertyName};
      return jsiRuntime->RunInMethodContext("HostObject::getOwnPropertyDescriptor", [&]() {
        auto value = jsiRuntime->ToNapiValue(hostObject->get(*jsiRuntime, propertyId));
        auto descriptor = jsiRuntime->CreatePropertyDescriptor(value, PropertyAttributes::None);
        return descriptor;
      });
    }

    return jsiRuntime->GetUndefined();
  });
}

napi_value NapiJsiRuntime::GetHostObjectProxyHandler() {
  if (!m_value.HostObjectProxyHandler) {
    const napi_value handler = CreateObject();
    SetProperty(handler, m_propertyId.get, CreateExternalFunction(m_propertyId.get, 3, HostObjectGetTrap, this));
    SetProperty(handler, m_propertyId.set, CreateExternalFunction(m_propertyId.set, 4, HostObjectSetTrap, this));
    SetProperty(
        handler, m_propertyId.ownKeys, CreateExternalFunction(m_propertyId.ownKeys, 1, HostObjectOwnKeysTrap, this));
    SetProperty(
        handler,
        m_propertyId.getOwnPropertyDescriptor,
        CreateExternalFunction(m_propertyId.getOwnPropertyDescriptor, 2, HostObjectGetOwnPropertyDescriptorTrap, this));
    m_value.HostObjectProxyHandler = NapiRefHolder{this, handler};
  }

  return m_value.HostObjectProxyHandler;
}

//===========================================================================
// NapiJsiRuntime::NapiValueArgs implementation
//===========================================================================

NapiJsiRuntime::NapiValueArgs::NapiValueArgs(NapiJsiRuntime &rt, Span<facebook::jsi::Value const> args)
    : m_count{args.size()}, m_args{m_count} {
  napi_value *const jsArgs = m_args.Data();
  for (size_t i = 0; i < m_count; ++i) {
    jsArgs[i] = rt.ToNapiValue(args[i]);
  }
}

NapiJsiRuntime::NapiValueArgs::operator NapiApi::Span<napi_value>() {
  return Span<napi_value>(m_args.Data(), m_count);
}

//===========================================================================
// NapiJsiRuntime::JsiValueView implementation
//===========================================================================

NapiJsiRuntime::JsiValueView::JsiValueView(NapiApi *napi, napi_value jsValue)
    : m_value{InitValue(napi, jsValue, std::addressof(m_pointerStore))} {}

NapiJsiRuntime::JsiValueView::~JsiValueView() noexcept {}

NapiJsiRuntime::JsiValueView::operator facebook::jsi::Value const &() const noexcept {
  return m_value;
}

/*static*/ facebook::jsi::Value
NapiJsiRuntime::JsiValueView::InitValue(NapiApi *napi, napi_value value, StoreType *store) {
  switch (napi->TypeOf(value)) {
    case napi_valuetype::napi_undefined:
      return facebook::jsi::Value::undefined();
    case napi_valuetype::napi_null:
      return facebook::jsi::Value::null();
    case napi_valuetype::napi_boolean:
      return facebook::jsi::Value{napi->GetValueBool(value)};
    case napi_valuetype::napi_number:
      return facebook::jsi::Value{napi->GetValueDouble(value)};
    case napi_valuetype::napi_string:
      return make<facebook::jsi::String>(new (store) NapiPointerValueView{napi, value});
    case napi_valuetype::napi_symbol:
      return make<facebook::jsi::Symbol>(new (store) NapiPointerValueView{napi, value});
    case napi_valuetype::napi_object:
    case napi_valuetype::napi_function:
    case napi_valuetype::napi_external:
    case napi_valuetype::napi_bigint:
      return make<facebook::jsi::Object>(new (store) NapiPointerValueView{napi, value});
    default:
      throw facebook::jsi::JSINativeException("Unexpected value type");
  }
}

//===========================================================================
// NapiJsiRuntime::JsiValueViewArray implementation
//===========================================================================

NapiJsiRuntime::JsiValueViewArgs::JsiValueViewArgs(NapiApi *napi, Span<napi_value> args) noexcept
    : m_size{args.size()}, m_pointerStore{args.size()}, m_args{args.size()} {
  JsiValueView::StoreType *pointerStore = m_pointerStore.Data();
  facebook::jsi::Value *const jsiArgs = m_args.Data();
  for (uint32_t i = 0; i < m_size; ++i) {
    jsiArgs[i] = JsiValueView::InitValue(napi, args[i], std::addressof(pointerStore[i]));
  }
}

facebook::jsi::Value const *NapiJsiRuntime::JsiValueViewArgs::Data() noexcept {
  return m_args.Data();
}

size_t NapiJsiRuntime::JsiValueViewArgs::Size() const noexcept {
  return m_size;
}

//===========================================================================
// NapiJsiRuntime::PropNameIDView implementation
//===========================================================================

NapiJsiRuntime::PropNameIDView::PropNameIDView(NapiApi *napi, napi_value propertyId) noexcept
    : m_propertyId{make<facebook::jsi::PropNameID>(new (std::addressof(m_pointerStore))
                                                       NapiPointerValueView{napi, propertyId})} {}

NapiJsiRuntime::PropNameIDView::~PropNameIDView() noexcept {}

NapiJsiRuntime::PropNameIDView::operator facebook::jsi::PropNameID const &() const noexcept {
  return m_propertyId;
}

//===========================================================================
// NapiJsiRuntime miscellaneous
//===========================================================================

std::unique_ptr<facebook::jsi::Runtime> MakeNapiJsiRuntime(napi_env env) noexcept {
  return std::make_unique<NapiJsiRuntime>(env);
}

} // namespace napijsi
