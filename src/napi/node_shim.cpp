#include "node_shim.h"
#include <mutex>
#include "js_native_api_v8.h"

namespace node {

void IsolateData::CreateProperties() {
  // Create string and private symbol properties as internalized one byte
  // strings after the platform is properly initialized.
  //
  // Internalized because it makes property lookups a little faster and
  // because the string is created in the old space straight away.  It's going
  // to end up in the old space sooner or later anyway but now it doesn't go
  // through v8::Eternal's new space handling first.
  //
  // One byte because our strings are ASCII and we can safely skip V8's UTF-8
  // decoding step.

  v8::HandleScope handle_scope(isolate_);

#define V(PropertyName, StringValue)                          \
  PropertyName##_.Set(                                        \
      isolate_,                                               \
      v8::Private::New(                                       \
          isolate_,                                           \
          v8::String::NewFromOneByte(                         \
              isolate_,                                       \
              reinterpret_cast<const uint8_t *>(StringValue), \
              v8::NewStringType::kInternalized,               \
              sizeof(StringValue) - 1)                        \
              .ToLocalChecked()));
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(V)
#undef V
}

/*static*/ int const Environment::kNodeContextTag = 0x6e6f64;
/*static*/ void *const Environment::kNodeContextTagPtr = const_cast<void *>(
    static_cast<const void *>(&Environment::kNodeContextTag));

Environment::~Environment() {
  v8::HandleScope handle_scope(isolate());

  context()->SetAlignedPointerInEmbedderData(
      ContextEmbedderIndex::kEnvironment, nullptr);
}

namespace Buffer {

namespace {

class CallbackInfo {
 public:
  static inline v8::Local<v8::ArrayBuffer> CreateTrackedArrayBuffer(
      Environment *env,
      char *data,
      size_t length,
      FreeCallback callback,
      void *hint);

  CallbackInfo(const CallbackInfo &) = delete;
  CallbackInfo &operator=(const CallbackInfo &) = delete;

 private:
  static void CleanupHook(void *data);
  inline void OnBackingStoreFree();
  inline void CallAndResetCallback();
  inline CallbackInfo(
      Environment *env,
      FreeCallback callback,
      char *data,
      void *hint);
  v8::Global<v8::ArrayBuffer> persistent_;
  std::mutex mutex_; // Protects callback_.
  FreeCallback callback_;
  char *const data_;
  void *const hint_;
  Environment *const env_;
};

v8::Local<v8::ArrayBuffer> CallbackInfo::CreateTrackedArrayBuffer(
    Environment *env,
    char *data,
    size_t length,
    FreeCallback callback,
    void *hint) {
  CHECK_NOT_NULL(callback);
  CHECK_IMPLIES(data == nullptr, length == 0);

  CallbackInfo *self = new CallbackInfo(env, callback, data, hint);
  std::unique_ptr<v8::BackingStore> bs = v8::ArrayBuffer::NewBackingStore(
      data,
      length,
      [](void *, size_t, void *arg) {
        static_cast<CallbackInfo *>(arg)->OnBackingStoreFree();
      },
      self);
  v8::Local<v8::ArrayBuffer> ab =
      v8::ArrayBuffer::New(env->isolate(), std::move(bs));

  // V8 simply ignores the BackingStore deleter callback if data == nullptr,
  // but our API contract requires it being called.
  if (data == nullptr) {
    ab->Detach();
    self->OnBackingStoreFree(); // This calls `callback` asynchronously.
  } else {
    // Store the ArrayBuffer so that we can detach it later.
    self->persistent_.Reset(env->isolate(), ab);
    self->persistent_.SetWeak();
  }

  return ab;
}

CallbackInfo::CallbackInfo(
    Environment *env,
    FreeCallback callback,
    char *data,
    void *hint)
    : callback_(callback), data_(data), hint_(hint), env_(env) {
  env->AddCleanupHook(CleanupHook, this);
  env->isolate()->AdjustAmountOfExternalAllocatedMemory(sizeof(*this));
}

void CallbackInfo::CleanupHook(void *data) {
  CallbackInfo *self = static_cast<CallbackInfo *>(data);

  {
    v8::HandleScope handle_scope(self->env_->isolate());
    v8::Local<v8::ArrayBuffer> ab =
        self->persistent_.Get(self->env_->isolate());
    if (!ab.IsEmpty() && ab->IsDetachable()) {
      ab->Detach();
      self->persistent_.Reset();
    }
  }

  // Call the callback in this case, but don't delete `this` yet because the
  // BackingStore deleter callback will do so later.
  self->CallAndResetCallback();
}

void CallbackInfo::CallAndResetCallback() {
  FreeCallback callback;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    callback = callback_;
    callback_ = nullptr;
  }
  if (callback != nullptr) {
    // Clean up all Environment-related state and run the callback.
    env_->RemoveCleanupHook(CleanupHook, this);
    int64_t change_in_bytes = -static_cast<int64_t>(sizeof(*this));
    env_->isolate()->AdjustAmountOfExternalAllocatedMemory(change_in_bytes);

    callback(data_, hint_);
  }
}

void CallbackInfo::OnBackingStoreFree() {
  // This method should always release the memory for `this`.
  std::unique_ptr<CallbackInfo> self{this};
  std::lock_guard<std::mutex> lock(mutex_);
  // If callback_ == nullptr, that means that the callback has already run from
  // the cleanup hook, and there is nothing left to do here besides to clean
  // up the memory involved. In particular, the underlying `Environment` may
  // be gone at this point, so don’t attempt to call SetImmediateThreadsafe().
  if (callback_ == nullptr)
    return;

  env_->SetImmediateThreadsafe([self = std::move(self)](Environment *env) {
    CHECK_EQ(self->env_, env); // Consistency check.

    self->CallAndResetCallback();
  });
}

} // anonymous namespace

