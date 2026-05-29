/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT license.
 *
 * Portions derived from facebook/hermes (Hermes ABI):
 *   Copyright (c) Meta Platforms, Inc. and affiliates.
 *   Licensed under the MIT license.
 *
 * JSI ABI — an ABI-stable C interface for JSI (JavaScript Interface).
 *
 * Derived from Meta's Hermes ABI: renamed to be engine-agnostic, with
 * additional changes and improvements.
 *
 * Pure C header. No engine-specific types. Can be used by any JS engine
 * (Hermes, V8, JSC).
 */

#ifndef JSI_ABI_H
#define JSI_ABI_H

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*==========================================================================
 * Platform Macros
 *==========================================================================*/

/* Calling convention — explicit __cdecl on Windows x86 for DLL safety. */
#ifdef _WIN32
#define JSI_CDECL __cdecl
#else
#define JSI_CDECL
#endif

/* DLL export/import macros.
 * If you need __declspec(dllimport), define JSI_API as __declspec(dllimport)
 * on the compiler's command line. */
#ifndef JSI_API
#ifdef _WIN32
#define JSI_API __declspec(dllexport)
#else
#define JSI_API __attribute__((visibility("default")))
#endif
#endif

/*==========================================================================
 * Forward Declarations
 *==========================================================================*/

struct jsi_runtime;
struct jsi_pointer;
struct jsi_growable_buffer;
struct jsi_buffer;
struct jsi_mutable_buffer;
struct jsi_prepared_javascript;
struct jsi_host_function;
struct jsi_host_object;
struct jsi_native_state;
struct jsi_propnameid_list;

/* Opaque, engine-agnostic configuration object passed to jsi_vtable.create_runtime.
 * Layout is owned by the engine implementation; consumers must use engine-
 * specific setter functions (e.g. v8_jsi_config_set_*). NULL means defaults. */
typedef struct jsi_config_s* jsi_config;

/* Standard data-deleter callback used when ownership of a buffer or other
 * pointer crosses the DLL boundary. The implementation that handed the
 * pointer over also supplies this callback so the consumer can free it
 * with the same allocator that allocated it. */
typedef void(JSI_CDECL *jsi_data_delete_cb)(void *data, void *deleter_data);

/*==========================================================================
 * Error Codes
 *==========================================================================*/

/* Error codes are odd values (bit 0 = 1) and 0 means "no error".
 * This lets bit-packed OrError variants encode the error directly
 * (no shift), and lets void-returning operations return jsi_error_code
 * directly with 0 = success / odd = error. Future codes must be odd. */
enum jsi_error_code {
  jsi_no_error = 0,
  jsi_error_native = 1, /* Native C++ exception (message available) */
  jsi_error_js = 3 /* JavaScript exception (JS value available) */
};

/*==========================================================================
 * Pointer Types (from Hermes ABI pattern)
 *
 * ManagedPointer — reference-counted GC root with vtable-based release.
 * Each pointer type wraps a jsi_pointer* to indicate the JS type.
 *
 * OrError variants come in two layouts, depending on whether the success
 * payload leaves a free bit for tagging:
 *
 * (A) Bit-packed (single uintptr_t). Used by every jsi_<ptr>_or_error
 *     (jsi_object_or_error, jsi_string_or_error, ...) and by
 *     jsi_bool_or_error. Discriminated by bit 0:
 *
 *       bit 0 = 0 : success.
 *                     pointer types — field is jsi_pointer* cast to
 *                                     uintptr_t (relies on jsi_pointer
 *                                     alignment >= 2 so bit 0 is 0).
 *                     jsi_bool_or_error — field is (val << 1):
 *                                         false = 0, true = 2.
 *                                         Decoder: field != 0.
 *       bit 0 = 1 : error. Field is the jsi_error_code value directly
 *                          (all error codes are odd; see enum above).
 *                          No shift is needed for either construction
 *                          or decoding.
 *
 * (B) Struct with explicit is_error flag + union. Used by
 *     jsi_size_or_error and jsi_uint8_ptr_or_error — their payloads use
 *     the full uintptr_t and leave no room for a tag bit.
 *
 * Void-returning operations skip the OrError wrapper entirely and return
 * jsi_error_code directly (0 = success, odd = error).
 *
 * Note: layout (A) stores a bare uintptr_t (not a union of jsi_pointer*
 * and uintptr_t). The tag bit shares the word with the pointer, so checking
 * it via a union would mean reading a non-active member — strict-reading
 * UB in C++. The pointer<->uintptr_t round-trip via reinterpret_cast is
 * explicitly well-defined ([expr.reinterpret.cast]/5).
 *
 * Construction and decoding helpers for both layouts live in
 * jsi_abi_helpers.h (create_*_or_error, is_error, get_error, get_*).
 *
 * NOTE: We intentionally spell out each type explicitly instead of using
 * X-macros (like Hermes ABI's HERMES_ABI_POINTER_TYPES macro). Explicit
 * declarations are debugger-friendly (F12/Go to Definition works), visible
 * to IntelliSense, and easier to read and maintain.
 *==========================================================================*/

