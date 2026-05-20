/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT license.
 *
 * Portions derived from facebook/hermes (Hermes ABI):
 *   Copyright (c) Meta Platforms, Inc. and affiliates.
 *   Licensed under the MIT license.
 *
 * JsiAbiRuntime — C++ wrapper that wraps the jsi_runtime C interface
 * into a facebook::jsi::Runtime C++ class. Engine-agnostic.
 */

#ifndef JSI_ABI_RUNTIME_H
#define JSI_ABI_RUNTIME_H

#include "jsi_abi/jsi_abi.h"
#include "jsi_abi/jsi_abi_helpers.h"

#include <jsi/jsi.h>
#include <jsi/instrumentation.h>

#include <memory>
#include <string>

namespace jsi::abi {

/// A facebook::jsi::Runtime implementation backed by the JSI ABI C interface.
/// Takes a jsi_vtable* in the constructor and creates a runtime via
/// create_runtime.
class JsiAbiRuntime : public facebook::jsi::Runtime {
 public:
  /// Create a JsiAbiRuntime from a factory vtable.
  /// \param vtable The top-level JSI ABI vtable (from get_jsi_abi_*_vtable).
  /// \param config Engine-specific config, nullptr for defaults.
  explicit JsiAbiRuntime(
      const jsi_vtable *vtable,
      const void *config = nullptr);

  /// Tag for the non-owning constructor — see below.
  struct AttachToExisting {};

  /// Wrap an existing jsi_runtime without taking ownership of it. Used by
  /// the W8 dual-API scenario (jsr_create_runtime → jsr_runtime_get_jsi_runtime
  /// → JsiAbiRuntime{abiRuntime, AttachToExisting{}}). The caller continues
  /// to own the runtime and is responsible for releasing it; this wrapper's
  /// destructor does NOT call jsi_release.
  JsiAbiRuntime(jsi_runtime *abiRuntime, AttachToExisting);

  ~JsiAbiRuntime() override;

  // Non-copyable, non-movable.
  JsiAbiRuntime(const JsiAbiRuntime &) = delete;
  JsiAbiRuntime &operator=(const JsiAbiRuntime &) = delete;

  facebook::jsi::Value evaluateJavaScript(
      const std::shared_ptr<const facebook::jsi::Buffer> &buffer,
      const std::string &sourceURL) override;

  std::shared_ptr<const facebook::jsi::PreparedJavaScript> prepareJavaScript(
      const std::shared_ptr<const facebook::jsi::Buffer> &buffer,
      std::string sourceURL) override;

  facebook::jsi::Value evaluatePreparedJavaScript(
      const std::shared_ptr<const facebook::jsi::PreparedJavaScript> &js)
      override;

#if JSI_VERSION >= 4
  bool drainMicrotasks(int maxMicrotasksHint = -1) override;
#endif

#if JSI_VERSION >= 12
  void queueMicrotask(const facebook::jsi::Function &callback) override;
#endif

  facebook::jsi::Object global() override;
  std::string description() override;
  bool isInspectable() override;

  /// Direct access to the underlying ABI runtime pointer. Intended for
  /// test-only ABI hooks (see jsi_abi_v8.cpp's v8_jsi_test_post_*) and
  /// for advanced consumers that need to call ABI-specific extensions.
  jsi_runtime *abiRuntime() const noexcept { return abiRt_; }

 protected:
  PointerValue *cloneSymbol(const PointerValue *pv) override;
  PointerValue *cloneString(const PointerValue *pv) override;
#if JSI_VERSION >= 6
  PointerValue *cloneBigInt(const PointerValue *pv) override;
#endif
  PointerValue *cloneObject(const PointerValue *pv) override;
  PointerValue *clonePropNameID(const PointerValue *pv) override;

  facebook::jsi::PropNameID createPropNameIDFromAscii(
      const char *str,
      size_t length) override;
  facebook::jsi::PropNameID createPropNameIDFromUtf8(
      const uint8_t *utf8,
      size_t length) override;
  facebook::jsi::PropNameID createPropNameIDFromString(
      const facebook::jsi::String &str) override;
#if JSI_VERSION >= 5
  facebook::jsi::PropNameID createPropNameIDFromSymbol(
      const facebook::jsi::Symbol &sym) override;
#endif
  std::string utf8(const facebook::jsi::PropNameID &) override;
  bool compare(
      const facebook::jsi::PropNameID &,
      const facebook::jsi::PropNameID &) override;

