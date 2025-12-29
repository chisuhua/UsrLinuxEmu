add_executable(cli main.cpp)
target_link_libraries(cli PRIVATE kernel)

# 可选：添加 JSON 头文件路径（如果你把 json.hpp 放在第三方目录）
# target_include_directories(cli PRIVATE ${PROJECT_SOURCE_DIR}/third_party/json)
