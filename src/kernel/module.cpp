#include "module.h"
#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

int module::load() {
  if (loaded)
    return 0;
  std::cout << "[Module] Loading module: " << name << std::endl;
  if (init)
    init();
  loaded = true;
  return 0;
}

void module::unload() {
  if (!loaded)
    return;
  std::cout << "[Module] Unloading module: " << name << std::endl;
  if (exit)
    exit();
  loaded = false;
}

#ifdef __cplusplus
}
#endif
