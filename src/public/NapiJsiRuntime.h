// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "NapiApi.h"

#include <jsi/jsi.h>

#include <array>
#include <mutex>
#include <sstream>

namespace napijsi {

std::unique_ptr<facebook::jsi::Runtime> MakeNapiJsiRuntime(
    napi_env env) noexcept;

struct NapiJsiRuntimeArgs {};

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
      const std::shared_ptr<const facebook::jsi::PreparedJavaScript> &js)
      override;

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

  facebook::jsi::PropNameID createPropNameIDFromAscii(
      const char *str,
      size_t length) override;
  facebook::jsi::PropNameID createPropNameIDFromUtf8(
      const uint8_t *utf8,
      size_t length) override;
  facebook::jsi::PropNameID createPropNameIDFromString(
      const facebook::jsi::String &str) override;
  std::string utf8(const facebook::jsi::PropNameID &id) override;
  bool compare(
      const facebook::jsi::PropNameID &lhs,
      const facebook::jsi::PropNameID &rhs) override;

  std::string symbolToString(const facebook::jsi::Symbol &s) override;

  facebook::jsi::String createStringFromAscii(const char *str, size_t length)
      override;
  facebook::jsi::String createStringFromUtf8(const uint8_t *utf8, size_t length)
      override;
  std::string utf8(const facebook::jsi::String &str) override;

  facebook::jsi::Object createObject() override;
  facebook::jsi::Object createObject(
      std::shared_ptr<facebook::jsi::HostObject> ho) override;
  std::shared_ptr<facebook::jsi::HostObject> getHostObject(
      const facebook::jsi::Object &) override;
  facebook::jsi::HostFunctionType &getHostFunction(
      const facebook::jsi::Function &) override;

  facebook::jsi::Value getProperty(
      const facebook::jsi::Object &obj,
      const facebook::jsi::PropNameID &name) override;
  facebook::jsi::Value getProperty(
      const facebook::jsi::Object &obj,
      const facebook::jsi::String &name) override;
  bool hasProperty(
      const facebook::jsi::Object &obj,
      const facebook::jsi::PropNameID &name) override;
  bool hasProperty(
      const facebook::jsi::Object &obj,
      const facebook::jsi::String &name) override;
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
  facebook::jsi::Array getPropertyNames(
      const facebook::jsi::Object &obj) override;

  facebook::jsi::WeakObject createWeakObject(
      const facebook::jsi::Object &obj) override;
  facebook::jsi::Value lockWeakObject(
      facebook::jsi::WeakObject &weakObj) override;

  facebook::jsi::Array createArray(size_t length) override;
  size_t size(const facebook::jsi::Array &arr) override;
  size_t size(const facebook::jsi::ArrayBuffer &arrBuf) override;
  // The lifetime of the buffer returned is the same as the lifetime of the
  // ArrayBuffer. The returned buffer pointer does not count as a reference to
  // the ArrayBuffer for the purpose of garbage collection.
  uint8_t *data(const facebook::jsi::ArrayBuffer &arrBuf) override;
  facebook::jsi::Value getValueAtIndex(
      const facebook::jsi::Array &arr,
      size_t index) override;
  void setValueAtIndexImpl(
      facebook::jsi::Array &arr,
      size_t index,
      const facebook::jsi::Value &value) override;

  facebook::jsi::Function createFunctionFromHostFunction(
      const facebook::jsi::PropNameID &name,
      unsigned int paramCount,
      facebook::jsi::HostFunctionType func) override;
  facebook::jsi::Value call(
      const facebook::jsi::Function &func,
      const facebook::jsi::Value &jsThis,
      const facebook::jsi::Value *args,
      size_t count) override;
  facebook::jsi::Value callAsConstructor(
      const facebook::jsi::Function &func,
      const facebook::jsi::Value *args,
      size_t count) override;

  // For now, pushing a scope does nothing, and popping a scope forces the
  // JavaScript garbage collector to run.
  ScopeState *pushScope() override;
  void popScope(ScopeState *) override;

  bool strictEquals(
      const facebook::jsi::Symbol &a,
      const facebook::jsi::Symbol &b) const override;
  bool strictEquals(
      const facebook::jsi::String &a,
      const facebook::jsi::String &b) const override;
  bool strictEquals(
      const facebook::jsi::Object &a,
      const facebook::jsi::Object &b) const override;

  bool instanceOf(
      const facebook::jsi::Object &obj,
      const facebook::jsi::Function &func) override;

