// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once
#ifndef SRC_V8API_H_
#define SRC_V8API_H_

#include <v8_api.h>
#include "NodeApi.h"

namespace Microsoft::NodeApiJsi {

class V8Api : public NodeApi {
 public:
  V8Api(IFuncResolver *funcResolver);

  static V8Api *current() noexcept {
    return current_;
  }

  static void setCurrent(V8Api *current) noexcept {
    NodeApi::setCurrent(current);
    current_ = current;
  }

  static V8Api *fromLib();

  class Scope : public NodeApi::Scope {
   public:
    Scope() : Scope(V8Api::fromLib()) {}

    Scope(V8Api *v8Api) : NodeApi::Scope(v8Api), prevV8Api_(V8Api::current_) {
      V8Api::current_ = v8Api;
    }

    ~Scope() {
      V8Api::current_ = prevV8Api_;
    }

   private:
    V8Api *prevV8Api_;
  };

#define V8_FUNC(func) decltype(::func) *const func;
#include "V8ApiFunctions.inc"

 private:
  static thread_local V8Api *current_;
};

} // namespace Microsoft::NodeApiJsi

#endif // !SRC_V8API_H_
