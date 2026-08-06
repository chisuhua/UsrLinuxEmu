#include "green_context.h"

#include <cerrno>
#include <cstdlib>

GreenContext* GreenContext::create(uint64_t tsg_id) {
  return new GreenContext(tsg_id);
}

int GreenContext::destroy() {
  if (destroyed_) return -EINVAL;
  destroyed_ = true;
  delete this;
  return 0;
}