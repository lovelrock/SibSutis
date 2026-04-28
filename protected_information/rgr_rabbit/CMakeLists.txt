cmake_minimum_required(VERSION 3.10)
project(RGR_Rabbit C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 99)

# Опции компиляции
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall")

# Заголовочные файлы
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ecrypt
    /usr/include
)

# Исходные файлы
set(SOURCES
    src/main.cpp
    src/ecdh.cpp
    src/sha3.cpp
    src/rabbit.c
    src/utils.cpp
    src/rdtsc.cpp
)

# Создаем исполняемые файлы
add_executable(secure_client ${SOURCES})
add_executable(secure_server ${SOURCES})

# Определения
target_compile_definitions(secure_client PRIVATE MODE_CLIENT)
target_compile_definitions(secure_server PRIVATE MODE_SERVER)

# Линковка
target_link_libraries(secure_client gmp pthread)
target_link_libraries(secure_server gmp pthread)

# Установка
install(TARGETS secure_client secure_server DESTINATION bin)
