#pragma once

#include <string>
#include <functional>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct module {
    const char* name;       // 插件名称
    const char** depends;   // 依赖项列表（NULL结尾）
    int (*init)(void);      // 初始化函数
    void (*exit)(void);     // 卸载函数
    
#ifdef __cplusplus
    // C++扩展功能
    bool loaded;            // 模块是否已加载

    int load();
    void unload();
    
    // 构造函数
    module() : name(nullptr), depends(nullptr), init(nullptr), exit(nullptr), loaded(false) {}
#endif
} module;

#ifdef __cplusplus
}
#endif