struct jsi_pointer_vtable {
  void(JSI_CDECL *invalidate)(struct jsi_pointer *self);
};
struct jsi_pointer {
  const struct jsi_pointer_vtable *vtable;
};

/* --- Object --- */
struct jsi_object {
  struct jsi_pointer *pointer;
};
struct jsi_object_or_error {
  uintptr_t ptr_or_error;
};

/* --- Array --- */
struct jsi_array {
  struct jsi_pointer *pointer;
};
struct jsi_array_or_error {
  uintptr_t ptr_or_error;
};

/* --- String --- */
struct jsi_string {
  struct jsi_pointer *pointer;
};
struct jsi_string_or_error {
  uintptr_t ptr_or_error;
};

/* --- BigInt --- */
struct jsi_bigint {
  struct jsi_pointer *pointer;
};
struct jsi_bigint_or_error {
  uintptr_t ptr_or_error;
};

/* --- Symbol --- */
struct jsi_symbol {
  struct jsi_pointer *pointer;
};
struct jsi_symbol_or_error {
  uintptr_t ptr_or_error;
};

/* --- Function --- */
struct jsi_function {
  struct jsi_pointer *pointer;
};
struct jsi_function_or_error {
  uintptr_t ptr_or_error;
};

/* --- ArrayBuffer --- */
struct jsi_arraybuffer {
  struct jsi_pointer *pointer;
};
struct jsi_arraybuffer_or_error {
  uintptr_t ptr_or_error;
};

/* --- PropNameID --- */
struct jsi_propnameid {
  struct jsi_pointer *pointer;
};
struct jsi_propnameid_or_error {
  uintptr_t ptr_or_error;
};

/* --- WeakObject --- */
struct jsi_weak_object {
  struct jsi_pointer *pointer;
};
struct jsi_weak_object_or_error {
  uintptr_t ptr_or_error;
};

/* --- Bool-or-error (bit-packing scheme; see big-box comment above) ---
 * Void-returning operations return jsi_error_code directly (no wrapper);
 * 0 means success, odd values are errors. */
struct jsi_bool_or_error {
  uintptr_t bool_or_error;
};

/* --- Size and uint8_t* or-error (explicit is_error field) --- */
struct jsi_size_or_error {
  bool is_error;
  union {
    size_t val;
    enum jsi_error_code error;
  } data;
};
struct jsi_uint8_ptr_or_error {
  bool is_error;
  union {
    uint8_t *val;
    enum jsi_error_code error;
  } data;
};

/*==========================================================================
 * Value Representation (inline tagged union — from Hermes ABI)
 *==========================================================================*/

/* The POINTER_MASK marks value kinds that contain a jsi_pointer*. */
#define JSI_POINTER_MASK (1u << (sizeof(uint32_t) * 8u - 1u))

enum jsi_valuekind {
  jsi_valuekind_undefined = 0,
  jsi_valuekind_null = 1,
  jsi_valuekind_boolean = 2,
  jsi_valuekind_error = 3, /* Internal: used in jsi_value_or_error */
  jsi_valuekind_number = 4,
  jsi_valuekind_symbol = 5 | JSI_POINTER_MASK,
  jsi_valuekind_bigint = 6 | JSI_POINTER_MASK,
  jsi_valuekind_string = 7 | JSI_POINTER_MASK,
  jsi_valuekind_object = 9 | JSI_POINTER_MASK
};

struct jsi_value {
  enum jsi_valuekind kind;
  union {
    bool boolean;
    double number;
    struct jsi_pointer *pointer;
    enum jsi_error_code error; /* Only when kind == jsi_valuekind_error */
  } data;
};

