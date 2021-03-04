#pragma once

//
// This file contains simplified versions of classes required from node for the
// V8 NAPI implementation.
//

#include <v8.h>
#include "node_context_data.h"
#include "util.h"

#define PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(V) \
  V(napi_type_tag, "node:napi:type_tag")         \
  V(napi_wrapper, "node:napi:wrapper")

namespace node {

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

  // Strings and private symbols are shared across shared contexts
  // The getters simply proxy to the per-isolate primitive.
#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define V(TypeName, PropertyName) v8::Local<TypeName> PropertyName() const;
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
#undef V
#undef VP

  v8::Local<v8::Context> Environment::context() const;
  v8::Isolate *isolate() const;

 private:
  static void *const kNodeContextTagPtr;
  static int const kNodeContextTag;
  v8::Global<v8::Context> context_;
  IsolateData *const isolate_data_;
};

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
    : isolate_data_{isolate_data} {
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

inline v8::Local<v8::Context> Environment::context() const {
  return PersistentToLocal::Strong(context_);
}

inline v8::Isolate *Environment::isolate() const {
  return isolate_data_->isolate();
}

inline IsolateData *Environment::isolate_data() const {
  return isolate_data_;
}

} // namespace node