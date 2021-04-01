#pragma once

//
// This file contains simplified versions of classes required from node for the
// V8 NAPI implementation.
//

#include <v8.h>
#include <cstdint>
#include "js_native_api_v8.h"
#include "node_context_data.h"
#include "util.h"

#define PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(V) \
  V(napi_type_tag, "node:napi:type_tag")         \
  V(napi_wrapper, "node:napi:wrapper")           \
  V(untransferable_object_private_symbol, "node:untransferableObject")

#define ENVIRONMENT_STRONG_PERSISTENT_VALUES(V) \
  V(buffer_prototype_object, v8::Object)

namespace node {

enum class CallbackFlags {
  kUnrefed = 0,
  kRefed = 1,
};

inline CallbackFlags operator&(CallbackFlags f1, CallbackFlags f2) {
  return static_cast<CallbackFlags>(
      static_cast<uint32_t>(f1) & static_cast<uint32_t>(f2));
}

inline bool operator!(CallbackFlags flags) {
  return static_cast<uint32_t>(flags) == 0;
}

// A queue of C++ functions that take Args... as arguments and return R
// (this is similar to the signature of std::function).
// New entries are added using `CreateCallback()`/`Push()`, and removed using
// `Shift()`.
// The `refed` flag is left for easier use in situations in which some of these
// should be run even if nothing else is keeping the event loop alive.
template <typename R, typename... Args>
class CallbackQueue {
 public:
  class Callback {
   public:
    explicit inline Callback(CallbackFlags flags);

    virtual ~Callback() = default;
    virtual R Call(Args... args) = 0;

    inline CallbackFlags flags() const;

   private:
    inline std::unique_ptr<Callback> get_next();
    inline void set_next(std::unique_ptr<Callback> next);

    CallbackFlags flags_;
    std::unique_ptr<Callback> next_;

    friend class CallbackQueue;
  };

  template <typename Fn>
  inline std::unique_ptr<Callback> CreateCallback(Fn &&fn, CallbackFlags);

  inline std::unique_ptr<Callback> Shift();
  inline void Push(std::unique_ptr<Callback> cb);
  // ConcatMove adds elements from 'other' to the end of this list, and clears
  // 'other' afterwards.
  inline void ConcatMove(CallbackQueue &&other);

  // size() is atomic and may be called from any thread.
  inline size_t size() const;

 private:
  template <typename Fn>
  class CallbackImpl final : public Callback {
   public:
    CallbackImpl(Fn &&callback, CallbackFlags flags);
    R Call(Args... args) override;

   private:
    Fn callback_;
  };

  std::atomic<size_t> size_{0};
  std::unique_ptr<Callback> head_;
  Callback *tail_ = nullptr;
};

class CleanupHookCallback {
 public:
  typedef void (*Callback)(void *);

  CleanupHookCallback(Callback fn, void *arg, uint64_t insertion_order_counter)
      : fn_(fn), arg_(arg), insertion_order_counter_(insertion_order_counter) {}

  // Only hashes `arg_`, since that is usually enough to identify the hook.
  struct Hash {
    inline size_t operator()(const CleanupHookCallback &cb) const;
  };

  // Compares by `fn_` and `arg_` being equal.
  struct Equal {
    inline bool operator()(
        const CleanupHookCallback &a,
        const CleanupHookCallback &b) const;
  };

  // inline BaseObject* GetBaseObject() const;

 private:
  friend class Environment;
  Callback fn_;
  void *arg_;

  // We keep track of the insertion order for these objects, so that we can
  // call the callbacks in reverse order when we are cleaning up.
  uint64_t insertion_order_counter_;
};

class ImmediateInfo {
 public:
  // inline AliasedUint32Array& fields();
  inline uint32_t count() const;
  inline uint32_t ref_count() const;
  inline bool has_outstanding() const;
  inline void ref_count_inc(uint32_t increment);
  inline void ref_count_dec(uint32_t decrement);

  ImmediateInfo(const ImmediateInfo &) = delete;
  ImmediateInfo &operator=(const ImmediateInfo &) = delete;
  ImmediateInfo(ImmediateInfo &&) = delete;
  ImmediateInfo &operator=(ImmediateInfo &&) = delete;
  ~ImmediateInfo() = default;

  //   SET_MEMORY_INFO_NAME(ImmediateInfo)
  //   SET_SELF_SIZE(ImmediateInfo)
  //   void MemoryInfo(MemoryTracker* tracker) const override;

