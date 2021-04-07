// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "NapiApi.h"
#include <sstream>
#include <utility>
//#include "Unicode.h"

namespace react::jsi {

NapiApi::NapiApi(napi_env env) noexcept : m_env{env} {}

//=============================================================================
// NapiApi::JsRefHolder implementation
//=============================================================================

NapiApi::NapiRefHolder::NapiRefHolder(NapiApi *napi, napi_ref ref) noexcept : m_napi{napi}, m_ref{ref} {}

NapiApi::NapiRefHolder::NapiRefHolder(NapiApi *napi, napi_value value) noexcept : m_napi{napi} {
  m_ref = m_napi->CreateReference(value);
}

NapiApi::NapiRefHolder::NapiRefHolder(NapiRefHolder &&other) noexcept
    : m_napi{std::exchange(other.m_napi, nullptr)}, m_ref{std::exchange(other.m_ref, nullptr)} {}

NapiApi::NapiRefHolder &NapiApi::NapiRefHolder::operator=(NapiRefHolder &&other) noexcept {
  if (this != &other) {
    NapiRefHolder temp{std::move(*this)};
    m_napi = std::exchange(other.m_napi, nullptr);
    m_ref = std::exchange(other.m_ref, nullptr);
  }

  return *this;
}

NapiApi::NapiRefHolder::~NapiRefHolder() noexcept {
  if (m_ref) {
    // Clear m_ref before calling napi_delete_reference on it to make sure that we always hold a valid m_ref.
    m_napi->DeleteReference(std::exchange(m_ref, nullptr));
  }
}

NapiApi::NapiRefHolder::operator napi_ref() const noexcept {
  return m_ref;
}

NapiApi::NapiRefHolder::operator napi_value() const {
  return m_napi->GetReferenceValue(m_ref);
}

NapiApi::NapiRefHolder::operator bool() const noexcept {
  return m_ref != nullptr;
}

//=============================================================================
// NapiApi implementation
//=============================================================================

[[noreturn]] void NapiApi::ThrowJsException(napi_status errorCode) const {
  napi_value jsError{};
  NapiVerifyElseCrash(napi_get_and_clear_last_exception(m_env, &jsError) == napi_ok, "Cannot retrieve JS exception.");
  ThrowJsExceptionOverride(errorCode, jsError);
}

[[noreturn]] void NapiApi::ThrowNativeException(char const *errorMessage) const {
  ThrowNativeExceptionOverride(errorMessage);
}

[[noreturn]] void NapiApi::ThrowJsExceptionOverride(napi_status errorCode, napi_value /*jsError*/) const {
  std::ostringstream errorString;
  errorString << "A call to NAPI API returned error code 0x" << std::hex << errorCode << '.';
  throw std::exception(errorString.str().c_str());
}

[[noreturn]] void NapiApi::ThrowNativeExceptionOverride(char const *errorMessage) const {
  throw std::exception(errorMessage);
}

napi_ref NapiApi::CreateReference(napi_value value) const {
  napi_ref result{};
  CHECK_NAPI(napi_create_reference(m_env, value, 1, &result));
  return result;
}

void NapiApi::DeleteReference(napi_ref ref) const {
  // TODO: [vmoroz] make it safe to be called from another thread per JSI spec.
  CHECK_NAPI(napi_delete_reference(m_env, ref));
}

napi_value NapiApi::GetReferenceValue(napi_ref ref) const {
  napi_value result{};
  CHECK_NAPI(napi_get_reference_value(m_env, ref, &result));
  return result;
}

bool NapiApi::IsArray(napi_value value) const {
  bool result{};
  CHECK_NAPI(napi_is_array(m_env, value, &result));
  return result;
}

bool NapiApi::IsArrayBuffer(napi_value value) const {
  bool result{};
  CHECK_NAPI(napi_is_arraybuffer(m_env, value, &result));
  return result;
}

bool NapiApi::IsFunction(napi_value value) const {
  return TypeOf(value) == napi_valuetype::napi_function;
}

///*static*/ JsRuntimeHandle NapiApi::CreateRuntime(
//    JsRuntimeAttributes attributes,
//    JsThreadServiceCallback threadService) {
//  JsRuntimeHandle runtime{JS_INVALID_REFERENCE};
//  NapiVerifyElseThrow(
//      JsCreateRuntime(attributes, threadService, &runtime) == JsErrorCode::JsNoError, "Cannot create Chakra
//      runtime.");
//  return runtime;
//}
//
///*static*/ void NapiApi::DisposeRuntime(JsRuntimeHandle runtime) {
//  NapiVerifyElseThrow(JsDisposeRuntime(runtime) == JsErrorCode::JsNoError, "Cannot dispaose Chakra runtime.");
//}
//
///*static*/ uint32_t NapiApi::AddRef(JsRef ref) {
//  uint32_t result{0};
//  NapiVerifyJsErrorElseThrow(JsAddRef(ref, &result));
//  return result;
//}
//
///*static*/ uint32_t NapiApi::Release(JsRef ref) {
//  uint32_t result{0};
//  NapiVerifyJsErrorElseThrow(JsRelease(ref, &result));
//  return result;
//}
//
///*static*/ JsContextRef NapiApi::CreateContext(JsRuntimeHandle runtime) {
//  JsContextRef context{JS_INVALID_REFERENCE};
//  NapiVerifyJsErrorElseThrow(JsCreateContext(runtime, &context));
//  return context;
//}
//
///*static*/ JsContextRef NapiApi::GetCurrentContext() {
//  JsContextRef context{JS_INVALID_REFERENCE};
//  NapiVerifyJsErrorElseThrow(JsGetCurrentContext(&context));
//  return context;
//}
//
///*static*/ void NapiApi::SetCurrentContext(JsContextRef context) {
//  NapiVerifyJsErrorElseThrow(JsSetCurrentContext(context));
//}

napi_value NapiApi::GetPropertyIdFromName(std::string_view name) const {
  napi_value propertyId{};
  CHECK_NAPI(napi_create_string_utf8(m_env, name.data(), name.size(), &propertyId));
  return propertyId;
}

///*static*/ JsPropertyIdRef NapiApi::GetPropertyIdFromString(JsValueRef value) {
//  return GetPropertyIdFromName(StringToPointer(value).data());
//}
//
///*static*/ JsPropertyIdRef NapiApi::GetPropertyIdFromName(std::string_view name) {
//  NapiVerifyElseThrow(name.data(), "Property name cannot be a nullptr.");
//
//  JsPropertyIdRef propertyId{JS_INVALID_REFERENCE};
//  // We use a #ifdef here because we can avoid a UTF-8 to UTF-16 conversion
//  // using ChakraCore's JsCreatePropertyId API.
//#ifdef CHAKRACORE
//  NapiVerifyJsErrorElseThrow(JsCreatePropertyId(name.data(), name.length(), &propertyId));
//#else
//  std::wstring utf16 = Common::Unicode::Utf8ToUtf16(name.data(), name.length());
//  NapiVerifyJsErrorElseThrow(JsGetPropertyIdFromName(utf16.data(), &propertyId));
//#endif
//  return propertyId;
//}
//
///*static*/ wchar_t const *NapiApi::GetPropertyNameFromId(JsPropertyIdRef propertyId) {
//  NapiVerifyElseThrow(
//      GetPropertyIdType(propertyId) == JsPropertyIdTypeString,
//      "It is illegal to retrieve the name associated with a property symbol.");
//
//  wchar_t const *name{nullptr};
//  NapiVerifyJsErrorElseThrow(JsGetPropertyNameFromId(propertyId, &name));
//  return name;
//}
//
///*static*/ JsValueRef NapiApi::GetPropertyStringFromId(JsPropertyIdRef propertyId) {
//  return PointerToString(GetPropertyNameFromId(propertyId));
//}
//
///*static*/ JsValueRef NapiApi::GetSymbolFromPropertyId(JsPropertyIdRef propertyId) {
//  NapiVerifyElseThrow(
//      GetPropertyIdType(propertyId) == JsPropertyIdTypeSymbol,
//      "It is illegal to retrieve the symbol associated with a property name.");
//
//  JsValueRef symbol{JS_INVALID_REFERENCE};
//  NapiVerifyJsErrorElseThrow(JsGetSymbolFromPropertyId(propertyId, &symbol));
//  return symbol;
//}
//
///*static*/ JsPropertyIdType NapiApi::GetPropertyIdType(JsPropertyIdRef propertyId) {
//  JsPropertyIdType type{JsPropertyIdType::JsPropertyIdTypeString};
//  NapiVerifyJsErrorElseThrow(JsGetPropertyIdType(propertyId, &type));
//  return type;
//}
//

napi_value NapiApi::GetPropertyIdFromSymbol(std::string_view symbolDescription) const {
  napi_value result{};
  napi_value description = CreateStringUtf8(symbolDescription);
  CHECK_NAPI(napi_create_symbol(m_env, description, &result));
  return result;
}
//
///*static*/ JsValueRef NapiApi::CreateSymbol(JsValueRef symbolDescription) {
//  JsValueRef symbol{JS_INVALID_REFERENCE};
//  NapiVerifyJsErrorElseThrow(JsCreateSymbol(symbolDescription, &symbol));
//  return symbol;
//}
//
///*static*/ JsValueRef NapiApi::CreateSymbol(std::wstring_view symbolDescription) {
//  return CreateSymbol(PointerToString(symbolDescription));
//}

napi_value NapiApi::GetUndefined() const {
  napi_value result{nullptr};
  CHECK_NAPI(napi_get_undefined(m_env, &result));
  return result;
}

napi_value NapiApi::GetNull() const {
  napi_value result{nullptr};
  CHECK_NAPI(napi_get_null(m_env, &result));
  return result;
}

napi_value NapiApi::GetBoolean(bool value) const {
  napi_value result{nullptr};
  CHECK_NAPI(napi_get_boolean(m_env, value, &result));
  return result;
}

bool NapiApi::GetValueBool(napi_value value) const {
  bool result{nullptr};
  CHECK_NAPI(napi_get_value_bool(m_env, value, &result));
  return result;
}

napi_valuetype NapiApi::TypeOf(napi_value value) const {
  napi_valuetype result{};
  CHECK_NAPI(napi_typeof(m_env, value, &result));
  return result;
}

napi_value NapiApi::CreateDouble(double value) const {
  napi_value result{};
  CHECK_NAPI(napi_create_double(m_env, value, &result));
  return result;
}

napi_value NapiApi::CreateInt32(int32_t value) const {
  napi_value result{};
  CHECK_NAPI(napi_create_int32(m_env, value, &result));
  return result;
}

///*static*/ JsValueRef NapiApi::IntToNumber(int32_t value) {
//  JsValueRef numberValue{JS_INVALID_REFERENCE};
//  NapiVerifyJsErrorElseThrow(JsIntToNumber(value, &numberValue));
//  return numberValue;
//}

double NapiApi::GetValueDouble(napi_value value) const {
  double result{0};
  CHECK_NAPI(napi_get_value_double(m_env, value, &result));
  return result;
}

///*static*/ int32_t NapiApi::NumberToInt(JsValueRef value) {
//  int intValue{0};
//  NapiVerifyJsErrorElseThrow(JsNumberToInt(value, &intValue));
//  return intValue;
//}
//
napi_value NapiApi::CreateStringLatin1(std::string_view value) const {
  NapiVerifyElseThrow(value.data(), "Cannot convert a nullptr to a JS string.");

  napi_value result{};
  NapiVerifyJsErrorElseThrow(napi_create_string_latin1(m_env, value.data(), value.size(), &result));
  return result;
}

napi_value NapiApi::CreateStringUtf8(std::string_view value) const {
  NapiVerifyElseThrow(value.data(), "Cannot convert a nullptr to a JS string.");

  napi_value result{};
  NapiVerifyJsErrorElseThrow(napi_create_string_utf8(m_env, value.data(), value.size(), &result));
  return result;
}

// napi_value NapiApi::CreateStringUtf16(std::u16string_view value) {
//  NapiVerifyElseThrow(value.data(), "Cannot convert a nullptr to a JS string.");
//
//  napi_value result{};
//  NapiVerifyJsErrorElseThrow(napi_create_string_utf16(m_env, value.data(), value.size(), &result));
//  return result;
//}

std::string NapiApi::PropertyIdToStdString(napi_value propertyId) const {
  // TODO: [vmoroz] account for symbol and number property ID
  return StringToStdString(propertyId);
}
//
// napi_value NapiApi::GetReferenceValue(napi_ref ref) {
//  napi_value value{};
//  NapiVerifyJsErrorElseThrow(napi_get_reference_value(m_env, ref, &value));
//  return value;
//}
//
///*static*/ std::wstring_view NapiApi::StringToPointer(JsValueRef string) {
//  wchar_t const *utf16{nullptr};
//  size_t length{0};
//  NapiVerifyJsErrorElseThrow(JsStringToPointer(string, &utf16, &length));
//  return {utf16, length};
//}
//
std::string NapiApi::StringToStdString(napi_value stringValue) const {
  std::string result;
  NapiVerifyElseThrow(
      TypeOf(stringValue) == napi_valuetype::napi_string,
      "Cannot convert a non JS string ChakraObjectRef to a std::string.");
  size_t strLength{};
  CHECK_NAPI(napi_get_value_string_utf8(m_env, stringValue, nullptr, 0, &strLength));
  result.assign(strLength, '\0');
  size_t copiedLength{};
  CHECK_NAPI(napi_get_value_string_utf8(m_env, stringValue, result.data(), result.length() + 1, &copiedLength));
  NapiVerifyElseThrow(result.length() == copiedLength, "Unexpected string length");
  return result;
}
//
// std::string NapiApi::StringToStdString(napi_ref stringRef) {
//  return StringToStdString(GetReferenceValue(stringRef));
//}
//
///*static*/ JsValueRef NapiApi::ConvertValueToString(JsValueRef value) {
//  JsValueRef stringValue{JS_INVALID_REFERENCE};
//  NapiVerifyJsErrorElseThrow(JsConvertValueToString(value, &stringValue));
//  return stringValue;
//}

napi_value NapiApi::GetGlobalObject() const {
  napi_value result{};
  CHECK_NAPI(napi_get_global(m_env, &result));
  return result;
}

napi_value NapiApi::CreateObject() const {
  napi_value result{};
  CHECK_NAPI(napi_create_object(m_env, &result));
  return result;
}

napi_value NapiApi::CreateExternalObject(void *data, napi_finalize finalizeCallback) const {
  napi_value result{};
  CHECK_NAPI(napi_create_external(m_env, data, finalizeCallback, nullptr, &result));
  return result;
}

///*static*/ JsValueRef NapiApi::GetPrototype(JsValueRef object) {
//  JsValueRef prototype{JS_INVALID_REFERENCE};
//  NapiVerifyJsErrorElseThrow(JsGetPrototype(object, &prototype));
//  return prototype;
//}
//
bool NapiApi::InstanceOf(napi_value object, napi_value constructor) const {
  bool result{false};
  CHECK_NAPI(napi_instanceof(m_env, object, constructor, &result));
  return result;
}

napi_value NapiApi::GetProperty(napi_value object, napi_value propertyId) const {
  napi_value result{};
  CHECK_NAPI(napi_get_property(m_env, object, propertyId, &result));
  return result;
}
//
///*static*/ JsValueRef NapiApi::GetOwnPropertyNames(JsValueRef object) {
//  JsValueRef propertyNames{JS_INVALID_REFERENCE};
//  NapiVerifyJsErrorElseThrow(JsGetOwnPropertyNames(object, &propertyNames));
//  return propertyNames;
//}
//
///*static*/ void NapiApi::SetProperty(JsValueRef object, JsPropertyIdRef propertyId, JsValueRef value) {
//  NapiVerifyJsErrorElseThrow(JsSetProperty(object, propertyId, value, /*useStringRules:*/ true));
//}

void NapiApi::SetProperty(napi_value object, napi_value propertyId, napi_value value) const {
  CHECK_NAPI(napi_set_property(m_env, object, propertyId, value));
}

bool NapiApi::HasProperty(napi_value object, napi_value propertyId) const {
  bool result{};
  CHECK_NAPI(napi_has_property(m_env, object, propertyId, &result));
  return result;
}

void NapiApi::DefineProperty(napi_value object, napi_value propertyId, napi_property_descriptor const &descriptor)
    const {
  CHECK_NAPI(napi_define_properties(m_env, object, 1, &descriptor));
}

///*static*/ JsValueRef NapiApi::GetIndexedProperty(JsValueRef object, int32_t index) {
//  JsValueRef result{JS_INVALID_REFERENCE};
//  NapiVerifyJsErrorElseThrow(JsGetIndexedProperty(object, IntToNumber(index), &result));
//  return result;
//}
//
void NapiApi::SetElement(napi_value object, uint32_t index, napi_value value) const {
  CHECK_NAPI(napi_set_element(m_env, object, index, value));
}

bool NapiApi::StrictEquals(napi_value left, napi_value right) const {
  bool result{false};
  CHECK_NAPI(napi_strict_equals(m_env, left, right, &result));
  return result;
}

void *NapiApi::GetExternalData(napi_value object) const {
  void *result{};
  CHECK_NAPI(napi_get_value_external(m_env, object, &result));
  return result;
}

napi_value NapiApi::CreateArray(size_t length) const {
  napi_value result{};
  CHECK_NAPI(napi_create_array_with_length(m_env, length, &result));
  return result;
}

///*static*/ JsValueRef NapiApi::CreateArrayBuffer(size_t byteLength) {
//  JsValueRef result{JS_INVALID_REFERENCE};
//  NapiVerifyJsErrorElseThrow(JsCreateArrayBuffer(static_cast<unsigned int>(byteLength), &result));
//  return result;
//}
//
///*static*/ NapiApi::Span<std::byte> NapiApi::GetArrayBufferStorage(JsValueRef arrayBuffer) {
//  BYTE *buffer{nullptr};
//  unsigned int bufferLength{0};
//  NapiVerifyJsErrorElseThrow(JsGetArrayBufferStorage(arrayBuffer, &buffer, &bufferLength));
//  return {reinterpret_cast<std::byte *>(buffer), bufferLength};
//}
//
napi_value NapiApi::CallFunction(napi_value thisArg, napi_value function, Span<napi_value> args) const {
  napi_value result{};
  CHECK_NAPI(napi_call_function(m_env, thisArg, function, args.size(), args.begin(), &result));
  return result;
}

napi_value NapiApi::ConstructObject(napi_value constructor, Span<napi_value> args) const {
  napi_value result{};
  CHECK_NAPI(napi_new_instance(m_env, constructor, args.size(), args.begin(), &result));
  return result;
}

napi_value NapiApi::CreateFunction(const char *utf8Name, size_t nameLength, napi_callback callback, void *callbackData)
    const {
  napi_value result{};
  CHECK_NAPI(napi_create_function(m_env, utf8Name, nameLength, callback, callbackData, &result));
  return result;
}

bool NapiApi::SetException(napi_value error) const noexcept {
  // This method must not throw. We return false in case of error.
  return napi_throw(m_env, error) == napi_status::napi_ok;
}

bool NapiApi::SetException(std::string_view message) const noexcept {
  return napi_throw_error(m_env, "Unknown", message.data()) == napi_status::napi_ok;
}

} // namespace react::jsi
