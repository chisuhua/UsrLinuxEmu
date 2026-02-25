// 测试字符设备兼容层
#include <iostream>
#include <cassert>
#include <cstring>

#include "../include/linux_compat/compat.h"

// 测试设备号操作
void test_dev_number_ops() {
    std::cout << "Testing device number operations...\n";

    dev_t dev = MKDEV(42, 7);
    assert(MAJOR(dev) == 42);
    assert(MINOR(dev) == 7);

    // 测试边界值
    dev_t dev2 = MKDEV(0, 0);
    assert(MAJOR(dev2) == 0);
    assert(MINOR(dev2) == 0);

    std::cout << "Device number operations test passed.\n";
}

// 测试chrdev区域分配
void test_chrdev_region() {
    std::cout << "Testing chrdev region allocation...\n";

    dev_t dev;
    int ret = alloc_chrdev_region(&dev, 0, 1, "test_device");
    assert(ret == 0);
    assert(MAJOR(dev) != 0);

    // 注销区域
    unregister_chrdev_region(dev, 1);

    // 测试register_chrdev_region
    ret = register_chrdev_region(MKDEV(100, 0), 1, "fixed_device");
    assert(ret == 0);

    std::cout << "Chrdev region test passed.\n";
}

// 测试cdev操作
void test_cdev_ops() {
    std::cout << "Testing cdev operations...\n";

    struct file_operations fops;
    memset(&fops, 0, sizeof(fops));

    struct cdev cd;
    cdev_init(&cd, &fops);
    assert(cd.ops == &fops);

    dev_t dev = MKDEV(200, 0);
    int ret = cdev_add(&cd, dev, 1);
    assert(ret == 0);
    assert(cd.dev == dev);
    assert(cd.count == 1);

    cdev_del(&cd);

    std::cout << "Cdev operations test passed.\n";
}

int main() {
    std::cout << "Starting cdev compatibility tests...\n";

    test_dev_number_ops();
    test_chrdev_region();
    test_cdev_ops();

    std::cout << "All cdev compatibility tests passed!\n";
    return 0;
}