struct jsi_value_or_error {
  struct jsi_value value;
};

/*==========================================================================
 * Buffer Types (from Hermes ABI)
 *
 * Three flavors with different ownership / lifetime semantics:
 *
 *   jsi_growable_buffer  — short-lived output parameter. Caller stack-
 *                          allocates and passes &buf for the DLL to fill
 *                          in during one call. The DLL only ever calls
 *                          try_grow_to to expand it; ownership stays with
 *                          the caller, so no release callback is needed.
 *
 *   jsi_buffer           — read-only input the DLL retains past the call
 *                          (e.g., script source). Consumer heap-allocates;
 *                          the DLL calls vtable->release when done.
 *
 *   jsi_mutable_buffer   — writable backing the DLL retains past the call
 *                          (e.g., ArrayBuffer storage). Same release
 *                          contract as jsi_buffer.
 *==========================================================================*/

struct jsi_growable_buffer_vtable {
  void(JSI_CDECL *try_grow_to)(struct jsi_growable_buffer *buf, size_t sz);
};
struct jsi_growable_buffer {
  const struct jsi_growable_buffer_vtable *vtable;
  uint8_t *data;
  size_t size;
  size_t used;
};

struct jsi_buffer_vtable {
  void(JSI_CDECL *release)(struct jsi_buffer *self);
};
struct jsi_buffer {
  const struct jsi_buffer_vtable *vtable;
  const uint8_t *data;
  size_t size;
};

struct jsi_mutable_buffer_vtable {
  void(JSI_CDECL *release)(struct jsi_mutable_buffer *self);
};
struct jsi_mutable_buffer {
  const struct jsi_mutable_buffer_vtable *vtable;
  uint8_t *data;
  size_t size;
};

/*==========================================================================
 * PreparedJavaScript
 *
 * Opaque engine-managed handle to a compiled script. Returned by
 * prepare_javascript; passed (by pointer) to evaluate_prepared_javascript.
 * Always carried by pointer so the implementation can append fields beyond
 * the vtable header without breaking ABI for callers that only see the
 * base layout.
 *
 * Lifecycle: the consumer owns the returned pointer and calls
 * vtable->release(p) when done.
 *==========================================================================*/

struct jsi_prepared_javascript_vtable {
  void(JSI_CDECL *release)(struct jsi_prepared_javascript *self);
};
struct jsi_prepared_javascript {
  const struct jsi_prepared_javascript_vtable *vtable;
  /* Implementation may append fields after this header. */
};
struct jsi_prepared_javascript_or_error {
  uintptr_t ptr_or_error;
};

/*==========================================================================
 * Callback Types (embedded-struct pattern — from Hermes ABI)
 *==========================================================================*/

/* Host function: called when JS invokes a native function. */
struct jsi_host_function_vtable {
  void(JSI_CDECL *release)(struct jsi_host_function *);
  struct jsi_value_or_error(JSI_CDECL *call)(
      struct jsi_host_function *self,
      struct jsi_runtime *rt,
      const struct jsi_value *this_arg,
      const struct jsi_value *args,
      size_t arg_count);
};
struct jsi_host_function {
  const struct jsi_host_function_vtable *vtable;
};

/* PropNameID list: returned by host object get_own_keys. */
struct jsi_propnameid_list_vtable {
  void(JSI_CDECL *release)(struct jsi_propnameid_list *);
};
struct jsi_propnameid_list {
  const struct jsi_propnameid_list_vtable *vtable;
  const struct jsi_propnameid *props;
  size_t size;
};

struct jsi_propnameid_list_or_error {
  uintptr_t ptr_or_error;
};

/* Host object: JS object backed by native callbacks. */
struct jsi_host_object_vtable {
  void(JSI_CDECL *release)(struct jsi_host_object *);
  struct jsi_value_or_error(JSI_CDECL *get)(
      struct jsi_host_object *self,
      struct jsi_runtime *rt,
      struct jsi_propnameid name);
  enum jsi_error_code(JSI_CDECL *set)(
      struct jsi_host_object *self,
      struct jsi_runtime *rt,
      struct jsi_propnameid name,
      const struct jsi_value *value);
  struct jsi_propnameid_list_or_error(JSI_CDECL *get_own_keys)(
      struct jsi_host_object *self,
      struct jsi_runtime *rt);
};
struct jsi_host_object {
  const struct jsi_host_object_vtable *vtable;
};

