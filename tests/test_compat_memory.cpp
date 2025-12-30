#include "../include/linux_compat/compat.h"
#include <iostream>
#include <cassert>
#include <cstring>

// 测试kmalloc/kfree
void test_kmalloc_kfree() {
    std::cout << "Testing kmalloc/kfree...\n";
    
    // 测试基本分配
    char *ptr = (char *)kmalloc(100, GFP_KERNEL);
    assert(ptr != nullptr);
    
    // 测试写入和读取
    strcpy(ptr, "Hello World");
    assert(strcmp(ptr, "Hello World") == 0);
    
    kfree(ptr);
    
    // 测试零大小分配
    ptr = (char *)kmalloc(0, GFP_KERNEL);
    assert(ptr == nullptr);
    
    // 测试小块内存分配
    ptr = (char *)kmalloc(1, GFP_KERNEL);
    assert(ptr != nullptr);
    *ptr = 'A';
    assert(*ptr == 'A');
    kfree(ptr);
    
    std::cout << "kmalloc/kfree test passed.\n";
}

// 测试kzalloc
void test_kzalloc() {
    std::cout << "Testing kzalloc...\n";
    
    // 分配内存并验证是否初始化为零
    char *ptr = (char *)kzalloc(100, GFP_KERNEL);
    assert(ptr != nullptr);
    
    // 检查内存是否全为零
    for (int i = 0; i < 100; i++) {
        assert(ptr[i] == 0);
    }
    
    kfree(ptr);
    
    // 测试零大小分配
    ptr = (char *)kzalloc(0, GFP_KERNEL);
    assert(ptr == nullptr);
    
    std::cout << "kzalloc test passed.\n";
}

// 测试kcalloc
void test_kcalloc() {
    std::cout << "Testing kcalloc...\n";
    
    // 分配数组并验证是否初始化为零
    int *arr = (int *)kcalloc(10, sizeof(int), GFP_KERNEL);
    assert(arr != nullptr);
    
    // 检查内存是否全为零
    for (int i = 0; i < 10; i++) {
        assert(arr[i] == 0);
    }
    
    // 测试赋值
    for (int i = 0; i < 10; i++) {
        arr[i] = i;
    }
    
    for (int i = 0; i < 10; i++) {
        assert(arr[i] == i);
    }
    
    kfree(arr);
    
    // 测试零元素分配
    arr = (int *)kcalloc(0, sizeof(int), GFP_KERNEL);
    assert(arr == nullptr);
    
    // 测试零大小分配
    arr = (int *)kcalloc(10, 0, GFP_KERNEL);
    assert(arr == nullptr);
    
    std::cout << "kcalloc test passed.\n";
}

// 测试vmalloc/vfree
void test_vmalloc_vfree() {
    std::cout << "Testing vmalloc/vfree...\n";
    
    // 测试基本分配
    char *ptr = (char *)vmalloc(200);
    assert(ptr != nullptr);
    
    // 测试写入和读取
    strcpy(ptr, "Vmalloc Test");
    assert(strcmp(ptr, "Vmalloc Test") == 0);
    
    vfree(ptr);
    
    // 测试零大小分配
    ptr = (char *)vmalloc(0);
    assert(ptr == nullptr);
    
    std::cout << "vmalloc/vfree test passed.\n";
}

// 测试内存拷贝函数
void test_memory_copy() {
    std::cout << "Testing memory copy functions...\n";
    
    char src[20] = "Source Data";
    char dest[20];
    
    // 测试memcpy_toio (实际上就是memcpy)
    memcpy_toio(dest, src, strlen(src) + 1);
    assert(strcmp(dest, "Source Data") == 0);
    
    // 测试memcpy_fromio (实际上就是memcpy)
    char dest2[20];
    memcpy_fromio(dest2, src, strlen(src) + 1);
    assert(strcmp(dest2, "Source Data") == 0);
    
    // 测试memset_io (实际上就是memset)
    memset_io(dest2, 'X', 5);
    assert(strncmp(dest2, "XXXXX", 5) == 0);
    
    std::cout << "Memory copy functions test passed.\n";
}

// 测试物理/虚拟地址转换
void test_address_conversion() {
    std::cout << "Testing address conversion functions...\n";
    
    char test_data = 'A';
    char *ptr = &test_data;
    
    // 测试虚拟到物理地址转换
    unsigned long phys_addr = __pa(ptr);
    char *back_to_virt = (char *)__va(phys_addr);
    
    // 验证转换后的地址访问数据一致
    assert(*back_to_virt == 'A');
    *back_to_virt = 'B';
    assert(test_data == 'B');
    
    std::cout << "Address conversion test passed.\n";
}

// 测试页面相关功能
void test_page_functions() {
    std::cout << "Testing page functions...\n";
    
    // 验证页面大小
    assert(PAGE_SIZE == 4096);
    
    // 测试对齐函数
    assert(round_up(4097, PAGE_SIZE) == 8192);
    assert(round_down(8191, PAGE_SIZE) == 4096);
    
    // 测试页面对齐
    assert(PAGE_ALIGN(4096) == 4096);
    assert(PAGE_ALIGN(4097) == 8192);
    
    std::cout << "Page functions test passed.\n";
}

int main() {
    std::cout << "Starting Linux compatibility memory tests...\n";
    
    test_kmalloc_kfree();
    test_kzalloc();
    test_kcalloc();
    test_vmalloc_vfree();
    test_memory_copy();
    test_address_conversion();
    test_page_functions();
    
    std::cout << "All memory compatibility tests passed!\n";
    
    return 0;
}