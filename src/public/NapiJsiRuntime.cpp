// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "NapiJsiRuntime.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include "compat.h"

#define NAPIJSI_SCOPE(env) EnvScope env_scope_{env};

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

struct NapiApi;

/**
 * @brief A wrapper for N-API.
 *
 * The NapiApi class wraps up the N-API functions in a way that:
 * - functions throw exceptions instead of returning error code (derived class
 * can define the exception types);
 * - standard library types are used when possible to simplify usage.
 *
 * Currently we only wrap up functions that are needed to implement the JSI API.
 */
struct NapiApi {
  explicit NapiApi(napi_env env) noexcept;
  /**
   * @brief A smart pointer for napi_ext_ref.
   *
   * napi_ext_ref is a reference to objects owned by the garbage collector.
   * NapiRefHolder ensures that napi_ext_ref is automatically deleted.
   */
  struct NapiRefHolder final {
    NapiRefHolder(std::nullptr_t = nullptr) noexcept {}
    explicit NapiRefHolder(NapiApi *napi, napi_ext_ref ref) noexcept;
    explicit NapiRefHolder(NapiApi *napi, napi_value value) noexcept;

    // The class is movable.
    NapiRefHolder(NapiRefHolder &&other) noexcept;
    NapiRefHolder &operator=(NapiRefHolder &&other) noexcept;

    // The class is not copyable.
    NapiRefHolder &operator=(NapiRefHolder const &other) = delete;
    NapiRefHolder(NapiRefHolder const &other) = delete;

    ~NapiRefHolder() noexcept;

    [[nodiscard]] napi_ext_ref CloneRef() const noexcept;
    operator napi_value() const;

    explicit operator bool() const noexcept;

   private:
    NapiApi *m_napi{};
    napi_ext_ref m_ref{};
  };

  [[noreturn]] virtual void ThrowJsExceptionOverride(napi_status errorCode, napi_value jsError) const;
  [[noreturn]] virtual void ThrowNativeExceptionOverride(char const *errorMessage) const;
  [[noreturn]] void ThrowJsException(napi_status errorCode) const;
  [[noreturn]] void ThrowNativeException(char const *errorMessage) const;
  napi_ext_ref CreateReference(napi_value value) const;
  void ReleaseReference(napi_ext_ref ref) const;
  napi_value GetReferenceValue(napi_ext_ref ref) const;
  napi_value GetPropertyIdFromName(string_view name) const;
  napi_value GetPropertyIdFromSymbol(string_view symbolDescription) const;
  napi_value GetUndefined() const;
  napi_value GetNull() const;
  napi_value GetGlobal() const;
  napi_value GetBoolean(bool value) const;
  bool GetValueBool(napi_value value) const;
  napi_valuetype TypeOf(napi_value value) const;
  napi_value CreateDouble(double value) const;
  napi_value CreateInt32(int32_t value) const;
  double GetValueDouble(napi_value value) const;
  bool IsArray(napi_value value) const;
  bool IsArrayBuffer(napi_value value) const;
  bool IsFunction(napi_value value) const;
  napi_value CreateStringLatin1(string_view value) const;
  napi_value CreateStringUtf8(string_view value) const;
  napi_ext_ref GetUniqueStringUtf8(string_view value) const;
  std::string PropertyIdToStdString(napi_value propertyId) const;
  std::string StringToStdString(napi_value stringValue) const;
  napi_value GetGlobalObject() const;
  napi_value CreateObject() const;
  napi_value CreateExternalObject(void *data, napi_finalize finalizeCallback) const;

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
  napi_value GetProperty(napi_value object, napi_value propertyId) const;
  void SetProperty(napi_value object, napi_value propertyId, napi_value value) const;
  bool HasProperty(napi_value object, napi_value propertyId) const;
  void DefineProperty(napi_value object, napi_value propertyId, napi_property_descriptor const &descriptor) const;
  void SetElement(napi_value object, uint32_t index, napi_value value) const;
  bool StrictEquals(napi_value left, napi_value right) const;
  void *GetExternalData(napi_value object) const;
  napi_value CreateArray(size_t length) const;
  napi_value CallFunction(napi_value thisArg, napi_value function, span<napi_value> args = {}) const;
  napi_value ConstructObject(napi_value constructor, span<napi_value> args = {}) const;
  napi_value CreateFunction(const char *utf8Name, size_t nameLength, napi_callback callback, void *callbackData) const;
  bool SetException(napi_value error) const noexcept;
  bool SetException(string_view message) const noexcept;

