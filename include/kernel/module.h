#pragma once

#include <functional>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct module {
  const char* name;
  const char** depends;
  int (*init)(void);
  void (*exit)(void);
#ifdef __cplusplus
  bool loaded;

  int load();
  void unload();

  module() : name(nullptr), depends(nullptr), init(nullptr), exit(nullptr), loaded(false) {}
#endif
} module;

#ifdef __cplusplus
}
#endif
