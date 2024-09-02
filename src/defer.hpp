#pragma once

#ifndef defer
#define DEFER_CONCAT_INNER(a, b) a##b
#define DEFER_CONCAT(a, b) DEFER_CONCAT_INNER(a, b)
#define DEFER_IDENT(base) DEFER_CONCAT(base, __COUNTER__)

template <typename Callable>
struct DeferStruct : Callable {
  ~DeferStruct() {
    this->operator()();
  }
};

#define defer(callable) auto DEFER_IDENT(defer__) = DeferStruct{callable};
#endif