 private:
  // TODO: [vmoroz] Add ref count for the environment
  napi_env m_env;
};

struct NapiJsiRuntimeArgs {};

struct EnvHolder {
  EnvHolder(napi_env env) : env_{env} {}

  ~EnvHolder() {
    napi_ext_env_unref(env_);
  }

 private:
  napi_env env_{nullptr};
};

// Implementation of N-API JSI Runtime
class NapiJsiRuntime : public facebook::jsi::Runtime, NapiApi {
 public:
  NapiJsiRuntime(napi_env env) noexcept;
  ~NapiJsiRuntime() noexcept;

#pragma region Functions_inherited_from_Runtime

  facebook::jsi::Value evaluateJavaScript(
      const std::shared_ptr<const facebook::jsi::Buffer> &buffer,
      const std::string &sourceURL) override;

  std::shared_ptr<const facebook::jsi::PreparedJavaScript> prepareJavaScript(
      const std::shared_ptr<const facebook::jsi::Buffer> &buffer,
      std::string sourceURL) override;

  facebook::jsi::Value evaluatePreparedJavaScript(
      const std::shared_ptr<const facebook::jsi::PreparedJavaScript> &js) override;

  facebook::jsi::Object global() override;

  std::string description() override;

  bool isInspectable() override;

  // We use the default instrumentation() implementation that returns an
  // Instrumentation instance which returns no metrics.

 private:
  // Despite the name "clone" suggesting a deep copy, a return value of these
  // functions points to a new heap allocated ChakraPointerValue whose member
  // JsRefHolder refers to the same JavaScript object as the member
  // JsRefHolder of *pointerValue. This behavior is consistent with that of
  // HermesRuntime and JSCRuntime. Also, Like all ChakraPointerValues, the
  // return value must only be used as an argument to the constructor of
  // jsi::Pointer or one of its derived classes.
  PointerValue *cloneSymbol(const PointerValue *pointerValue) override;
  PointerValue *cloneString(const PointerValue *pointerValue) override;
  PointerValue *cloneObject(const PointerValue *pointerValue) override;
  PointerValue *clonePropNameID(const PointerValue *pointerValue) override;

  facebook::jsi::PropNameID createPropNameIDFromAscii(const char *str, size_t length) override;
  facebook::jsi::PropNameID createPropNameIDFromUtf8(const uint8_t *utf8, size_t length) override;
  facebook::jsi::PropNameID createPropNameIDFromString(const facebook::jsi::String &str) override;
  std::string utf8(const facebook::jsi::PropNameID &id) override;
  bool compare(const facebook::jsi::PropNameID &lhs, const facebook::jsi::PropNameID &rhs) override;

  std::string symbolToString(const facebook::jsi::Symbol &s) override;

  facebook::jsi::String createStringFromAscii(const char *str, size_t length) override;
  facebook::jsi::String createStringFromUtf8(const uint8_t *utf8, size_t length) override;
  std::string utf8(const facebook::jsi::String &str) override;

  facebook::jsi::Object createObject() override;
  facebook::jsi::Object createObject(std::shared_ptr<facebook::jsi::HostObject> ho) override;
  std::shared_ptr<facebook::jsi::HostObject> getHostObject(const facebook::jsi::Object &) override;
  facebook::jsi::HostFunctionType &getHostFunction(const facebook::jsi::Function &) override;

