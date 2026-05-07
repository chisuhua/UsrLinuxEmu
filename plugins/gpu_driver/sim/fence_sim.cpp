/*
 * fence_sim.cpp — Fence 仿真（影子编译）
 */

#include <atomic>
#include <map>
#include <mutex>

class FenceSim {
 public:
  uint64_t create() {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t id = ++counter_;
    fences_[id] = false;
    return id;
  }

  void signal(uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    fences_[id] = true;
  }

  bool isSignaled(uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = fences_.find(id);
    return it != fences_.end() && it->second;
  }

 private:
  std::map<uint64_t, bool> fences_;
  uint64_t counter_ = 0;
  mutable std::mutex mutex_;
};
