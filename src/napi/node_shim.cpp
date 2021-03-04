#include "node_shim.h"

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

} // namespace node