/* Native state: attached to JS objects, released on GC. */
struct jsi_native_state_vtable {
  void(JSI_CDECL *release)(struct jsi_native_state *self);
};
struct jsi_native_state {
  const struct jsi_native_state_vtable *vtable;
};

/*==========================================================================
 * Interface Query (for optional extensions — like JSI C++ ICast)
 *==========================================================================*/

struct jsi_interface_id {
  uint64_t lower;
  uint64_t upper;
};

/*==========================================================================
 * Runtime VTable
 *==========================================================================*/

struct jsi_runtime_vtable {
  /* Version of this vtable. Increment when appending new functions. */
  uint32_t version;
  uint32_t reserved;

  /*----------------------------------------------------------------------
   * QueryInterface / ICast (C++ JSI: castInterface)
   * Placed first (after version) — like COM's QueryInterface at slot 0.
   * Allows consumers to discover capabilities before calling anything else.
   *----------------------------------------------------------------------*/

  /* Query for an optional interface extension.
   * Returns jsi_error_native with empty message if not supported. */
  enum jsi_error_code(JSI_CDECL *query_interface)(
      struct jsi_runtime *rt,
      const struct jsi_interface_id *iid,
      const void **vtable_out,
      void **instance_out);

  /*----------------------------------------------------------------------
   * Lifecycle
   *
   * The runtime is reference-counted. v8_create_runtime / jsr_create_runtime
   * hand back a runtime with refcount 1. Each add_ref increments; each
   * release decrements and destroys the runtime when the count reaches 0.
   *----------------------------------------------------------------------*/

  void(JSI_CDECL *add_ref)(struct jsi_runtime *);
  void(JSI_CDECL *release)(struct jsi_runtime *);

  /*----------------------------------------------------------------------
   * Error retrieval (from Hermes ABI pattern)
   *----------------------------------------------------------------------*/

  /* Get and clear the JS exception value (after jsi_error_js). */
  struct jsi_value(JSI_CDECL *get_and_clear_js_error_value)(
      struct jsi_runtime *rt);

  /* Get and clear native exception message (after jsi_error_native). */
  void(JSI_CDECL *get_and_clear_native_exception_message)(
      struct jsi_runtime *rt,
      struct jsi_growable_buffer *buf);

  /* Set errors before returning from host function/object callbacks. */
  void(JSI_CDECL *set_js_error_value)(
      struct jsi_runtime *rt,
      const struct jsi_value *error_value);
  void(JSI_CDECL *set_native_exception_message)(
      struct jsi_runtime *rt,
      const uint8_t *utf8,
      size_t length);

  /*----------------------------------------------------------------------
   * Clone operations (from Hermes ABI)
   *----------------------------------------------------------------------*/

  struct jsi_propnameid(JSI_CDECL *clone_propnameid)(
      struct jsi_runtime *rt,
      struct jsi_propnameid name);
  struct jsi_string(JSI_CDECL *clone_string)(
      struct jsi_runtime *rt,
      struct jsi_string str);
  struct jsi_symbol(JSI_CDECL *clone_symbol)(
      struct jsi_runtime *rt,
      struct jsi_symbol sym);
  struct jsi_object(JSI_CDECL *clone_object)(
      struct jsi_runtime *rt,
      struct jsi_object obj);
  struct jsi_bigint(JSI_CDECL *clone_bigint)(
      struct jsi_runtime *rt,
      struct jsi_bigint bigint);

  /*----------------------------------------------------------------------
   * Runtime description and capabilities
   *----------------------------------------------------------------------*/

  void(JSI_CDECL *get_description)(
      struct jsi_runtime *rt,
      struct jsi_growable_buffer *buf);
  bool(JSI_CDECL *is_inspectable)(struct jsi_runtime *rt);

  /*----------------------------------------------------------------------
   * Handle scopes
   *----------------------------------------------------------------------*/

  /* Push a new handle scope. All pointer types created after this call
   * are valid until pop_scope. Engines that don't need scopes (Hermes)
   * can implement these as no-ops. */
  enum jsi_error_code(JSI_CDECL *push_scope)(
      struct jsi_runtime *rt,
      void **scope);
  enum jsi_error_code(JSI_CDECL *pop_scope)(
      struct jsi_runtime *rt,
      void *scope);