  facebook::jsi::Value getProperty(const facebook::jsi::Object &obj, const facebook::jsi::PropNameID &name) override;
  facebook::jsi::Value getProperty(const facebook::jsi::Object &obj, const facebook::jsi::String &name) override;
  bool hasProperty(const facebook::jsi::Object &obj, const facebook::jsi::PropNameID &name) override;
  bool hasProperty(const facebook::jsi::Object &obj, const facebook::jsi::String &name) override;
  void setPropertyValue(
      facebook::jsi::Object &obj,
      const facebook::jsi::PropNameID &name,
      const facebook::jsi::Value &value) override;
  void setPropertyValue(
      facebook::jsi::Object &obj,
      const facebook::jsi::String &name,
      const facebook::jsi::Value &value) override;

  bool isArray(const facebook::jsi::Object &obj) const override;
  bool isArrayBuffer(const facebook::jsi::Object &obj) const override;
  bool isFunction(const facebook::jsi::Object &obj) const override;
  bool isHostObject(const facebook::jsi::Object &obj) const override;
  bool isHostFunction(const facebook::jsi::Function &func) const override;
  // Returns the names of all enumerable properties of an object. This
  // corresponds the properties iterated through by the JavaScript for..in loop.
  facebook::jsi::Array getPropertyNames(const facebook::jsi::Object &obj) override;

  facebook::jsi::WeakObject createWeakObject(const facebook::jsi::Object &obj) override;
  facebook::jsi::Value lockWeakObject(facebook::jsi::WeakObject &weakObj) override;

  facebook::jsi::Array createArray(size_t length) override;
  size_t size(const facebook::jsi::Array &arr) override;
  size_t size(const facebook::jsi::ArrayBuffer &arrBuf) override;
  // The lifetime of the buffer returned is the same as the lifetime of the
  // ArrayBuffer. The returned buffer pointer does not count as a reference to
  // the ArrayBuffer for the purpose of garbage collection.
  uint8_t *data(const facebook::jsi::ArrayBuffer &arrBuf) override;
  facebook::jsi::Value getValueAtIndex(const facebook::jsi::Array &arr, size_t index) override;
  void setValueAtIndexImpl(facebook::jsi::Array &arr, size_t index, const facebook::jsi::Value &value) override;

  facebook::jsi::Function createFunctionFromHostFunction(
      const facebook::jsi::PropNameID &name,
      unsigned int paramCount,
      facebook::jsi::HostFunctionType func) override;
  facebook::jsi::Value call(
      const facebook::jsi::Function &func,
      const facebook::jsi::Value &jsThis,
      const facebook::jsi::Value *args,
      size_t count) override;
  facebook::jsi::Value
  callAsConstructor(const facebook::jsi::Function &func, const facebook::jsi::Value *args, size_t count) override;

  // For now, pushing a scope does nothing, and popping a scope forces the
  // JavaScript garbage collector to run.
  ScopeState *pushScope() override;
  void popScope(ScopeState *) override;

  bool strictEquals(const facebook::jsi::Symbol &a, const facebook::jsi::Symbol &b) const override;
  bool strictEquals(const facebook::jsi::String &a, const facebook::jsi::String &b) const override;
  bool strictEquals(const facebook::jsi::Object &a, const facebook::jsi::Object &b) const override;

  bool instanceOf(const facebook::jsi::Object &obj, const facebook::jsi::Function &func) override;

#pragma endregion Functions_inherited_from_Runtime

 protected:
  NapiJsiRuntimeArgs &runtimeArgs() {
    return m_args;
  }

 private: //  ChakraApi::IExceptionThrower members
  [[noreturn]] void ThrowJsExceptionOverride(napi_status errorCode, napi_value jsError) const override;
  [[noreturn]] void ThrowNativeExceptionOverride(char const *errorMessage) const override;
  void RewriteErrorMessage(napi_value jsError) const;