  //   struct SerializeInfo {
  //     AliasedBufferIndex fields;
  //   };
  //   SerializeInfo Serialize(v8::Local<v8::Context> context,
  //                           v8::SnapshotCreator* creator);
  //   void Deserialize(v8::Local<v8::Context> context);

 private:
  friend class Environment; // So we can call the constructor.
  explicit ImmediateInfo(v8::Isolate *isolate); //, const SerializeInfo* info);

  enum Fields { kCount, kRefCount, kHasOutstanding, kFieldsCount };

  // AliasedUint32Array fields_;
};

class IsolateData {
 public:
  IsolateData(v8::Isolate *isolate);

#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define V(TypeName, PropertyName) v8::Local<TypeName> PropertyName() const;
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
#undef V
#undef VP

  v8::Isolate *isolate() const;

  IsolateData(const IsolateData &) = delete;
  IsolateData &operator=(const IsolateData &) = delete;
  IsolateData(IsolateData &&) = delete;
  IsolateData &operator=(IsolateData &&) = delete;

 private:
  void CreateProperties();

 private:
  v8::Isolate *const isolate_;

#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define V(TypeName, PropertyName) v8::Eternal<TypeName> PropertyName##_;
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
#undef V
#undef VP
};

class Environment {
 public:
  Environment(IsolateData *isolate_data, v8::Local<v8::Context> context);
  ~Environment();

  IsolateData *isolate_data() const;

  Environment(const Environment &) = delete;
  Environment &operator=(const Environment &) = delete;
  Environment(Environment &&) = delete;
  Environment &operator=(Environment &&) = delete;

  static Environment *GetCurrent(v8::Local<v8::Context> context);

  template <typename Fn>
  inline void SetImmediate(
      Fn &&cb,
      CallbackFlags flags = CallbackFlags::kRefed);
  // This behaves like SetImmediate() but can be called from any thread.
  template <typename Fn>
  inline void SetImmediateThreadsafe(
      Fn &&cb,
      CallbackFlags flags = CallbackFlags::kRefed);

  // Strings and private symbols are shared across shared contexts
  // The getters simply proxy to the per-isolate primitive.
#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define V(TypeName, PropertyName) v8::Local<TypeName> PropertyName() const;
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
#undef V
#undef VP

#define V(PropertyName, TypeName)                  \
  inline v8::Local<TypeName> PropertyName() const; \
  inline void set_##PropertyName(v8::Local<TypeName> value);
  ENVIRONMENT_STRONG_PERSISTENT_VALUES(V)
#undef V

  v8::Local<v8::Context> context() const;
  v8::Isolate *isolate() const;

  using CleanupCallback = CleanupHookCallback::Callback;
  inline void AddCleanupHook(CleanupCallback cb, void *arg);
  inline void RemoveCleanupHook(CleanupCallback cb, void *arg);
  void RunCleanup();

  inline ImmediateInfo *immediate_info();

  void ToggleImmediateRef(bool ref);

 private:
  ImmediateInfo immediate_info_;
  static void *const kNodeContextTagPtr;
  static int const kNodeContextTag;
  v8::Global<v8::Context> context_;
  IsolateData *const isolate_data_;

