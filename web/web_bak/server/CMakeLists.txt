cmake_minimum_required(VERSION 3.16)
project(guaguale_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MYSQL REQUIRED mysqlclient)
pkg_check_modules(HIREDIS REQUIRED hiredis)
pkg_check_modules(NLOHMANN_JSON REQUIRED nlohmann_json)

add_executable(guaguale_server
    main.cpp server.cpp http.cpp db.cpp handlers.cpp
)

target_include_directories(guaguale_server PRIVATE
    ${MYSQL_INCLUDE_DIRS} ${HIREDIS_INCLUDE_DIRS} ${NLOHMANN_JSON_INCLUDE_DIRS}
)

target_link_libraries(guaguale_server
    ${MYSQL_LIBRARIES} ${HIREDIS_LIBRARIES}
    OpenSSL::SSL OpenSSL::Crypto pthread
)