 private:
  // NapiPointerValueView is the base class for NapiPointerValue.
  // It holds either napi_value or napi_ext_ref. It does nothing in the
  // invalidate() method. It is used directly by the JsiValueView,
  // JsiValueViewArray, and JsiPropNameIDView classes to keep temporary
  // PointerValues on the call stack to avoid extra memory allocations. In that
  // case it is assumed that it holds a napi_value
  struct NapiPointerValueView : PointerValue {
    NapiPointerValueView(NapiApi const *napi, void *valueOrRef) noexcept : m_napi{napi}, m_valueOrRef{valueOrRef} {}

    NapiPointerValueView(NapiPointerValueView const &) = delete;
    NapiPointerValueView &operator=(NapiPointerValueView const &) = delete;

    // Intentionally do nothing in the invalidate() method.
    void invalidate() noexcept override {}

    virtual napi_value GetValue() const {
      return reinterpret_cast<napi_value>(m_valueOrRef);
    }

    /*[[noreturn]]*/ virtual napi_ext_ref GetRef() const {
      NapiVerifyElseCrash(false, "Not implemented");
      return nullptr;
    }

    NapiApi const *GetNapi() const noexcept {
      return m_napi;
    }

   private:
    // TODO: [vmoroz] How to make it safe to hold the m_api? Is there a way to
    // use weak pointers?
    NapiApi const *m_napi;
    void *m_valueOrRef;
  };

  // NapiPointerValue is needed for working with Facebook's jsi::Pointer class
  // and must only be used for this purpose. Every instance of
  // NapiPointerValue should be allocated on the heap and be used as an
  // argument to the constructor of jsi::Pointer or one of its derived classes.
  // Pointer makes sure that invalidate(), which frees the heap allocated
  // NapiPointerValue, is called upon destruction. Since the constructor of
  // jsi::Pointer is protected, we usually have to invoke it through
  // jsi::Runtime::make. The code should look something like:
  //
  //     make<Pointer>(new NapiPointerValue(...));
  //
  // or you can use the helper function MakePointer(), as defined below.
  struct NapiPointerValue final : NapiPointerValueView {
    NapiPointerValue(NapiApi const *napi, napi_ext_ref ref) : NapiPointerValueView{napi, ref} {}

    NapiPointerValue(NapiApi const *napi, napi_value value) noexcept
        : NapiPointerValueView{napi, napi->CreateReference(value)} {}

    napi_value GetValue() const override {
      return GetNapi()->GetReferenceValue(GetRef());
    }

    napi_ext_ref GetRef() const override {
      return reinterpret_cast<napi_ext_ref>(NapiPointerValueView::GetValue());
    }

    void invalidate() noexcept override {
      delete this;
    }

   private:
    // ~NapiPointerValue() should only be invoked by invalidate().
    // Hence we make it private.
    ~NapiPointerValue() noexcept override {
      if (napi_ext_ref ref = GetRef()) {
        GetNapi()->ReleaseReference(ref);
      }
    }
  };

  template <typename T, std::enable_if_t<std::is_base_of_v<facebook::jsi::Pointer, T>, int> = 0>
  T MakePointer(napi_ext_ref ref) const {
    return make<T>(new NapiPointerValue(this, ref));
  }

  template <typename T, std::enable_if_t<std::is_base_of_v<facebook::jsi::Pointer, T>, int> = 0>
  T MakePointer(napi_value value) const {
    return make<T>(new NapiPointerValue(this, value));
  }

  // The pointer passed to this function must point to a NapiPointerValue.
  static NapiPointerValue *CloneNapiPointerValue(const PointerValue *pointerValue) {
    auto napiPointerValue = static_cast<const NapiPointerValueView *>(pointerValue);
    return new NapiPointerValue(napiPointerValue->GetNapi(), napiPointerValue->GetValue());
  }

  // The jsi::Pointer passed to this function must hold a NapiPointerValue.
  static napi_value GetNapiValue(const facebook::jsi::Pointer &p) {
    return static_cast<const NapiPointerValueView *>(getPointerValue(p))->GetValue();
  }