  std::string symbolToString(const facebook::jsi::Symbol &) override;

#if JSI_VERSION >= 8
  facebook::jsi::BigInt createBigIntFromInt64(int64_t) override;
  facebook::jsi::BigInt createBigIntFromUint64(uint64_t) override;
  bool bigintIsInt64(const facebook::jsi::BigInt &) override;
  bool bigintIsUint64(const facebook::jsi::BigInt &) override;
  uint64_t truncate(const facebook::jsi::BigInt &) override;
  facebook::jsi::String bigintToString(
      const facebook::jsi::BigInt &,
      int) override;
#endif

  facebook::jsi::String createStringFromAscii(
      const char *str,
      size_t length) override;
  facebook::jsi::String createStringFromUtf8(
      const uint8_t *utf8,
      size_t length) override;
  std::string utf8(const facebook::jsi::String &) override;

  facebook::jsi::Object createObject() override;
  facebook::jsi::Object createObject(
      std::shared_ptr<facebook::jsi::HostObject> ho) override;
  std::shared_ptr<facebook::jsi::HostObject> getHostObject(
      const facebook::jsi::Object &) override;
  facebook::jsi::HostFunctionType &getHostFunction(
      const facebook::jsi::Function &) override;

#if JSI_VERSION >= 7
  bool hasNativeState(const facebook::jsi::Object &) override;
  std::shared_ptr<facebook::jsi::NativeState> getNativeState(
      const facebook::jsi::Object &) override;
  void setNativeState(
      const facebook::jsi::Object &,
      std::shared_ptr<facebook::jsi::NativeState> state) override;
#endif

#if JSI_VERSION >= 17
  void setPrototypeOf(
      const facebook::jsi::Object &object,
      const facebook::jsi::Value &prototype) override;
  facebook::jsi::Value getPrototypeOf(
      const facebook::jsi::Object &object) override;
#endif

  facebook::jsi::Value getProperty(
      const facebook::jsi::Object &,
      const facebook::jsi::PropNameID &name) override;
  facebook::jsi::Value getProperty(
      const facebook::jsi::Object &,
      const facebook::jsi::String &name) override;
  bool hasProperty(
      const facebook::jsi::Object &,
      const facebook::jsi::PropNameID &name) override;
  bool hasProperty(
      const facebook::jsi::Object &,
      const facebook::jsi::String &name) override;
  void setPropertyValue(
      JSI_CONST_10 facebook::jsi::Object &,
      const facebook::jsi::PropNameID &name,
      const facebook::jsi::Value &value) override;
  void setPropertyValue(
      JSI_CONST_10 facebook::jsi::Object &,
      const facebook::jsi::String &name,
      const facebook::jsi::Value &value) override;

  bool isArray(const facebook::jsi::Object &) const override;
  bool isArrayBuffer(const facebook::jsi::Object &) const override;
  bool isFunction(const facebook::jsi::Object &) const override;
  bool isHostObject(const facebook::jsi::Object &) const override;
  bool isHostFunction(const facebook::jsi::Function &) const override;
  facebook::jsi::Array getPropertyNames(
      const facebook::jsi::Object &) override;

  facebook::jsi::WeakObject createWeakObject(
      const facebook::jsi::Object &) override;
  facebook::jsi::Value lockWeakObject(
      JSI_NO_CONST_3 JSI_CONST_10 facebook::jsi::WeakObject &) override;

  facebook::jsi::Array createArray(size_t length) override;
#if JSI_VERSION >= 9
  facebook::jsi::ArrayBuffer createArrayBuffer(
      std::shared_ptr<facebook::jsi::MutableBuffer> buffer) override;
#endif
  size_t size(const facebook::jsi::Array &) override;
  size_t size(const facebook::jsi::ArrayBuffer &) override;
  uint8_t *data(const facebook::jsi::ArrayBuffer &) override;
  facebook::jsi::Value getValueAtIndex(
      const facebook::jsi::Array &,
      size_t i) override;
  void setValueAtIndexImpl(
      JSI_CONST_10 facebook::jsi::Array &,
      size_t i,
      const facebook::jsi::Value &value) override;

