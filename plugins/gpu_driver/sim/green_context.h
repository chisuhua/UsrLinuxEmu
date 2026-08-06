#pragma once

#include <cstdint>

class GreenContext {
 public:
  static GreenContext* create(uint64_t tsg_id);

  int destroy();

  uint64_t tsg_id() const { return tsg_id_; }

 private:
  GreenContext(uint64_t tsg_id) : tsg_id_(tsg_id) {}
  ~GreenContext() = default;

  uint64_t tsg_id_;
  bool destroyed_ = false;
};