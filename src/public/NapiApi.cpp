// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "NapiApi.h"
#include <sstream>
#include <utility>

namespace napijsi {

NapiApi::NapiApi(napi_env env) noexcept : m_env{env} {}

//=============================================================================
// NapiApi::JsRefHolder implementation
//=============================================================================

NapiApi::NapiRefHolder::NapiRefHolder(NapiApi *napi, napi_ext_ref ref) noexcept
    : m_napi{napi}, m_ref{ref} {}

NapiApi::NapiRefHolder::NapiRefHolder(NapiApi *napi, napi_value value) noexcept
    : m_napi{napi} {
  m_ref = m_napi->CreateReference(value);
}

NapiApi::NapiRefHolder::NapiRefHolder(NapiRefHolder &&other) noexcept
    : m_napi{std::exchange(other.m_napi, nullptr)},
      m_ref{std::exchange(other.m_ref, nullptr)} {}

NapiApi::NapiRefHolder &NapiApi::NapiRefHolder::operator=(
    NapiRefHolder &&other) noexcept {
  if (this != &other) {
    NapiRefHolder temp{std::move(*this)};
    m_napi = std::exchange(other.m_napi, nullptr);
    m_ref = std::exchange(other.m_ref, nullptr);
  }

  return *this;
}

NapiApi::NapiRefHolder::~NapiRefHolder() noexcept {
  if (m_ref) {
    // Clear m_ref before calling napi_delete_reference on it to make sure that
    // we always hold a valid m_ref.
    m_napi->ReleaseReference(std::exchange(m_ref, nullptr));
  }
}

[[nodiscard]] napi_ext_ref NapiApi::NapiRefHolder::CloneRef() const noexcept {
  if (m_ref) {
    napi_ext_clone_reference(m_napi->m_env, m_ref);
  }

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
  NapiVerifyElseCrash(
      napi_get_and_clear_last_exception(m_env, &jsError) == napi_ok,
      "Cannot retrieve JS exception.");
  ThrowJsExceptionOverride(errorCode, jsError);
}

[[noreturn]] void NapiApi::ThrowNativeException(
    char const *errorMessage) const {
  ThrowNativeExceptionOverride(errorMessage);
}

[[noreturn]] void NapiApi::ThrowJsExceptionOverride(
    napi_status errorCode,
    napi_value /*jsError*/) const {
  std::ostringstream errorString;
  errorString << "A call to NAPI API returned error code 0x" << std::hex
              << errorCode << '.';
  throw std::exception(errorString.str().c_str());
}

[[noreturn]] void NapiApi::ThrowNativeExceptionOverride(
    char const *errorMessage) const {
  throw std::exception(errorMessage);
}

napi_ext_ref NapiApi::CreateReference(napi_value value) const {
  napi_ext_ref result{};
  CHECK_NAPI(napi_ext_create_reference(m_env, value, &result));
  return result;
}

void NapiApi::ReleaseReference(napi_ext_ref ref) const {
  // TODO: [vmoroz] make it safe to be called from another thread per JSI spec.
  CHECK_NAPI(napi_ext_release_reference(m_env, ref));
}

napi_value NapiApi::GetReferenceValue(napi_ext_ref ref) const {
  napi_value result{};
  CHECK_NAPI(napi_ext_get_reference_value(m_env, ref, &result));
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

napi_value NapiApi::GetPropertyIdFromName(StringView name) const {
  napi_value propertyId{};
  CHECK_NAPI(
      napi_create_string_utf8(m_env, name.data(), name.size(), &propertyId));
  return propertyId;
}

napi_value NapiApi::GetPropertyIdFromSymbol(
    StringView symbolDescription) const {
  napi_value result{};
  napi_value description = CreateStringUtf8(symbolDescription);
  CHECK_NAPI(napi_create_symbol(m_env, description, &result));
  return result;
}

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

napi_value NapiApi::GetGlobal() const {
  napi_value result{nullptr};
  CHECK_NAPI(napi_get_global(m_env, &result));
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

double NapiApi::GetValueDouble(napi_value value) const {
  double result{0};
  CHECK_NAPI(napi_get_value_double(m_env, value, &result));
  return result;
}

napi_value NapiApi::CreateStringLatin1(StringView value) const {
  NapiVerifyElseThrow(value.data(), "Cannot convert a nullptr to a JS string.");

  napi_value result{};
  NapiVerifyJsErrorElseThrow(
      napi_create_string_latin1(m_env, value.data(), value.size(), &result));
  return result;
}

napi_value NapiApi::CreateStringUtf8(StringView value) const {
  NapiVerifyElseThrow(value.data(), "Cannot convert a nullptr to a JS string.");

  napi_value result{};
  NapiVerifyJsErrorElseThrow(
      napi_create_string_utf8(m_env, value.data(), value.size(), &result));
  return result;
}

// Gets or creates a unique string value from an UTF-8 std::string_view.
napi_ext_ref NapiApi::GetUniqueStringUtf8(StringView value) const {
  napi_ext_ref ref{};
  NapiVerifyJsErrorElseThrow(napi_ext_get_unique_utf8_string_ref(
      m_env, value.data(), value.size(), &ref));
  return ref;
}

std::string NapiApi::PropertyIdToStdString(napi_value propertyId) const {
  // TODO: [vmoroz] account for symbol and number property ID
  return StringToStdString(propertyId);
}

std::string NapiApi::StringToStdString(napi_value stringValue) const {
  std::string result;
  NapiVerifyElseThrow(
      TypeOf(stringValue) == napi_valuetype::napi_string,
      "Cannot convert a non JS string ChakraObjectRef to a std::string.");
  size_t strLength{};
  CHECK_NAPI(
      napi_get_value_string_utf8(m_env, stringValue, nullptr, 0, &strLength));
  result.assign(strLength, '\0');
  size_t copiedLength{};
  CHECK_NAPI(napi_get_value_string_utf8(
      m_env, stringValue, &result[0], result.length() + 1, &copiedLength));
  NapiVerifyElseThrow(
      result.length() == copiedLength, "Unexpected string length");
  return result;
}

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

napi_value NapiApi::CreateExternalObject(
    void *data,
    napi_finalize finalizeCallback) const {
  napi_value result{};
  CHECK_NAPI(
      napi_create_external(m_env, data, finalizeCallback, nullptr, &result));
  return result;
}

bool NapiApi::InstanceOf(napi_value object, napi_value constructor) const {
  bool result{false};
  CHECK_NAPI(napi_instanceof(m_env, object, constructor, &result));
  return result;
}

napi_value NapiApi::GetProperty(napi_value object, napi_value propertyId)
    const {
  napi_value result{};
  CHECK_NAPI(napi_get_property(m_env, object, propertyId, &result));
  return result;
}

void NapiApi::SetProperty(
    napi_value object,
    napi_value propertyId,
    napi_value value) const {
  CHECK_NAPI(napi_set_property(m_env, object, propertyId, value));
}

bool NapiApi::HasProperty(napi_value object, napi_value propertyId) const {
  bool result{};
  CHECK_NAPI(napi_has_property(m_env, object, propertyId, &result));
  return result;
}

void NapiApi::DefineProperty(
    napi_value object,
    napi_value propertyId,
    napi_property_descriptor const &descriptor) const {
  CHECK_NAPI(napi_define_properties(m_env, object, 1, &descriptor));
}

void NapiApi::SetElement(napi_value object, uint32_t index, napi_value value)
    const {
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

napi_value NapiApi::CallFunction(
    napi_value thisArg,
    napi_value function,
    Span<napi_value> args) const {
  napi_value result{};
  CHECK_NAPI(napi_call_function(
      m_env, thisArg, function, args.size(), args.begin(), &result));
  return result;
}

napi_value NapiApi::ConstructObject(
    napi_value constructor,
    Span<napi_value> args) const {
  napi_value result{};
  CHECK_NAPI(napi_new_instance(
      m_env, constructor, args.size(), args.begin(), &result));
  return result;
}

napi_value NapiApi::CreateFunction(
    const char *utf8Name,
    size_t nameLength,
    napi_callback callback,
    void *callbackData) const {
  napi_value result{};
  CHECK_NAPI(napi_create_function(
      m_env, utf8Name, nameLength, callback, callbackData, &result));
  return result;
}

bool NapiApi::SetException(napi_value error) const noexcept {
  // This method must not throw. We return false in case of error.
  return napi_throw(m_env, error) == napi_status::napi_ok;
}

bool NapiApi::SetException(StringView message) const noexcept {
  return napi_throw_error(m_env, "Unknown", message.data()) ==
      napi_status::napi_ok;
}

//=============================================================================
// StringView implementation
//=============================================================================

StringView::StringView(const char *data, size_t size) noexcept
    : m_data{data}, m_size{size} {}

StringView::StringView(const char *data) noexcept
    : m_data{data}, m_size{std::char_traits<char>::length(data)} {}

StringView::StringView(const std::string &str) noexcept
    : m_data{str.data()}, m_size{str.size()} {}

constexpr const char *StringView::begin() const noexcept {
  return m_data;
}

constexpr const char *StringView::end() const noexcept {
  return m_data + m_size;
}

constexpr const char &StringView::operator[](size_t pos) const noexcept {
  return *(m_data + pos);
}

constexpr const char *StringView::data() const noexcept {
  return m_data;
}

constexpr size_t StringView::size() const noexcept {
  return m_size;
}

constexpr bool StringView::empty() const noexcept {
  return m_size == 0;
}

void StringView::swap(StringView &other) noexcept {
  using std::swap;
  swap(m_data, other.m_data);
  swap(m_size, other.m_size);
}

int StringView::compare(StringView other) const noexcept {
  size_t minCommonSize = (std::min)(m_size, other.m_size);
  int result =
      std::char_traits<char>::compare(m_data, other.m_data, minCommonSize);
  if (result == 0) {
    if (m_size < other.m_size) {
      result = -1;
    } else if (m_size > other.m_size) {
      result = 1;
    }
  }
  return result;
}

void swap(StringView &left, StringView &right) noexcept {
  left.swap(right);
}

bool operator==(StringView left, StringView right) noexcept {
  return left.compare(right) == 0;
}

bool operator!=(StringView left, StringView right) noexcept {
  return left.compare(right) != 0;
}

bool operator<(StringView left, StringView right) noexcept {
  return left.compare(right) < 0;
}

bool operator<=(StringView left, StringView right) noexcept {
  return left.compare(right) <= 0;
}

bool operator>(StringView left, StringView right) noexcept {
  return left.compare(right) > 0;
}

bool operator>=(StringView left, StringView right) noexcept {
  return left.compare(right) >= 0;
}

StringView operator"" _sv(const char *str, std::size_t len) noexcept {
  return StringView(str, len);
}

//=============================================================================
// StringViewHash implementation
//=============================================================================

size_t StringViewHash::operator()(StringView view) const noexcept {
  return s_classic_collate.hash(view.begin(), view.end());
}

/*static*/ const std::collate<char> &StringViewHash::s_classic_collate =
    std::use_facet<std::collate<char>>(std::locale::classic());

} // namespace napijsi