  /*----------------------------------------------------------------------
   * Script evaluation
   *----------------------------------------------------------------------*/

  struct jsi_value_or_error(JSI_CDECL *evaluate_javascript_source)(
      struct jsi_runtime *rt,
      struct jsi_buffer *buf,
      const char *source_url,
      size_t source_url_len);

  /* Prepare (compile) a script for later execution. The returned
   * jsi_prepared_javascript* is owned by the caller and freed via
   * vtable->release. */
  struct jsi_prepared_javascript_or_error(JSI_CDECL *prepare_javascript)(
      struct jsi_runtime *rt,
      struct jsi_buffer *buf,
      const char *source_url,
      size_t source_url_len);
  struct jsi_value_or_error(JSI_CDECL *evaluate_prepared_javascript)(
      struct jsi_runtime *rt,
      struct jsi_prepared_javascript *prepared);

  /*----------------------------------------------------------------------
   * Microtasks
   *----------------------------------------------------------------------*/

  struct jsi_bool_or_error(JSI_CDECL *drain_microtasks)(
      struct jsi_runtime *rt,
      int32_t max_hint);
  enum jsi_error_code(JSI_CDECL *queue_microtask)(
      struct jsi_runtime *rt,
      struct jsi_function callback);

  /*----------------------------------------------------------------------
   * Global object
   *----------------------------------------------------------------------*/

  struct jsi_object(JSI_CDECL *get_global_object)(struct jsi_runtime *rt);

  /*----------------------------------------------------------------------
   * String operations
   *----------------------------------------------------------------------*/

  struct jsi_string_or_error(JSI_CDECL *create_string_from_utf8)(
      struct jsi_runtime *rt,
      const uint8_t *utf8,
      size_t len);
  struct jsi_string_or_error(JSI_CDECL *create_string_from_utf16)(
      struct jsi_runtime *rt,
      const uint16_t *utf16,
      size_t len);
  void(JSI_CDECL *get_utf8_from_string)(
      struct jsi_runtime *rt,
      struct jsi_string str,
      struct jsi_growable_buffer *buf);
  void(JSI_CDECL *get_utf16_from_string)(
      struct jsi_runtime *rt,
      struct jsi_string str,
      struct jsi_growable_buffer *buf);

  /*----------------------------------------------------------------------
   * Symbol operations
   *----------------------------------------------------------------------*/

  void(JSI_CDECL *get_utf8_from_symbol)(
      struct jsi_runtime *rt,
      struct jsi_symbol sym,
      struct jsi_growable_buffer *buf);

  /*----------------------------------------------------------------------
   * PropNameID operations
   *----------------------------------------------------------------------*/

  struct jsi_propnameid_or_error(JSI_CDECL *create_propnameid_from_utf8)(
      struct jsi_runtime *rt,
      const uint8_t *utf8,
      size_t len);
  struct jsi_propnameid_or_error(JSI_CDECL *create_propnameid_from_string)(
      struct jsi_runtime *rt,
      struct jsi_string str);
  struct jsi_propnameid_or_error(JSI_CDECL *create_propnameid_from_symbol)(
      struct jsi_runtime *rt,
      struct jsi_symbol sym);
  bool(JSI_CDECL *prop_name_id_equals)(
      struct jsi_runtime *rt,
      struct jsi_propnameid a,
      struct jsi_propnameid b);
  void(JSI_CDECL *get_utf8_from_propnameid)(
      struct jsi_runtime *rt,
      struct jsi_propnameid name,
      struct jsi_growable_buffer *buf);

  /*----------------------------------------------------------------------
   * Object operations
   *----------------------------------------------------------------------*/

  struct jsi_object_or_error(JSI_CDECL *create_object)(
      struct jsi_runtime *rt);
  struct jsi_object_or_error(JSI_CDECL *create_object_with_prototype)(
      struct jsi_runtime *rt,
      const struct jsi_value *prototype);

  /* Prototype access */
  struct jsi_value_or_error(JSI_CDECL *get_prototype_of)(
      struct jsi_runtime *rt,
      struct jsi_object obj);
  enum jsi_error_code(JSI_CDECL *set_prototype_of)(
      struct jsi_runtime *rt,
      struct jsi_object obj,
      const struct jsi_value *prototype);

