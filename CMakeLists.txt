cmake_minimum_required(VERSION 3.10)
project(simple_server)

# 生成compile_commands.json供clangd使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加头文件路径
include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/WebApps/GomokuServer/include
    /usr/include/mysql-cppconn-8
    /usr/include/mysql
)

# 查找必要的库
find_library(MYSQLCPPCONN_LIBRARY
    NAMES mysqlcppconn mysqlcppconn8
    PATHS /usr/lib /usr/lib64 /usr/local/lib
)

# 添加所有源文件（httpserver 下大量为 .cc，不能只匹配 .cpp）
file(GLOB_RECURSE HTTP_SERVER_SRC
    "${PROJECT_SOURCE_DIR}/src/httpserver/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/httpserver/**/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/httpserver/*.cc"
    "${PROJECT_SOURCE_DIR}/src/httpserver/**/*.cc"
    "${PROJECT_SOURCE_DIR}/src/muduo/*.cc"
)

add_library(http_server STATIC ${HTTP_SERVER_SRC})

target_include_directories(http_server PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(http_server PUBLIC
    HTTP_SERVER_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

file(GLOB_RECURSE GOMOKU_SERVER_SRC
    "${PROJECT_SOURCE_DIR}/WebApps/GomokuServer/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/WebApps/GomokuServer/src/handlers/*.cpp"
)

add_library(gomoku_server STATIC ${GOMOKU_SERVER_SRC})

target_include_directories(gomoku_server PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/WebApps/GomokuServer/include
)
target_compile_definitions(gomoku_server PUBLIC
    HTTP_SERVER_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

# 添加主程序文件
set(MAIN_SRC "${PROJECT_SOURCE_DIR}/WebApps/GomokuServer/src/main.cpp")

# 添加可执行文件
add_executable(simple_server
    ${MAIN_SRC}
)

# gomoku_server 依赖 http_server 中的符号；静态库链接顺序要求被依赖库在后
target_link_libraries(simple_server gomoku_server http_server)

# 链接必要的库
target_link_libraries(simple_server
    pthread
)

# 打印调试信息
message(STATUS "Include directories:")
get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
    message(STATUS "  ${dir}")
endforeach()

message(STATUS "Main source file: ${MAIN_SRC}")
message(STATUS "HTTP Server sources: ${HTTP_SERVER_SRC}")
message(STATUS "Gomoku Server sources: ${GOMOKU_SERVER_SRC}")

find_package(OpenSSL REQUIRED)

target_link_libraries(simple_server
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(MYSQLCPPCONN_LIBRARY)
    target_link_libraries(simple_server ${MYSQLCPPCONN_LIBRARY})
else()
    message(WARNING "MySQL Connector C++ library not found; link may fail if DbConnection is used")
endif()
