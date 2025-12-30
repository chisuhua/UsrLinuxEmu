#include "../include/linux_compat/compat.h"
#include <iostream>
#include <cassert>
#include <cstring>

// 测试类型定义兼容性
void test_type_definitions() {
    std::cout << "Testing type definitions...\n";
    
    // 测试基础类型大小
    static_assert(sizeof(u8) == 1, "u8 should be 1 byte");
    static_assert(sizeof(u16) == 2, "u16 should be 2 bytes");
    static_assert(sizeof(u32) == 4, "u32 should be 4 bytes");
    static_assert(sizeof(u64) == 8, "u64 should be 8 bytes");
    
    static_assert(sizeof(s8) == 1, "s8 should be 1 byte");
    static_assert(sizeof(s16) == 2, "s16 should be 2 bytes");
    static_assert(sizeof(s32) == 4, "s32 should be 4 bytes");
    static_assert(sizeof(s64) == 8, "s64 should be 8 bytes");
    
    // 测试__开头的类型
    static_assert(sizeof(__u8) == 1, "__u8 should be 1 byte");
    static_assert(sizeof(__u16) == 2, "__u16 should be 2 bytes");
    static_assert(sizeof(__u32) == 4, "__u32 should be 4 bytes");
    static_assert(sizeof(__u64) == 8, "__u64 should be 8 bytes");
    
    std::cout << "Type definitions test passed.\n";
}

// 测试宏定义
void test_macro_definitions() {
    std::cout << "Testing macro definitions...\n";
    
    // 测试ARRAY_SIZE
    int arr[] = {1, 2, 3, 4, 5};
    assert(ARRAY_SIZE(arr) == 5);
    
    // 测试位操作
    assert(BIT(0) == 1);
    assert(BIT(1) == 2);
    assert(BIT(2) == 4);
    assert(BIT(3) == 8);
    
    // 测试对齐
    assert(ALIGN(13, 8) == 16);
    assert(ALIGN(16, 8) == 16);
    assert(ALIGN(17, 8) == 24);
    
    // 测试min/max
    assert(min(5, 10) == 5);
    assert(max(5, 10) == 10);
    
    // 测试clamp
    assert(clamp(5, 1, 10) == 5);
    assert(clamp(0, 1, 10) == 1);
    assert(clamp(15, 1, 10) == 10);
    
    std::cout << "Macro definitions test passed.\n";
}

// 测试链表结构
void test_list_structure() {
    std::cout << "Testing list structure...\n";
    
    struct test_node {
        int data;
        struct list_head list;
    };
    
    struct test_node node;
    // 初始化链表节点
    node.list.next = &node.list;
    node.list.prev = &node.list;
    
    assert(node.list.next == &node.list);
    assert(node.list.prev == &node.list);
    
    std::cout << "List structure test passed.\n";
}

// 测试container_of宏
void test_container_of() {
    std::cout << "Testing container_of macro...\n";
    
    struct test_struct {
        int a;
        int b;
        struct list_head list;
        int c;
    };
    
    struct test_struct obj;
    obj.a = 10;
    obj.b = 20;
    obj.c = 30;
    
    // 使用container_of宏从成员地址获取结构体地址
    struct list_head *ptr = &obj.list;
    struct test_struct *result = container_of(ptr, struct test_struct, list);
    
    assert(result == &obj);
    assert(result->a == 10);
    assert(result->b == 20);
    assert(result->c == 30);
    
    std::cout << "Container_of test passed.\n";
}

// 测试错误处理函数
void test_error_handling() {
    std::cout << "Testing error handling functions...\n";
    
    // 测试错误指针处理
    void *error_ptr = ERR_PTR(-EINVAL);
    assert(IS_ERR(error_ptr));
    assert(PTR_ERR(error_ptr) == -EINVAL);
    
    // 测试NULL指针
    assert(IS_ERR_OR_NULL(NULL));
    
    std::cout << "Error handling test passed.\n";
}

int main() {
    std::cout << "Starting Linux compatibility types tests...\n";
    
    test_type_definitions();
    test_macro_definitions();
    test_list_structure();
    test_container_of();
    test_error_handling();
    
    std::cout << "All tests passed!\n";
    
    return 0;
}