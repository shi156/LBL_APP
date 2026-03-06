cmake_minimum_required(VERSION 3.20)
project(lbl_app_server VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LBL_BUILD_TESTS "Build tests" OFF)

# Force host build path priority and avoid accidental cross-toolchain leakage.
set(LBL_CROSS_IGNORE_PATH "/home/ss/rk3588_toolchain;/home/ss/rk3588_toolchain/aarch64-buildroot-linux-gnu/sysroot")
if(CMAKE_IGNORE_PATH)
  set(CMAKE_IGNORE_PATH "${CMAKE_IGNORE_PATH};${LBL_CROSS_IGNORE_PATH}")
else()
  set(CMAKE_IGNORE_PATH "${LBL_CROSS_IGNORE_PATH}")
endif()

if(EXISTS "/usr/bin/pkg-config")
  set(PKG_CONFIG_EXECUTABLE "/usr/bin/pkg-config" CACHE FILEPATH "Host pkg-config" FORCE)
endif()
set(OPENSSL_ROOT_DIR "/usr" CACHE PATH "Use system OpenSSL for host build" FORCE)

find_package(OpenSSL REQUIRED)

if(OPENSSL_SSL_LIBRARY MATCHES "rk3588_toolchain" OR OPENSSL_CRYPTO_LIBRARY MATCHES "rk3588_toolchain")
  message(FATAL_ERROR "Detected cross-toolchain OpenSSL path (${OPENSSL_SSL_LIBRARY}). Please run ./build.sh for host g++ build.")
endif()

set(MYSQL_ROOT "${CMAKE_SOURCE_DIR}/third_party/mysql/usr")
find_path(MYSQL_INCLUDE_DIR
  NAMES mysql/mysql.h
  PATHS
    /usr/include
    /usr/local/include
    "${MYSQL_ROOT}/include"
)

find_library(MYSQL_LIBRARY
  NAMES mysqlclient
  PATHS
    /usr/lib/x86_64-linux-gnu
    /usr/local/lib
    "${MYSQL_ROOT}/lib/x86_64-linux-gnu"
)

if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
  message(FATAL_ERROR
    "MySQL dev library not found. Either install default-libmysqlclient-dev or extract MySQL .deb files into third_party/mysql.")
endif()

add_executable(lbl_server
  src/main.cpp
  src/config/app_config.cpp
  src/core/thread_pool.cpp
  src/db/database.cpp
  src/http/http_parser.cpp
  src/http/http_response.cpp
  src/security/password_hasher.cpp
  src/security/token_service.cpp
  src/server/https_server.cpp
)

target_include_directories(lbl_server
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${MYSQL_INCLUDE_DIR}
)

target_link_libraries(lbl_server
  PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    ${MYSQL_LIBRARY}
    pthread
    dl
)

target_compile_options(lbl_server PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(lbl_server PROPERTIES
  BUILD_RPATH "${MYSQL_ROOT}/lib/x86_64-linux-gnu"
)
