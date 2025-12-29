#pragma once

#include <string>
#include <functional>
/*
struct module {
    std::string name;
    int (*init)(void);
    void (*exit)(void);

    bool loaded = false;

    int load();
    void unload();
};
*/

//struct module;

extern "C" {
typedef struct module {
    const char* name;       // 插件名称
    const char** depends;   // 依赖项列表（NULL结尾）
    int (*init)(void);      // 初始化函数
    void (*exit)(void);     // 卸载函数
} module;
}

