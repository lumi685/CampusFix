cmake_minimum_required(VERSION 3.20)

project(CampusFixBackend VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Drogon CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

add_executable(campusfix_backend
    src/main.cpp
    src/controllers/HealthController.cpp
)

target_link_libraries(campusfix_backend
    PRIVATE
        Drogon::Drogon
        unofficial::sqlite3::sqlite3
)

if(MSVC)
    target_compile_options(campusfix_backend PRIVATE /W4 /permissive-)
endif()
