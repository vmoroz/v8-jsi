// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#define NAPI_EXPERIMENTAL
#include <js_native_ext_api.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

// We use macros to report errors.
// Macros provide more flexibility to show assert and provide failure context.

// Check condition and crash process if it fails.
#define NapiVerifyElseCrash(condition, message)            \
  do {                                                     \
    if (!(condition)) {                                    \
      assert(false && "Failed: " #condition && (message)); \
      *((int *)0) = 1;                                     \
    }                                                      \
  } while (false)

// Throw native exception
#define NapiThrow(message) ThrowNativeException(message);

// Check condition and throw native exception if it fails.
#define NapiVerifyElseThrow(condition, message) \
  do {                                          \
    if (!(condition)) {                         \
      ThrowNativeException(message);            \
    }                                           \
  } while (false)

// Check condition and throw native exception if it fails.
#define CHECK_ELSE_THROW(rt, condition, message) \
  do {                                           \
    if (!(condition)) {                          \
      rt->ThrowNativeException(message);         \
    }                                            \
  } while (false)

// Evaluate expression and throw JS exception if it fails.
#define NapiVerifyJsErrorElseThrow(expression)      \
  do {                                              \
    napi_status temp_error_code_ = (expression);    \
    if (temp_error_code_ != napi_status::napi_ok) { \
      ThrowJsException(temp_error_code_);           \
    }                                               \
  } while (false)

// Evaluate expression and throw JS exception if it fails.
#define CHECK_NAPI(expression)                      \
  do {                                              \
    napi_status temp_error_code_ = (expression);    \
    if (temp_error_code_ != napi_status::napi_ok) { \
      ThrowJsException(temp_error_code_);           \
    }                                               \
  } while (false)

namespace napijsi {

/**
 * @brief A minimal subset of std::string_view.
 *
 * For C++17 we must replace it with std::string_view.
 */
struct StringView {
  constexpr StringView() noexcept = default;
  constexpr StringView(const StringView &other) noexcept = default;
  constexpr StringView(const char *data, size_t size) noexcept;
  constexpr StringView(const std::string &str) noexcept;

  constexpr StringView &operator=(const StringView &view) noexcept = default;

  constexpr const char *begin() const noexcept;
  constexpr const char *end() const noexcept;

  constexpr const char &operator[](size_t pos) const noexcept;
  constexpr const char *data() const noexcept;
  constexpr size_t size() const noexcept;

  constexpr bool empty() const noexcept;
  constexpr void swap(StringView &other) noexcept;
  constexpr int compare(StringView other) const noexcept;

  static constexpr size_t npos = size_t(-1);

 private:
  const char *m_data{nullptr};
  size_t m_size{0};
};

void swap(StringView &left, StringView &right) noexcept;

bool operator==(StringView left, StringView right) noexcept;
bool operator!=(StringView left, StringView right) noexcept;
bool operator<(StringView left, StringView right) noexcept;
bool operator<=(StringView left, StringView right) noexcept;
bool operator>(StringView left, StringView right) noexcept;
bool operator>=(StringView left, StringView right) noexcept;

constexpr StringView operator"" _sv(const char *str, std::size_t len) noexcept;

/**
 * @brief A wrapper for N-API.
 *
 * The NapiApi class wraps up the N-API functions in a way that:
 * - functions throw exceptions instead of returning error code (derived class can define the exception types);
 * - standard library types are used when possible to simplify usage.
 *
 * Currently we only wrap up functions that are needed to implement the JSI API.
 */
struct NapiApi {
  explicit NapiApi(napi_env env) noexcept;
  /**
   * @brief A smart pointer for napi_ref.
   *
   * napi_ref is a reference to objects owned by the garbage collector.
   * NapiRefHolder ensures that napi_ref is automatically deleted.
   */
  struct NapiRefHolder final {
    NapiRefHolder(std::nullptr_t = nullptr) noexcept {}
    explicit NapiRefHolder(NapiApi *napi, napi_ref ref) noexcept;
    explicit NapiRefHolder(NapiApi *napi, napi_value value) noexcept;

    // The class is movable.
    NapiRefHolder(NapiRefHolder &&other) noexcept;
    NapiRefHolder &operator=(NapiRefHolder &&other) noexcept;

    // The class is not copyable.
    NapiRefHolder &operator=(NapiRefHolder const &other) = delete;
    NapiRefHolder(NapiRefHolder const &other) = delete;

    ~NapiRefHolder() noexcept;

    operator napi_ref() const noexcept;
    operator napi_value() const;

    explicit operator bool() const noexcept;

   private:
    NapiApi *m_napi{};
    napi_ref m_ref{};
  };

  [[noreturn]] virtual void ThrowJsExceptionOverride(napi_status errorCode, napi_value jsError) const;

  [[noreturn]] virtual void ThrowNativeExceptionOverride(char const *errorMessage) const;

  /**
   * @brief Throws JavaScript exception with provided errorCode.
   */
  [[noreturn]] void ThrowJsException(napi_status errorCode) const;

  /**
   * @brief Throws native exception with provided message.
   */
  [[noreturn]] void ThrowNativeException(char const *errorMessage) const;

  napi_ref CreateReference(napi_value value) const;
  void DeleteReference(napi_ref ref) const;
  napi_value GetReferenceValue(napi_ref ref) const;

    /**
   * @brief Gets the property ID associated with the name.
   */
  napi_value GetPropertyIdFromName(StringView name) const;

  /**
   * @brief Creates symbol and gets the property ID associated with the symbol.
   */
  napi_value GetPropertyIdFromSymbol(StringView symbolDescription) const;

  /**
   * @brief Gets the value of \c undefined in the current script context.
   */
  napi_value GetUndefined() const;

  /**
   * @brief Gets the value of \c null in the current script context.
   */
  napi_value GetNull() const;

  /**
   * @brief Creates a Boolean value from a \c bool value.
   */
  napi_value GetBoolean(bool value) const;

  /**
   * @brief Retrieves the \c bool value of a Boolean value.
   */
  bool GetValueBool(napi_value value) const;

  /**
   * @brief Gets the JavaScript type of a JsValueRef.
   */
  napi_valuetype TypeOf(napi_value value) const;

  napi_value CreateDouble(double value) const;
  napi_value CreateInt32(int32_t value) const;
  double GetValueDouble(napi_value value) const;

  bool IsArray(napi_value value) const;
  bool IsArrayBuffer(napi_value value) const;
  bool IsFunction(napi_value value) const;
  
  // Creates a string value from an ASCII std::string_view.
  napi_value CreateStringLatin1(StringView value) const;

  // Creates a string value from an UTF-8 std::string_view.
  napi_value CreateStringUtf8(StringView value) const;

  // Get a string representation of property Id
  std::string PropertyIdToStdString(napi_value propertyId) const;

  // Converts the string to the utf8-encoded std::string.
  std::string StringToStdString(napi_value stringValue) const;

  ///**
  // * @brief Gets the global object in the current script context.
  // */
  napi_value GetGlobalObject() const;

  /**
   * @brief Creates a new object.
   */
  napi_value CreateObject() const;

  /**
   * @brief Creates a new object that stores some external data.
   */
  napi_value CreateExternalObject(void *data, napi_finalize finalizeCallback) const;

  /**
   * @brief Creates a new object that stores external data as an std::unique_ptr.
   */
  template <typename T>
  napi_value CreateExternalObject(std::unique_ptr<T> &&data) const {
    napi_value object = CreateExternalObject(
        data.get(),
        [](napi_env /*env*/, void *dataToDestroy, void *
           /*finalizerHint*/) {
          // We wrap dataToDestroy in a unique_ptr to avoid calling delete explicitly.
          delete static_cast<T *>(dataToDestroy);
        });

    // We only call data.release() after the CreateExternalObject succeeds.
    // Otherwise, when CreateExternalObject fails and an exception is thrown,
    // the memory that data used to own will be leaked.
    data.release();
    return object;
  }

  bool InstanceOf(napi_value object, napi_value constructor) const;

  /**
   * @brief Gets an object's property.
   */
  napi_value GetProperty(napi_value object, napi_value propertyId) const;

  /**
   * @brief Puts an object's property.
   */
  void SetProperty(napi_value object, napi_value propertyId, napi_value value) const;

  /**
   * @brief Determines whether an object has a property.
   */
  bool HasProperty(napi_value object, napi_value propertyId) const;

  /**
  * @brief Defines a new object's own property from a property descriptor.
  */
  void DefineProperty(napi_value object, napi_value propertyId, napi_property_descriptor const &descriptor) const;

  /**
  * @brief Set the value at the specified index of an object.
  */
  void SetElement(napi_value object, uint32_t index, napi_value value) const;

  /**
  * @brief Compare two JavaScript values for strict equality.
  */
  bool StrictEquals(napi_value left, napi_value right) const;

  /**
   * @brief Retrieves the data from an external object.
   */
  void *GetExternalData(napi_value object) const;

  /**
  * @brief Creates a JavaScript array object.
  */
  napi_value CreateArray(size_t length) const;

  /**
   * @brief A span of values that can be used to pass arguments to function.
   *
   * For C++20 we should consider to replace it with std::span.
   */
  template <typename T>
  struct Span final {
    constexpr Span(std::initializer_list<T> il) noexcept : m_data{const_cast<T *>(il.begin())}, m_size{il.size()} {}
    constexpr Span(T *data, size_t size) noexcept : m_data{data}, m_size{size} {}

    [[nodiscard]] constexpr T *begin() const noexcept {
      return m_data;
    }

    [[nodiscard]] constexpr T *end() const noexcept {
      return m_data + m_size;
    }

    [[nodiscard]] constexpr size_t size() const noexcept {
      return m_size;
    }

   private:
    T *const m_data;
    size_t const m_size;
  };

  napi_value CallFunction(napi_value thisArg, napi_value function, Span<napi_value> args = {}) const;

  napi_value ConstructObject(napi_value constructor, Span<napi_value> args = {}) const;

  napi_value CreateFunction(const char *utf8Name, size_t nameLength, napi_callback callback, void *callbackData) const;

  /**
   * @brief  Sets the runtime of the current context to an exception state.
   *
   * It returns \c false in case if the current context is already in an exception state.
   */
  bool SetException(napi_value error) const noexcept;

  /**
   * @brief  Sets the runtime of the current context to an exception state.
   */
  bool SetException(StringView message) const noexcept;

 private:
  napi_env m_env;
};

} // namespace napijsi