  /* Property operations (PropNameID-based) */
  struct jsi_bool_or_error(JSI_CDECL *has_object_property_from_propnameid)(
      struct jsi_runtime *rt,
      struct jsi_object obj,
      struct jsi_propnameid name);
  struct jsi_value_or_error(JSI_CDECL *get_object_property_from_propnameid)(
      struct jsi_runtime *rt,
      struct jsi_object obj,
      struct jsi_propnameid name);
  enum jsi_error_code(JSI_CDECL *set_object_property_from_propnameid)(
      struct jsi_runtime *rt,
      struct jsi_object obj,
      struct jsi_propnameid name,
      const struct jsi_value *value);
  enum jsi_error_code(JSI_CDECL *delete_property_from_propnameid)(
      struct jsi_runtime *rt,
      struct jsi_object obj,
      struct jsi_propnameid name);

  /* Property operations (Value-based — key can be string, symbol, number) */
  struct jsi_bool_or_error(JSI_CDECL *has_object_property_from_value)(
      struct jsi_runtime *rt,
      struct jsi_object obj,
      const struct jsi_value *key);
  struct jsi_value_or_error(JSI_CDECL *get_object_property_from_value)(
      struct jsi_runtime *rt,
      struct jsi_object obj,
      const struct jsi_value *key);
  enum jsi_error_code(JSI_CDECL *set_object_property_from_value)(
      struct jsi_runtime *rt,
      struct jsi_object obj,
      const struct jsi_value *key,
      const struct jsi_value *value);
  enum jsi_error_code(JSI_CDECL *delete_property_from_value)(
      struct jsi_runtime *rt,
      struct jsi_object obj,
      const struct jsi_value *key);

  /* Get enumerable string property names. */
  struct jsi_array_or_error(JSI_CDECL *get_object_property_names)(
      struct jsi_runtime *rt,
      struct jsi_object obj);

  /* External memory pressure hint for GC. */
  enum jsi_error_code(JSI_CDECL *set_object_external_memory_pressure)(
      struct jsi_runtime *rt,
      struct jsi_object obj,
      size_t amount);

  /* Error object creation */
  struct jsi_object_or_error(JSI_CDECL *create_error)(
      struct jsi_runtime *rt,
      const uint8_t *utf8_msg,
      size_t len);

  /*----------------------------------------------------------------------
   * Array operations
   *----------------------------------------------------------------------*/

  struct jsi_array_or_error(JSI_CDECL *create_array)(
      struct jsi_runtime *rt,
      size_t length);
  size_t(JSI_CDECL *get_array_length)(
      struct jsi_runtime *rt,
      struct jsi_array arr);
  struct jsi_value_or_error(JSI_CDECL *get_array_element)(
      struct jsi_runtime *rt,
      struct jsi_object arr,
      size_t index);
  enum jsi_error_code(JSI_CDECL *set_array_element)(
      struct jsi_runtime *rt,
      struct jsi_object arr,
      size_t index,
      const struct jsi_value *value);

  /*----------------------------------------------------------------------
   * ArrayBuffer operations
   *----------------------------------------------------------------------*/

  struct jsi_arraybuffer_or_error(JSI_CDECL *create_arraybuffer)(
      struct jsi_runtime *rt,
      size_t byte_length);
  struct jsi_arraybuffer_or_error(
      JSI_CDECL *create_arraybuffer_from_external_data)(
      struct jsi_runtime *rt,
      struct jsi_mutable_buffer *buf);
  struct jsi_uint8_ptr_or_error(JSI_CDECL *get_arraybuffer_data)(
      struct jsi_runtime *rt,
      struct jsi_arraybuffer ab);
  struct jsi_size_or_error(JSI_CDECL *get_arraybuffer_size)(
      struct jsi_runtime *rt,
      struct jsi_arraybuffer ab);

  /*----------------------------------------------------------------------
   * Function operations
   *----------------------------------------------------------------------*/

