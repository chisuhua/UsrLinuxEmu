# sim/CMakeLists.txt — ③ hardware sim (host file backing disk)

# sim is header-only (inlined in storage_driver.cpp) for Stage 2.3.3
# No .cpp target needed; storage_drv links the sim implementation.