v8::MaybeLocal<v8::Uint8Array> New(
    Environment *env,
    v8::Local<v8::ArrayBuffer> ab,
    size_t byte_offset,
    size_t length) {
  CHECK(!env->buffer_prototype_object().IsEmpty());
  v8::Local<v8::Uint8Array> ui = v8::Uint8Array::New(ab, byte_offset, length);
  v8::Maybe<bool> mb =
      ui->SetPrototype(env->context(), env->buffer_prototype_object());
  if (mb.IsNothing())
    return v8::MaybeLocal<v8::Uint8Array>();
  return ui;
}

v8::MaybeLocal<v8::Object> New(v8::Isolate* isolate,
                       char* data,
                       size_t length,
                       FreeCallback callback,
                       void* hint) {
  v8::EscapableHandleScope handle_scope(isolate);
  Environment* env = nullptr;//Environment::GetCurrent(isolate);
  if (env == nullptr) {
    callback(data, hint);
    //THROW_ERR_BUFFER_CONTEXT_NOT_AVAILABLE(isolate);
    return v8::MaybeLocal<v8::Object>();
  }
  return handle_scope.EscapeMaybe(
      Buffer::New(env, data, length, callback, hint));
}

v8::MaybeLocal<v8::Object> New(
    Environment *env,
    char *data,
    size_t length,
    FreeCallback callback,
    void *hint) {
  v8::EscapableHandleScope scope(env->isolate());

  if (length > kMaxLength) {
    // TODO: [vmoroz]
    // env->isolate()->ThrowException(ERR_BUFFER_TOO_LARGE(env->isolate()));
    callback(data, hint);
    return v8::Local<v8::Object>();
  }

  v8::Local<v8::ArrayBuffer> ab =
      CallbackInfo::CreateTrackedArrayBuffer(env, data, length, callback, hint);
  if (ab->SetPrivate(
            env->context(),
            env->untransferable_object_private_symbol(),
            True(env->isolate()))
          .IsNothing()) {
    return v8::Local<v8::Object>();
  }
  v8::MaybeLocal<v8::Uint8Array> maybe_ui = Buffer::New(env, ab, 0, length);

  v8::Local<v8::Uint8Array> ui;
  if (!maybe_ui.ToLocal(&ui))
    return v8::MaybeLocal<v8::Object>();

  return scope.Escape(ui);
}

void SetBufferPrototype(const v8::FunctionCallbackInfo<v8::Value> &args) {
  Environment *env =
      Environment::GetCurrent(args.GetIsolate()->GetCurrentContext());

  CHECK(args[0]->IsObject());
  v8::Local<v8::Object> proto = args[0].As<v8::Object>();
  env->set_buffer_prototype_object(proto);
}

} // namespace Buffer
} // namespace node

namespace v8impl {
class BufferFinalizer : private Finalizer {
 public:
  // node::Buffer::FreeCallback
  static void FinalizeBufferCallback(char *data, void *hint) {
    std::unique_ptr<BufferFinalizer, Deleter> finalizer{
        static_cast<BufferFinalizer *>(hint)};
    finalizer->_finalize_data = data;

    node::Environment *node_env = node::Environment::GetCurrent(
        static_cast<napi_env>(finalizer->_env)->context());
    node_env->SetImmediate(
        [finalizer = std::move(finalizer)](node::Environment *env) {
          if (finalizer->_finalize_callback == nullptr)
            return;

          v8::HandleScope handle_scope(finalizer->_env->isolate);
          v8::Context::Scope context_scope(finalizer->_env->context());

          finalizer->_env->CallIntoModule([&](napi_env env) {
            finalizer->_finalize_callback(
                env, finalizer->_finalize_data, finalizer->_finalize_hint);
          });
        });
  }

  struct Deleter {
    void operator()(BufferFinalizer *finalizer) {
      Finalizer::Delete(finalizer);
    }
  };
};
} // namespace v8impl

napi_status napi_create_external_buffer(
    napi_env env,
    size_t length,
    void *data,
    napi_finalize finalize_cb,
    void *finalize_hint,
    napi_value *result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Isolate *isolate = env->isolate;

  // The finalizer object will delete itself after invoking the callback.
  v8impl::Finalizer *finalizer = v8impl::Finalizer::New(
      env,
      finalize_cb,
      nullptr,
      finalize_hint,
      v8impl::Finalizer::kKeepEnvReference);

  auto maybe = node::Buffer::New(
      isolate,
      static_cast<char *>(data),
      length,
      v8impl::BufferFinalizer::FinalizeBufferCallback,
      finalizer);

  CHECK_MAYBE_EMPTY(env, maybe, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(maybe.ToLocalChecked());
  return GET_RETURN_STATUS(env);
  // Tell coverity that 'finalizer' should not be freed when we return
  // as it will be deleted when the buffer to which it is associated
  // is finalized.
  // coverity[leaked_storage]
}

// From node.cc
namespace node {
namespace per_process {
// util.h
// Tells whether the per-process V8::Initialize() is called and
// if it is safe to call v8::Isolate::GetCurrent().
bool v8_initialized = false;
}  // namespace per_process
}  // namespace node
