# sim/CMakeLists.txt — ③ hardware sim (loopback NIC echo)

# sim is header-only (inlined in net_driver.cpp) for Stage 2.2.3
# No .cpp target needed; net_drv links the sim implementation.