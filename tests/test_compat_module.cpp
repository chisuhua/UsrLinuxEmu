// 测试模块加载兼容层
#include <iostream>
#include <cassert>
#include <cstring>

#include "../include/linux_compat/compat.h"

// 测试模块属性宏（只验证它们编译通过）
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Test Author");
MODULE_DESCRIPTION("Test Module");
MODULE_VERSION("1.0");

// 测试__init/__exit标记
static int __init test_module_init_fn() {
    return 0;
}

static void __exit test_module_exit_fn() {
}

// 测试EXPORT_SYMBOL宏（只验证编译通过）
static int exported_symbol = 42;
EXPORT_SYMBOL(exported_symbol);

// 测试THIS_MODULE
void test_this_module() {
    std::cout << "Testing THIS_MODULE...\n";
    void *mod = THIS_MODULE;
    assert(mod == nullptr); // 用户态下THIS_MODULE为NULL
    std::cout << "THIS_MODULE test passed.\n";
}

// 测试__init/__exit标记
void test_init_exit_attributes() {
    std::cout << "Testing __init/__exit attributes...\n";

    // 验证__init/__exit函数可以正常调用
    int ret = test_module_init_fn();
    assert(ret == 0);

    test_module_exit_fn();

    std::cout << "__init/__exit attributes test passed.\n";
}

// 测试printk（确保不崩溃）
void test_printk() {
    std::cout << "Testing printk compatibility...\n";

    printk(KERN_INFO "Test printk message\n");
    printk(KERN_WARNING "Test warning message\n");
    printk(KERN_ERR "Test error message\n");

    std::cout << "Printk compatibility test passed.\n";
}

int main() {
    std::cout << "Starting module compatibility tests...\n";

    test_this_module();
    test_init_exit_attributes();
    test_printk();

    std::cout << "All module compatibility tests passed!\n";
    return 0;
}