  typedef CallbackQueue<void, Environment *> NativeImmediateQueue;
  NativeImmediateQueue native_immediates_;

#define V(PropertyName, TypeName) v8::Global<TypeName> PropertyName##_;
  ENVIRONMENT_STRONG_PERSISTENT_VALUES(V)
#undef V
};


template <typename R, typename... Args>
CallbackQueue<R, Args...>::Callback::Callback(CallbackFlags flags)
  : flags_(flags) {}

template <typename R, typename... Args>
template <typename Fn>
std::unique_ptr<typename CallbackQueue<R, Args...>::Callback>
CallbackQueue<R, Args...>::CreateCallback(Fn&& fn, CallbackFlags flags) {
  return std::make_unique<CallbackImpl<Fn>>(std::move(fn), flags);
}

template <typename R, typename... Args>
std::unique_ptr<typename CallbackQueue<R, Args...>::Callback>
CallbackQueue<R, Args...>::Shift() {
//   std::unique_ptr<Callback> ret = std::move(head_);
//   if (ret) {
//     head_ = ret->get_next();
//     if (!head_)
//       tail_ = nullptr;  // The queue is now empty.
//     size_--;
//   }
//   return ret;
return {};
}

template <typename R, typename... Args>
void CallbackQueue<R, Args...>::Push(std::unique_ptr<Callback> cb) {
    (void)cb;
//   Callback* prev_tail = tail_;

//   size_++;
//   tail_ = cb.get();
//   if (prev_tail != nullptr)
//     prev_tail->set_next(std::move(cb));
//   else
//     head_ = std::move(cb);
}

template <typename R, typename... Args>
template <typename Fn>
CallbackQueue<R, Args...>::CallbackImpl<Fn>::CallbackImpl(
    Fn&& callback, CallbackFlags flags)
  : Callback(flags),
    callback_(std::move(callback)) {}

template <typename R, typename... Args>
template <typename Fn>
R CallbackQueue<R, Args...>::CallbackImpl<Fn>::Call(Args... args) {
  return callback_(std::forward<Args>(args)...);
}

inline ImmediateInfo::ImmediateInfo(v8::Isolate *isolate)
//: fields_(isolate, kFieldsCount, MAYBE_FIELD_PTR(info, fields)) {}
{
  (void)isolate;
}

//=============================================================================
// IsolateData inline implementation.
//=============================================================================

inline IsolateData::IsolateData(v8::Isolate *isolate) : isolate_{isolate} {
  CreateProperties();
}

#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define V(TypeName, PropertyName)                                \
  inline v8::Local<TypeName> IsolateData::PropertyName() const { \
    return PropertyName##_.Get(isolate_);                        \
  }
PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
#undef V
#undef VP

inline v8::Isolate *IsolateData::isolate() const {
  return isolate_;
}

//=============================================================================
// Environment inline implementation.
//=============================================================================

inline Environment::Environment(
    IsolateData *isolate_data,
    v8::Local<v8::Context> context)
    : isolate_data_{isolate_data}, immediate_info_(context->GetIsolate()) {
  context_.Reset(context->GetIsolate(), context);
  v8::HandleScope handle_scope(isolate());
  context->SetAlignedPointerInEmbedderData(
      ContextEmbedderIndex::kEnvironment, this);
  // Used by Environment::GetCurrent to know that we are on a node context.
  context->SetAlignedPointerInEmbedderData(
      ContextEmbedderIndex::kContextTag, Environment::kNodeContextTagPtr);
}

#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define V(TypeName, PropertyName)                                \
  inline v8::Local<TypeName> Environment::PropertyName() const { \
    return isolate_data()->PropertyName();                       \
  }
PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
#undef V
#undef VP

#define V(PropertyName, TypeName)                                          \
  inline v8::Local<TypeName> Environment::PropertyName() const {           \
    return PersistentToLocal::Strong(PropertyName##_);                     \
  }                                                                        \
  inline void Environment::set_##PropertyName(v8::Local<TypeName> value) { \
    PropertyName##_.Reset(isolate(), value);                               \
  }
ENVIRONMENT_STRONG_PERSISTENT_VALUES(V)
#undef V

/*static*/ inline Environment *Environment::GetCurrent(
    v8::Local<v8::Context> context) {
  if (UNLIKELY(context.IsEmpty())) {
    return nullptr;
  }
  if (UNLIKELY(
          context->GetNumberOfEmbedderDataFields() <=
          ContextEmbedderIndex::kContextTag)) {
    return nullptr;
  }
  if (UNLIKELY(
          context->GetAlignedPointerFromEmbedderData(
              ContextEmbedderIndex::kContextTag) !=
          Environment::kNodeContextTagPtr)) {
    return nullptr;
  }
  return static_cast<Environment *>(context->GetAlignedPointerFromEmbedderData(
      ContextEmbedderIndex::kEnvironment));
}

template <typename Fn>
void Environment::SetImmediate(Fn &&cb, CallbackFlags flags) {
  auto callback = native_immediates_.CreateCallback(std::move(cb), flags);
  native_immediates_.Push(std::move(callback));

  if (!!(flags & CallbackFlags::kRefed)) {
    if (immediate_info()->ref_count() == 0)
      ToggleImmediateRef(true);
    immediate_info()->ref_count_inc(1);
  }
}

template <typename Fn>
void Environment::SetImmediateThreadsafe(Fn&& cb, CallbackFlags flags) {
    (void)cb;
    (void)flags;
//   auto callback = native_immediates_threadsafe_.CreateCallback(
//       std::move(cb), flags);
//   {
//     Mutex::ScopedLock lock(native_immediates_threadsafe_mutex_);
//     native_immediates_threadsafe_.Push(std::move(callback));
//     if (task_queues_async_initialized_)
//       uv_async_send(&task_queues_async_);
//   }
}

inline void Environment::ToggleImmediateRef(bool ref) {
    (void)ref;
//   if (started_cleanup_) return;

//   if (ref) {
//     // Idle handle is needed only to stop the event loop from blocking in poll.
//     uv_idle_start(immediate_idle_handle(), [](uv_idle_t*){ });
//   } else {
//     uv_idle_stop(immediate_idle_handle());
//   }
}

inline v8::Local<v8::Context> Environment::context() const {
  return PersistentToLocal::Strong(context_);
}

inline v8::Isolate *Environment::isolate() const {
  return isolate_data_->isolate();
}

inline IsolateData *Environment::isolate_data() const {
  return isolate_data_;
}

inline ImmediateInfo *Environment::immediate_info() {
  return &immediate_info_;
}

inline uint32_t ImmediateInfo::ref_count() const {
  return 1; // fields_[kRefCount];
}

inline void ImmediateInfo::ref_count_inc(uint32_t increment) {
  (void)increment;
  // fields_[kRefCount] += increment;
}

inline void ImmediateInfo::ref_count_dec(uint32_t decrement) {
  (void)decrement;
  // fields_[kRefCount] -= decrement;
}

void Environment::AddCleanupHook(CleanupCallback fn, void *arg) {
  (void)fn;
  (void)arg;
  //   auto insertion_info = cleanup_hooks_.emplace(CleanupHookCallback {
  //     fn, arg, cleanup_hook_counter_++
  //   });
  //   // Make sure there was no existing element with these values.
  //   CHECK_EQ(insertion_info.second, true);
}

void Environment::RemoveCleanupHook(CleanupCallback fn, void *arg) {
  (void)fn;
  (void)arg;
  //   CleanupHookCallback search { fn, arg, 0 };
  //   cleanup_hooks_.erase(search);
}

namespace Buffer {

static const size_t kMaxLength = v8::TypedArray::kMaxLength;

typedef void (*FreeCallback)(char *data, void *hint);

// NODE_EXTERN bool HasInstance(v8::Local<v8::Value> val);
// NODE_EXTERN bool HasInstance(v8::Local<v8::Object> val);
// NODE_EXTERN char *Data(v8::Local<v8::Value> val);
// NODE_EXTERN char *Data(v8::Local<v8::Object> val);
// NODE_EXTERN size_t Length(v8::Local<v8::Value> val);
// NODE_EXTERN size_t Length(v8::Local<v8::Object> val);

// public constructor - data is copied
// NODE_EXTERN v8::MaybeLocal<v8::Object>
// Copy(v8::Isolate *isolate, const char *data, size_t len);

// public constructor
// NODE_EXTERN v8::MaybeLocal<v8::Object> New(v8::Isolate *isolate, size_t
// length);

// public constructor from string
// NODE_EXTERN v8::MaybeLocal<v8::Object> New(
//     v8::Isolate *isolate,
//     v8::Local<v8::String> string,
//     enum encoding enc = UTF8);

// public constructor - data is used, callback is passed data on object gc
v8::MaybeLocal<v8::Object> New(
    v8::Isolate *isolate,
    char *data,
    size_t length,
    FreeCallback callback,
    void *hint);

v8::MaybeLocal<v8::Object> New(
    Environment *env,
    char *data,
    size_t length,
    FreeCallback callback,
    void *hint);

// public constructor - data is used.
// NODE_EXTERN v8::MaybeLocal<v8::Object>
// New(v8::Isolate *isolate, char *data, size_t len);

// // Creates a Buffer instance over an existing ArrayBuffer.
v8::MaybeLocal<v8::Uint8Array> New(
    v8::Isolate *isolate,
    v8::Local<v8::ArrayBuffer> ab,
    size_t byte_offset,
    size_t length);

// This is verbose to be explicit with inline commenting
// static inline bool IsWithinBounds(size_t off, size_t len, size_t max) {
//   // Asking to seek too far into the buffer
//   // check to avoid wrapping in subsequent subtraction
//   if (off > max)
//     return false;

//   // Asking for more than is left over in the buffer
//   if (max - off < len)
//     return false;

//   // Otherwise we're in bounds
//   return true;
// }

} // namespace Buffer

} // namespace node

napi_status napi_create_external_buffer(
    napi_env env,
    size_t length,
    void *data,
    napi_finalize finalize_cb,
    void *finalize_hint,
    napi_value *result);