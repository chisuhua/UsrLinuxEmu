# plugins/Makefile - 独立示例插件构建
#
# ⚠️ 注意: 此 Makefile 仅用于构建示例插件 sample_driver.so
#
# GPU 驱动插件 (gpu_driver_plugin.so) 通过 CMake 构建系统管理:
#   cd build && cmake .. && make gpu_driver_plugin
#
# 此文件不参与 CMake 构建流程，是遗留文件 (legacy)。
#
# 示例插件用途: 演示如何编写简单的 Linux 设备驱动插件
# 主要文件: drivers/sample_driver.cpp

PLUGINS = sample_driver.so

all: $(PLUGINS)

sample_driver.so:
	g++ -fPIC -shared ../drivers/sample_driver.cpp -o $@ -I../include

clean:
	rm -f *.so