  static napi_ext_ref GetNapiRef(const facebook::jsi::Pointer &p) {
    return static_cast<const NapiPointerValueView *>(getPointerValue(p))->GetRef();
  }

  // These three functions only performs shallow copies.
  facebook::jsi::Value ToJsiValue(napi_value value) const;
  napi_value ToNapiValue(const facebook::jsi::Value &value) const;

  napi_value
  CreateExternalFunction(napi_value name, int32_t paramCount, napi_callback nativeFunction, void *callbackState);

  // Host function helper
  static napi_value HostFunctionCall(napi_env env, napi_callback_info info) noexcept;

  // Host object helpers; runtime must be referring to a ChakraRuntime.
  static napi_value HostObjectGetTrap(napi_env env, napi_callback_info info) noexcept;

  static napi_value HostObjectSetTrap(napi_env env, napi_callback_info info) noexcept;

  static napi_value HostObjectOwnKeysTrap(napi_env env, napi_callback_info info) noexcept;

  static napi_value HostObjectGetOwnPropertyDescriptorTrap(napi_env env, napi_callback_info info) noexcept;

  napi_value GetHostObjectProxyHandler();

  // Evaluate lambda and augment exception messages with the methodName.
  template <typename TLambda>
  auto RunInMethodContext(char const *methodName, TLambda lambda) {
    try {
      return lambda();
    } catch (facebook::jsi::JSError const &) {
      throw; // do not augment the JSError exceptions.
    } catch (std::exception const &ex) {
      NapiThrow((std::string{"Exception in "} + methodName + ": " + ex.what()).c_str());
    } catch (...) {
      NapiThrow((std::string{"Exception in "} + methodName + ": <unknown>").c_str());
    }
  }

  // Evaluate lambda and convert all exceptions to Chakra engine exceptions.
  template <typename TLambda>
  napi_value HandleCallbackExceptions(TLambda lambda) const noexcept {
    try {
      try {
        return lambda();
      } catch (facebook::jsi::JSError const &jsError) {
        // This block may throw exceptions
        SetException(ToNapiValue(jsError.value()));
      }
    } catch (std::exception const &ex) {
      SetException(ex.what());
    } catch (...) {
      SetException("Unexpected error");
    }

    return m_value.Undefined;
  }

  std::unique_ptr<facebook::jsi::Buffer> GeneratePreparedScript(
      facebook::jsi::Buffer const &sourceBuffer,
      const char *sourceUrl);

  enum class PropertyAttributes {
    None = 0,
    ReadOnly = 1 << 1,
    DontEnum = 1 << 2,
    DontDelete = 1 << 3,
    Frozen = ReadOnly | DontDelete,
    DontEnumAndFrozen = DontEnum | Frozen,
  };

  friend constexpr PropertyAttributes operator&(PropertyAttributes left, PropertyAttributes right) {
    return (PropertyAttributes)((int)left & (int)right);
  }

  friend constexpr bool operator!(PropertyAttributes attrs) {
    return attrs == PropertyAttributes::None;
  }

  napi_value CreatePropertyDescriptor(napi_value value, PropertyAttributes attrs);

  // Keep CallstackSize elements on the callstack, otherwise allocate memory in
  // heap.
  template <typename T, size_t CallstackSize>
  struct SmallBuffer final {
    SmallBuffer(size_t size) noexcept
        : m_size{size}, m_heapData{m_size > CallstackSize ? std::make_unique<T[]>(m_size) : nullptr} {}

    T *Data() noexcept {
      return m_heapData ? m_heapData.get() : m_stackData.data();
    }

    size_t Size() const noexcept {
      return m_size;
    }

   private:
    size_t const m_size{};
    std::array<T, CallstackSize> m_stackData{};
    std::unique_ptr<T[]> const m_heapData{};
  };

  // The number of arguments that we keep on stack.
  // We use heap if we have more argument.
  constexpr static size_t MaxStackArgCount = 8;