  struct jsi_value_or_error(JSI_CDECL *call)(
      struct jsi_runtime *rt,
      struct jsi_function fn,
      const struct jsi_value *js_this,
      const struct jsi_value *args,
      size_t arg_count);
  struct jsi_value_or_error(JSI_CDECL *call_as_constructor)(
      struct jsi_runtime *rt,
      struct jsi_function fn,
      const struct jsi_value *args,
      size_t arg_count);
  struct jsi_function_or_error(JSI_CDECL *create_function_from_host_function)(
      struct jsi_runtime *rt,
      struct jsi_propnameid name,
      uint32_t param_count,
      struct jsi_host_function *hf);
  struct jsi_host_function *(JSI_CDECL *get_host_function)(
      struct jsi_runtime *rt,
      struct jsi_function fn);

  /*----------------------------------------------------------------------
   * Host object operations
   *----------------------------------------------------------------------*/

  struct jsi_object_or_error(JSI_CDECL *create_object_from_host_object)(
      struct jsi_runtime *rt,
      struct jsi_host_object *ho);
  struct jsi_host_object *(JSI_CDECL *get_host_object)(
      struct jsi_runtime *rt,
      struct jsi_object obj);

  /*----------------------------------------------------------------------
   * Native state
   *----------------------------------------------------------------------*/

  bool(JSI_CDECL *has_native_state)(
      struct jsi_runtime *rt,
      struct jsi_object obj);
  struct jsi_native_state *(JSI_CDECL *get_native_state)(
      struct jsi_runtime *rt,
      struct jsi_object obj);
  enum jsi_error_code(JSI_CDECL *set_native_state)(
      struct jsi_runtime *rt,
      struct jsi_object obj,
      struct jsi_native_state *ns);

  /*----------------------------------------------------------------------
   * Type checks
   *----------------------------------------------------------------------*/

  bool(JSI_CDECL *object_is_array)(
      struct jsi_runtime *rt,
      struct jsi_object obj);
  bool(JSI_CDECL *object_is_arraybuffer)(
      struct jsi_runtime *rt,
      struct jsi_object obj);
  bool(JSI_CDECL *object_is_function)(
      struct jsi_runtime *rt,
      struct jsi_object obj);

  /*----------------------------------------------------------------------
   * Weak references
   *----------------------------------------------------------------------*/

  struct jsi_weak_object_or_error(JSI_CDECL *create_weak_object)(
      struct jsi_runtime *rt,
      struct jsi_object obj);
  struct jsi_value(JSI_CDECL *lock_weak_object)(
      struct jsi_runtime *rt,
      struct jsi_weak_object wo);

  /*----------------------------------------------------------------------
   * Comparison
   *----------------------------------------------------------------------*/

  struct jsi_bool_or_error(JSI_CDECL *instance_of)(
      struct jsi_runtime *rt,
      struct jsi_object obj,
      struct jsi_function ctor);
  bool(JSI_CDECL *strict_equals_symbol)(
      struct jsi_runtime *rt,
      struct jsi_symbol a,
      struct jsi_symbol b);
  bool(JSI_CDECL *strict_equals_bigint)(
      struct jsi_runtime *rt,
      struct jsi_bigint a,
      struct jsi_bigint b);
  bool(JSI_CDECL *strict_equals_string)(
      struct jsi_runtime *rt,
      struct jsi_string a,
      struct jsi_string b);
  bool(JSI_CDECL *strict_equals_object)(
      struct jsi_runtime *rt,
      struct jsi_object a,
      struct jsi_object b);

  /*----------------------------------------------------------------------
   * BigInt
   *----------------------------------------------------------------------*/

  struct jsi_bigint_or_error(JSI_CDECL *create_bigint_from_int64)(
      struct jsi_runtime *rt,
      int64_t value);
  struct jsi_bigint_or_error(JSI_CDECL *create_bigint_from_uint64)(
      struct jsi_runtime *rt,
      uint64_t value);
  bool(JSI_CDECL *bigint_is_int64)(
      struct jsi_runtime *rt,
      struct jsi_bigint bigint);
  bool(JSI_CDECL *bigint_is_uint64)(
      struct jsi_runtime *rt,
      struct jsi_bigint bigint);
  uint64_t(JSI_CDECL *bigint_truncate_to_uint64)(
      struct jsi_runtime *rt,
      struct jsi_bigint bigint);
  int64_t(JSI_CDECL *bigint_to_int64)(
      struct jsi_runtime *rt,
      struct jsi_bigint bigint,
      bool *lossless);
  uint64_t(JSI_CDECL *bigint_to_uint64)(
      struct jsi_runtime *rt,
      struct jsi_bigint bigint,
      bool *lossless);
  struct jsi_string_or_error(JSI_CDECL *bigint_to_string)(
      struct jsi_runtime *rt,
      struct jsi_bigint bigint,
      uint32_t radix);
};

