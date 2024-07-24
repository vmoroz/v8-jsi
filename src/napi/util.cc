#include "util-inl.h"

namespace node {

using v8::HandleScope;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Nothing;
using v8::String;
using v8::Value;

// Fast, but can be triple UTF-8 strings <= 65536 chars in length
static v8::Maybe<size_t> Utf8StringStorageSize(v8::Isolate* isolate,
                                               v8::Local<v8::Value> val) {
  HandleScope scope(isolate);
  Local<String> str;
  if (!val->ToString(isolate->GetCurrentContext()).ToLocal(&str))
    return Nothing<size_t>();

  // A single UCS2 codepoint never takes up more than 3 utf8 bytes.
  // It is an exercise for the caller to decide when a string is
  // long enough to justify calling Size() instead of StorageSize()
  size_t data_size = 3 * str->Length();
  return Just(data_size);
};

template <typename T>
static void MakeUtf8String(Isolate* isolate,
                           Local<Value> value,
                           MaybeStackBuffer<T>* target) {
  Local<String> string;
  if (!value->ToString(isolate->GetCurrentContext()).ToLocal(&string)) return;

  size_t storage;
  if (!Utf8StringStorageSize(isolate, string).To(&storage)) return;
  storage += 1;
  target->AllocateSufficientStorage(storage);
  const int flags = String::NO_NULL_TERMINATION | String::REPLACE_INVALID_UTF8;
  const int length =
      string->WriteUtf8(isolate, target->out(), storage, nullptr, flags);
  target->SetLengthAndZeroTerminate(length);
}

Utf8Value::Utf8Value(Isolate* isolate, Local<Value> value) {
  if (value.IsEmpty()) return;

  MakeUtf8String(isolate, value, this);
}

void LowMemoryNotification() {
  if (per_process::v8_initialized) {
    auto isolate = Isolate::TryGetCurrent();
    if (isolate != nullptr) {
      isolate->LowMemoryNotification();
    }
  }
}

}  // namespace node