  // NapiValueArgs helps to optimize passing arguments to NAPI function.
  // If number of arguments is below or equal to MaxStackArgCount,
  // then they are kept on call stack, otherwise arguments are allocated on
  // heap.
  struct NapiValueArgs final {
    NapiValueArgs(NapiJsiRuntime &rt, span<facebook::jsi::Value const> args);
    operator span<napi_value>();

   private:
    size_t const m_count{};
    SmallBuffer<napi_value, MaxStackArgCount> m_args;
  };

  // This type represents a view to Value based on JsValueRef.
  // It avoids extra memory allocation by using an in-place storage.
  // It uses ChakraPointerValueView that does nothing in the invalidate()
  // method.
  struct JsiValueView final {
    JsiValueView(NapiApi *napi, napi_value jsValue);
    ~JsiValueView() noexcept;
    operator facebook::jsi::Value const &() const noexcept;

    using StoreType = std::aligned_storage_t<sizeof(NapiPointerValueView)>;
    static facebook::jsi::Value InitValue(NapiApi *napi, napi_value jsValue, StoreType *store);

   private:
    StoreType m_pointerStore{};
    facebook::jsi::Value const m_value{};
  };

  // This class helps to use stack storage for passing arguments that must be
  // temporary converted from JsValueRef to facebook::jsi::Value.
  struct JsiValueViewArgs final {
    JsiValueViewArgs(NapiApi *napi, span<napi_value> args) noexcept;
    facebook::jsi::Value const *Data() noexcept;
    size_t Size() const noexcept;

   private:
    size_t const m_size{};
    SmallBuffer<JsiValueView::StoreType, MaxStackArgCount> m_pointerStore{0};
    SmallBuffer<facebook::jsi::Value, MaxStackArgCount> m_args{0};
  };

  // PropNameIDView helps to use the stack storage for temporary conversion from
  // JsPropertyIdRef to facebook::jsi::PropNameID.
  struct PropNameIDView final {
    PropNameIDView(NapiApi *napi, napi_value propertyId) noexcept;
    ~PropNameIDView() noexcept;
    operator facebook::jsi::PropNameID const &() const noexcept;

    using StoreType = std::aligned_storage_t<sizeof(NapiPointerValueView)>;

   private:
    StoreType m_pointerStore{};
    facebook::jsi::PropNameID const m_propertyId;
  };

 private:
  EnvHolder m_envHolder;

  // Property ID cache to improve execution speed
  struct PropertyId {
    NapiRefHolder Error;
    NapiRefHolder Object;
    NapiRefHolder Proxy;
    NapiRefHolder Symbol;
    NapiRefHolder byteLength;
    NapiRefHolder configurable;
    NapiRefHolder enumerable;
    NapiRefHolder get;
    NapiRefHolder getOwnPropertyDescriptor;
    NapiRefHolder hostFunctionSymbol;
    NapiRefHolder hostObjectSymbol;
    NapiRefHolder length;
    NapiRefHolder message;
    NapiRefHolder ownKeys;
    NapiRefHolder propertyIsEnumerable;
    NapiRefHolder prototype;
    NapiRefHolder set;
    NapiRefHolder toString;
    NapiRefHolder value;
    NapiRefHolder writable;
  } m_propertyId;

  struct CachedValue final {
    NapiRefHolder Error;
    NapiRefHolder Global;
    NapiRefHolder False;
    NapiRefHolder HostObjectProxyHandler;
    NapiRefHolder Null;
    NapiRefHolder ProxyConstructor;
    NapiRefHolder True;
    NapiRefHolder Undefined;
  } m_value;

  static std::once_flag s_runtimeVersionInitFlag;
  static uint64_t s_runtimeVersion;

  // Arguments shared by the specializations
  NapiJsiRuntimeArgs m_args;

  napi_env m_env;
  bool m_pendingJSError{false};
};

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

NapiApi::NapiApi(napi_env env) noexcept : m_env{env} {}

//=============================================================================
// NapiApi::JsRefHolder implementation
//=============================================================================