#pragma endregion Functions_inherited_from_Runtime

 protected:
  NapiJsiRuntimeArgs &runtimeArgs() {
    return m_args;
  }

 private: //  ChakraApi::IExceptionThrower members
  [[noreturn]] void ThrowJsExceptionOverride(
      napi_status errorCode,
      napi_value jsError) const override;
  [[noreturn]] void ThrowNativeExceptionOverride(
      char const *errorMessage) const override;
  void RewriteErrorMessage(napi_value jsError) const;

 private:
  // NapiPointerValueView is the base class for NapiPointerValue.
  // It holds either napi_value or napi_ext_ref. It does nothing in the
  // invalidate() method. It is used directly by the JsiValueView,
  // JsiValueViewArray, and JsiPropNameIDView classes to keep temporary
  // PointerValues on the call stack to avoid extra memory allocations. In that
  // case it is assumed that it holds a napi_value
  struct NapiPointerValueView : PointerValue {
    NapiPointerValueView(NapiApi const *napi, void *valueOrRef) noexcept
        : m_napi{napi}, m_valueOrRef{valueOrRef} {}

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
    NapiPointerValue(NapiApi const *napi, napi_ext_ref ref)
        : NapiPointerValueView{napi, ref} {}

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

  template <
      typename T,
      std::enable_if_t<std::is_base_of_v<facebook::jsi::Pointer, T>, int> = 0>
  T MakePointer(napi_ext_ref ref) const {
    return make<T>(new NapiPointerValue(this, ref));
  }

  template <
      typename T,
      std::enable_if_t<std::is_base_of_v<facebook::jsi::Pointer, T>, int> = 0>
  T MakePointer(napi_value value) const {
    return make<T>(new NapiPointerValue(this, value));
  }

  // The pointer passed to this function must point to a NapiPointerValue.
  static NapiPointerValue *CloneNapiPointerValue(
      const PointerValue *pointerValue) {
    auto napiPointerValue =
        static_cast<const NapiPointerValueView *>(pointerValue);
    return new NapiPointerValue(
        napiPointerValue->GetNapi(), napiPointerValue->GetValue());
  }

  // The jsi::Pointer passed to this function must hold a NapiPointerValue.
  static napi_value GetNapiValue(const facebook::jsi::Pointer &p) {
    return static_cast<const NapiPointerValueView *>(getPointerValue(p))
        ->GetValue();
  }

  static napi_ext_ref GetNapiRef(const facebook::jsi::Pointer &p) {
    return static_cast<const NapiPointerValueView *>(getPointerValue(p))
        ->GetRef();
  }

  // These three functions only performs shallow copies.
  facebook::jsi::Value ToJsiValue(napi_value value) const;
  napi_value ToNapiValue(const facebook::jsi::Value &value) const;

  napi_value CreateExternalFunction(
      napi_value name,
      int32_t paramCount,
      napi_callback nativeFunction,
      void *callbackState);

  // Host function helper
  static napi_value HostFunctionCall(
      napi_env env,
      napi_callback_info info) noexcept;

  // Host object helpers; runtime must be referring to a ChakraRuntime.
  static napi_value HostObjectGetTrap(
      napi_env env,
      napi_callback_info info) noexcept;

  static napi_value HostObjectSetTrap(
      napi_env env,
      napi_callback_info info) noexcept;

  static napi_value HostObjectOwnKeysTrap(
      napi_env env,
      napi_callback_info info) noexcept;

  static napi_value HostObjectGetOwnPropertyDescriptorTrap(
      napi_env env,
      napi_callback_info info) noexcept;

  napi_value GetHostObjectProxyHandler();

  // Evaluate lambda and augment exception messages with the methodName.
  template <typename TLambda>
  auto RunInMethodContext(char const *methodName, TLambda lambda) {
    try {
      return lambda();
    } catch (facebook::jsi::JSError const &) {
      throw; // do not augment the JSError exceptions.
    } catch (std::exception const &ex) {
      NapiThrow((std::string{"Exception in "} + methodName + ": " + ex.what())
                    .c_str());
    } catch (...) {
      NapiThrow(
          (std::string{"Exception in "} + methodName + ": <unknown>").c_str());
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

  friend constexpr PropertyAttributes operator&(
      PropertyAttributes left,
      PropertyAttributes right) {
    return (PropertyAttributes)((int)left & (int)right);
  }

  friend constexpr bool operator!(PropertyAttributes attrs) {
    return attrs == PropertyAttributes::None;
  }

  napi_value CreatePropertyDescriptor(
      napi_value value,
      PropertyAttributes attrs);

  // Keep CallstackSize elements on the callstack, otherwise allocate memory in
  // heap.
  template <typename T, size_t CallstackSize>
  struct SmallBuffer final {
    SmallBuffer(size_t size) noexcept
        : m_size{size},
          m_heapData{
              m_size > CallstackSize ? std::make_unique<T[]>(m_size)
                                     : nullptr} {}

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
    NapiValueArgs(NapiJsiRuntime &rt, Span<facebook::jsi::Value const> args);
    operator NapiApi::Span<napi_value>();

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
    static facebook::jsi::Value
    InitValue(NapiApi *napi, napi_value jsValue, StoreType *store);

   private:
    StoreType m_pointerStore{};
    facebook::jsi::Value const m_value{};
  };

  // This class helps to use stack storage for passing arguments that must be
  // temporary converted from JsValueRef to facebook::jsi::Value.
  struct JsiValueViewArgs final {
    JsiValueViewArgs(NapiApi *napi, Span<napi_value> args) noexcept;
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
};

} // namespace napijsi
