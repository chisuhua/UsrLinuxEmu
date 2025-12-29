# 构建主程序
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# 构建插件
cd ..
make -C plugins

# 运行主程序
./build/user_kernel_emu