NapiApi::NapiRefHolder::NapiRefHolder(NapiApi *napi, napi_ext_ref ref) noexcept : m_napi{napi}, m_ref{ref} {}

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

napi_value NapiApi::GetPropertyIdFromName(string_view name) const {
  napi_value propertyId{};
  CHECK_NAPI(napi_create_string_utf8(m_env, name.data(), name.size(), &propertyId));
  return propertyId;
}

napi_value NapiApi::GetPropertyIdFromSymbol(string_view symbolDescription) const {
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

napi_value NapiApi::CreateStringLatin1(string_view value) const {
  NapiVerifyElseThrow(value.data(), "Cannot convert a nullptr to a JS string.");

  napi_value result{};
  NapiVerifyJsErrorElseThrow(napi_create_string_latin1(m_env, value.data(), value.size(), &result));
  return result;
}

napi_value NapiApi::CreateStringUtf8(string_view value) const {
  NapiVerifyElseThrow(value.data(), "Cannot convert a nullptr to a JS string.");

  napi_value result{};
  NapiVerifyJsErrorElseThrow(napi_create_string_utf8(m_env, value.data(), value.size(), &result));
  return result;
}

// Gets or creates a unique string value from an UTF-8 std::string_view.
napi_ext_ref NapiApi::GetUniqueStringUtf8(string_view value) const {
  napi_ext_ref ref{};
  NapiVerifyJsErrorElseThrow(napi_ext_get_unique_utf8_string_ref(m_env, value.data(), value.size(), &ref));
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
  CHECK_NAPI(napi_get_value_string_utf8(m_env, stringValue, nullptr, 0, &strLength));
  result.assign(strLength, '\0');
  size_t copiedLength{};
  CHECK_NAPI(napi_get_value_string_utf8(m_env, stringValue, &result[0], result.length() + 1, &copiedLength));
  NapiVerifyElseThrow(result.length() == copiedLength, "Unexpected string length");
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

napi_value NapiApi::CreateExternalObject(void *data, napi_finalize finalizeCallback) const {
  napi_value result{};
  CHECK_NAPI(napi_create_external(m_env, data, finalizeCallback, nullptr, &result));
  return result;
}

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

napi_value NapiApi::CallFunction(napi_value thisArg, napi_value function, span<napi_value> args) const {
  napi_value result{};
  CHECK_NAPI(napi_call_function(m_env, thisArg, function, args.size(), args.begin(), &result));
  return result;
}

napi_value NapiApi::ConstructObject(napi_value constructor, span<napi_value> args) const {
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

bool NapiApi::SetException(string_view message) const noexcept {
  return napi_throw_error(m_env, "Unknown", message.data()) == napi_status::napi_ok;
}

//=============================================================================
// NapiJsiRuntime implementation
//=============================================================================

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
      ToNapiValue(jsThis), GetNapiValue(func), NapiValueArgs(*this, span<facebook::jsi::Value const>(args, count))));
}

facebook::jsi::Value
NapiJsiRuntime::callAsConstructor(const facebook::jsi::Function &func, const facebook::jsi::Value *args, size_t count) {
  NAPIJSI_SCOPE(m_env);
  return ToJsiValue(
      ConstructObject(GetNapiValue(func), NapiValueArgs(*this, span<facebook::jsi::Value const>(args, count))));
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

NapiJsiRuntime::NapiValueArgs::NapiValueArgs(NapiJsiRuntime &rt, span<facebook::jsi::Value const> args)
    : m_count{args.size()}, m_args{m_count} {
  napi_value *const jsArgs = m_args.Data();
  for (size_t i = 0; i < m_count; ++i) {
    jsArgs[i] = rt.ToNapiValue(args[i]);
  }
}

NapiJsiRuntime::NapiValueArgs::operator span<napi_value>() {
  return span<napi_value>(m_args.Data(), m_count);
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

NapiJsiRuntime::JsiValueViewArgs::JsiValueViewArgs(NapiApi *napi, span<napi_value> args) noexcept
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