  facebook::jsi::Function createFunctionFromHostFunction(
      const facebook::jsi::PropNameID &name,
      unsigned int paramCount,
      facebook::jsi::HostFunctionType func) override;
  facebook::jsi::Value call(
      const facebook::jsi::Function &,
      const facebook::jsi::Value &jsThis,
      const facebook::jsi::Value *args,
      size_t count) override;
  facebook::jsi::Value callAsConstructor(
      const facebook::jsi::Function &,
      const facebook::jsi::Value *args,
      size_t count) override;

  bool strictEquals(
      const facebook::jsi::Symbol &a,
      const facebook::jsi::Symbol &b) const override;
#if JSI_VERSION >= 6
  bool strictEquals(
      const facebook::jsi::BigInt &a,
      const facebook::jsi::BigInt &b) const override;
#endif
  bool strictEquals(
      const facebook::jsi::String &a,
      const facebook::jsi::String &b) const override;
  bool strictEquals(
      const facebook::jsi::Object &a,
      const facebook::jsi::Object &b) const override;

  bool instanceOf(
      const facebook::jsi::Object &o,
      const facebook::jsi::Function &f) override;

#if JSI_VERSION >= 11
  void setExternalMemoryPressure(
      const facebook::jsi::Object &,
      size_t) override;
#endif

  facebook::jsi::Instrumentation &instrumentation() override;

  ScopeState *pushScope() override;
  void popScope(ScopeState *) override;

 private:
  class ManagedPointerHolder;
  class HostFunctionWrapper;
  class HostObjectWrapper;
  class NativeStateWrapper;
  class AbiInstrumentation;

  // Extract the underlying jsi_pointer from a JSI PointerValue.
  jsi_pointer *getABIPointer(const PointerValue *pv) const;

  // Convert JSI types to ABI types (aliasing, no ownership transfer).
  jsi_object toABIObject(const facebook::jsi::Object &obj) const;
  jsi_string toABIString(const facebook::jsi::String &str) const;
  jsi_symbol toABISymbol(const facebook::jsi::Symbol &sym) const;
  jsi_propnameid toABIPropNameID(
      const facebook::jsi::PropNameID &name) const;
  jsi_function toABIFunction(const facebook::jsi::Function &fn) const;
  jsi_array toABIArray(const facebook::jsi::Array &arr) const;
  jsi_arraybuffer toABIArrayBuffer(
      const facebook::jsi::ArrayBuffer &ab) const;
  jsi_weak_object toABIWeakObject(
      const facebook::jsi::WeakObject &wo) const;
#if JSI_VERSION >= 6
  jsi_bigint toABIBigInt(const facebook::jsi::BigInt &bi) const;
#endif

  // Convert a JSI Value to an ABI value (aliasing, no ownership transfer).
  jsi_value toABIValue(const facebook::jsi::Value &val) const;

  // Clone a JSI Value into an ABI value (increments refcount for pointers).
  jsi_value cloneToABIValue(const facebook::jsi::Value &val) const;

  // Clone an ABI value into a JSI Value (creates new ManagedPointerHolder).
  facebook::jsi::Value cloneToJSIValue(const jsi_value &val);

  // Clone an ABI PropNameID into a JSI PropNameID.
  facebook::jsi::PropNameID cloneToJSIPropNameID(jsi_propnameid name);

  // Take ownership of an ABI value into a JSI Value.
  facebook::jsi::Value intoJSIValue(jsi_value val);

  // Check an error code and throw JSError or JSINativeException.
  [[noreturn]] void throwError(jsi_error_code err);
  void checkStatus(jsi_error_code err);

  template <typename OrError>
  void checkResult(const OrError &result);

  // Catch C++ exceptions in callbacks and convert to ABI error codes.
  template <typename T, typename Fn>
  T abiRethrow(T (*wrapErr)(jsi_error_code), Fn fn);

  const jsi_vtable *abiVtable_;
  const jsi_runtime_vtable *vt_;
  jsi_runtime *abiRt_;
  bool ownsRuntime_ = true;
  bool activeJSError_ = false;
  std::unique_ptr<AbiInstrumentation> instrumentation_;
};

} // namespace jsi::abi

#endif /* JSI_ABI_RUNTIME_H */
