cmake_minimum_required(VERSION 3.20)

project(CampusFixBackend VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Drogon CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

add_library(campusfix_database STATIC
    src/infrastructure/DatabaseMigration.cpp
    src/infrastructure/SqliteConnection.cpp
    src/infrastructure/DemoSeed.cpp
)
target_include_directories(campusfix_database PUBLIC src)
target_link_libraries(campusfix_database PUBLIC unofficial::sqlite3::sqlite3)

add_executable(campusfix_backend
    src/main.cpp
    src/controllers/HealthController.cpp
)

target_link_libraries(campusfix_backend
    PRIVATE
        Drogon::Drogon
        campusfix_database
)

if(MSVC)
    target_compile_options(campusfix_backend PRIVATE /W4 /permissive-)
    target_compile_options(campusfix_database PRIVATE /W4 /permissive-)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(campusfix_schema_test tests/integration/SchemaMigrationTest.cpp)
    target_link_libraries(campusfix_schema_test PRIVATE campusfix_database)
    target_compile_definitions(campusfix_schema_test PRIVATE
        CAMPUSFIX_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/migrations")
    if(MSVC)
        target_compile_options(campusfix_schema_test PRIVATE /W4 /permissive-)
    endif()
    add_test(NAME schema_migration COMMAND campusfix_schema_test)

    add_executable(campusfix_demo_seed_test tests/integration/DemoSeedTest.cpp)
    target_link_libraries(campusfix_demo_seed_test PRIVATE campusfix_database)
    target_compile_definitions(campusfix_demo_seed_test PRIVATE
        CAMPUSFIX_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/migrations"
        CAMPUSFIX_SEED_FILE="${CMAKE_CURRENT_SOURCE_DIR}/seeds/001_demo_data.sql")
    if(MSVC)
        target_compile_options(campusfix_demo_seed_test PRIVATE /W4 /permissive-)
    endif()
    add_test(NAME demo_seed COMMAND campusfix_demo_seed_test)
endif()
