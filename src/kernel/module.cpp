#include "module.h"
#include <iostream>

int module::load() {
    if (loaded) return 0;
    std::cout << "[Module] Loading module: " << name << std::endl;
    if (init) init();
    loaded = true;
    return 0;
}

void module::unload() {
    if (!loaded) return;
    std::cout << "[Module] Unloading module: " << name << std::endl;
    if (exit) exit();
    loaded = false;
}
