/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT license.
 *
 * Portions derived from facebook/hermes (Hermes ABI):
 *   Copyright (c) Meta Platforms, Inc. and affiliates.
 *   Licensed under the MIT license.
 *
 * JsiAbiRuntime — a facebook::jsi::Runtime implementation backed by the
 * JSI ABI C interface. Engine-agnostic; compiled on the consumer side.
 *
 * The implementation class lives entirely in JsiAbiRuntime.cpp. Consumers
 * see only the factory + accessor functions declared here.
 */

#ifndef JSI_ABI_RUNTIME_H
#define JSI_ABI_RUNTIME_H

#include <jsi/jsi.h>

#include <memory>

struct jsi_vtable;
struct jsi_runtime;

namespace jsi::abi {

/// Create a new JSI runtime via the supplied factory vtable.
/// \param vtable The top-level JSI ABI vtable (e.g., from
///   get_jsi_abi_v8_vtable()).
/// \param config Engine-specific config, nullptr for defaults.
/// \return A facebook::jsi::Runtime that owns one ref on the new runtime.
std::unique_ptr<facebook::jsi::Runtime> makeJsiAbiRuntime(
    const jsi_vtable *vtable,
    const void *config = nullptr);

/// Wrap an existing jsi_runtime. The returned wrapper holds its own ref
/// (add_ref is called on entry), so the caller's ref is independent.
///
/// Used by the W8 dual-API scenario where a jsr_runtime exposes the
/// underlying jsi_runtime via jsr_runtime_get_jsi_runtime and the caller
/// wants to drive the same engine through facebook::jsi::Runtime.
std::unique_ptr<facebook::jsi::Runtime> wrapJsiRuntime(jsi_runtime *abiRuntime);

/// Retrieve the underlying jsi_runtime pointer from a facebook::jsi::Runtime
/// that was created by one of the factories above. The returned pointer is
/// borrowed (no add_ref); it stays alive as long as the wrapping Runtime does.
///
/// Intended for test hooks and advanced consumers that need to call
/// ABI-specific extensions (e.g., v8_open_inspector). Returns nullptr if
/// the supplied Runtime was not created by this factory.
jsi_runtime *getAbiRuntime(facebook::jsi::Runtime &runtime) noexcept;

} // namespace jsi::abi

#endif /* JSI_ABI_RUNTIME_H */