/*==========================================================================
 * Runtime Instance
 *==========================================================================*/

struct jsi_runtime {
  const struct jsi_runtime_vtable *vt;
};

/*==========================================================================
 * Top-Level VTable (factory)
 *==========================================================================*/

struct jsi_vtable {
  uint32_t version;
  uint32_t reserved;

  /* Create a new runtime instance. */
  struct jsi_runtime *(JSI_CDECL *create_runtime)(
      const void *config); /* Engine-specific config, NULL for defaults */

  /* Check if a buffer contains engine-specific bytecode.
   * Engines that don't support bytecode return false. */
  bool(JSI_CDECL *is_bytecode)(const uint8_t *buf, size_t len);
};

/*==========================================================================
 * Instrumentation Interface (via query_interface)
 *==========================================================================*/

/* Well-known interface ID for instrumentation. */
#define JSI_IID_INSTRUMENTATION \
  { 0x8A3F9E2B7C1D4E5Full, 0xB6A2C8D9E0F13457ull }

struct jsi_instrumentation_vtable {
  uint32_t version;
  uint32_t reserved;

  void(JSI_CDECL *release)(void *inst);

  /* Get GC stats as JSON string (JSI: getRecordedGCStats). */
  void(JSI_CDECL *get_recorded_gc_stats)(
      void *inst,
      struct jsi_growable_buffer *buf);

  /* Get heap info as key-value pairs (JSI: getHeapInfo). */
  void(JSI_CDECL *get_heap_info)(
      void *inst,
      bool include_expensive,
      void(JSI_CDECL *callback)(void *user_data, const char *key, int64_t value),
      void *user_data);

  /* Request garbage collection (JSI: collectGarbage). */
  void(JSI_CDECL *collect_garbage)(void *inst, const char *cause);

  /* Start tracking heap object stack traces
   * (JSI: startTrackingHeapObjectStackTraces). */
  void(JSI_CDECL *start_tracking_heap_object_stack_traces)(void *inst);

  /* Stop tracking heap object stack traces
   * (JSI: stopTrackingHeapObjectStackTraces). */
  void(JSI_CDECL *stop_tracking_heap_object_stack_traces)(void *inst);

  /* Start heap sampling profiler (JSI: startHeapSampling). */
  void(JSI_CDECL *start_heap_sampling)(void *inst, size_t sampling_interval);

  /* Stop heap sampling and write output (JSI: stopHeapSampling). */
  void(JSI_CDECL *stop_heap_sampling)(
      void *inst,
      void(JSI_CDECL *callback)(void *user_data, const char *data, size_t size),
      void *user_data);

  /* Create heap snapshot to file (JSI: createSnapshotToFile). */
  void(JSI_CDECL *create_snapshot_to_file)(
      void *inst,
      const char *path,
      bool capture_numeric_value);

  /* Create heap snapshot to stream (JSI: createSnapshotToStream). */
  void(JSI_CDECL *create_snapshot_to_stream)(
      void *inst,
      bool capture_numeric_value,
      void(JSI_CDECL *callback)(void *user_data, const char *data, size_t size),
      void *user_data);

  /* Flush and disable bridge traffic trace
   * (JSI: flushAndDisableBridgeTrafficTrace). */
  void(JSI_CDECL *flush_and_disable_bridge_traffic_trace)(
      void *inst,
      struct jsi_growable_buffer *buf);

  /* Write basic block profile trace to file
   * (JSI: writeBasicBlockProfileTraceToFile). */
  void(JSI_CDECL *write_basic_block_profile_trace_to_file)(
      void *inst,
      const char *path);

  /* Dump opcode stats (JSI v21: dumpOpcodeStats). */
  void(JSI_CDECL *dump_opcode_stats)(
      void *inst,
      void(JSI_CDECL *callback)(void *user_data, const char *data, size_t size),
      void *user_data);

  /* Dump profiler symbols to file (JSI: dumpProfilerSymbolsToFile). */
  void(JSI_CDECL *dump_profiler_symbols_to_file)(
      void *inst,
      const char *path);
};

#ifdef __cplusplus
}
#endif

#endif /* JSI_ABI_H */
