cmake_minimum_required(VERSION 3.20)

project(CampusFixBackend VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Drogon CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(jsoncpp CONFIG REQUIRED) # Already installed as a Drogon dependency; reuse its safe JSON parser.

add_library(campusfix_database STATIC
    src/repositories/CatalogRepository.cpp
    src/services/CatalogService.cpp
    src/infrastructure/DatabaseMigration.cpp
    src/infrastructure/SqliteConnection.cpp
    src/infrastructure/DemoSeed.cpp
    src/infrastructure/AuthCrypto.cpp
    src/repositories/AuthRepository.cpp
    src/services/AuthService.cpp
    src/services/DemoAccountActivation.cpp
    src/domain/AuthorizationPolicy.cpp
    src/repositories/AuthorizationRepository.cpp
    src/services/AuthorizationService.cpp
    src/domain/WorkOrderValidation.cpp
    src/repositories/WorkOrderRepository.cpp
    src/services/WorkOrderService.cpp
    src/repositories/WorkOrderQueries.cpp
    src/services/WorkOrderReadService.cpp
    src/domain/WorkOrderStateMachine.cpp
    src/repositories/StatusLogRepository.cpp
    src/services/WorkOrderTimelineService.cpp
    src/repositories/ReviewRepository.cpp
    src/services/WorkOrderReviewService.cpp
    src/services/WorkOrderEditService.cpp
)
target_include_directories(campusfix_database PUBLIC src)
target_link_libraries(campusfix_database PUBLIC unofficial::sqlite3::sqlite3 unofficial-sodium::sodium)
target_link_libraries(campusfix_database PRIVATE JsonCpp::JsonCpp)

add_library(campusfix_http STATIC
    src/controllers/CatalogController.cpp
    src/controllers/HealthController.cpp
    src/controllers/AuthController.cpp
    src/middleware/AuthGuard.cpp
    src/controllers/WorkOrderController.cpp
    src/controllers/WorkOrderQueryController.cpp
    src/controllers/WorkOrderTimelineController.cpp
    src/controllers/ReviewController.cpp
)
target_link_libraries(campusfix_http PUBLIC Drogon::Drogon campusfix_database)
add_executable(campusfix_backend src/main.cpp)

target_link_libraries(campusfix_backend
    PRIVATE
        campusfix_http
)

if(MSVC)
    target_compile_options(campusfix_backend PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(campusfix_database PRIVATE /W4 /permissive-)
    target_compile_options(campusfix_http PRIVATE /W4 /permissive- /utf-8)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(campusfix_edit_test tests/integration/WorkOrderEditTest.cpp)
    target_link_libraries(campusfix_edit_test PRIVATE campusfix_database)
    target_compile_definitions(campusfix_edit_test PRIVATE CAMPUSFIX_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/migrations" CAMPUSFIX_SEED_FILE="${CMAKE_CURRENT_SOURCE_DIR}/seeds/001_demo_data.sql")
    add_executable(campusfix_edit_http_fixture tests/integration/EditHttpFixture.cpp)
    target_link_libraries(campusfix_edit_http_fixture PRIVATE campusfix_database Drogon::Drogon)
    if(MSVC)
        target_compile_options(campusfix_edit_test PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(campusfix_edit_http_fixture PRIVATE /W4 /permissive- /utf-8)
    endif()
    add_test(NAME work_order_edit_service COMMAND campusfix_edit_test)
    find_program(CAMPUSFIX_POWERSHELL NAMES powershell.exe REQUIRED)
    add_test(NAME work_order_edit_http COMMAND "${CAMPUSFIX_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/WorkOrderEditHttpTest.ps1"
        -Server "$<TARGET_FILE:campusfix_backend>" -Fixture "$<TARGET_FILE:campusfix_edit_http_fixture>" -Migrations "${CMAKE_CURRENT_SOURCE_DIR}/migrations" -Seed "${CMAKE_CURRENT_SOURCE_DIR}/seeds/001_demo_data.sql"
        -EvidencePath "${CMAKE_CURRENT_BINARY_DIR}/Testing/w5d2-evidence.json")
    set_tests_properties(work_order_edit_service work_order_edit_http PROPERTIES TIMEOUT 120)
    add_executable(campusfix_review_test tests/integration/ReviewServiceTest.cpp)
    target_link_libraries(campusfix_review_test PRIVATE campusfix_database)
    target_compile_definitions(campusfix_review_test PRIVATE
        CAMPUSFIX_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/migrations"
        CAMPUSFIX_SEED_FILE="${CMAKE_CURRENT_SOURCE_DIR}/seeds/001_demo_data.sql")
    add_executable(campusfix_review_http_fixture tests/integration/ReviewHttpFixture.cpp)
    target_link_libraries(campusfix_review_http_fixture PRIVATE campusfix_database Drogon::Drogon)
    if(MSVC)
        target_compile_options(campusfix_review_test PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(campusfix_review_http_fixture PRIVATE /W4 /permissive- /utf-8)
    endif()
    add_test(NAME work_order_review_service COMMAND campusfix_review_test)
    find_program(CAMPUSFIX_POWERSHELL NAMES powershell.exe REQUIRED)
    add_test(NAME work_order_review_http COMMAND "${CAMPUSFIX_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/ReviewHttpTest.ps1"
        -Server "$<TARGET_FILE:campusfix_backend>" -Fixture "$<TARGET_FILE:campusfix_review_http_fixture>"
        -Migrations "${CMAKE_CURRENT_SOURCE_DIR}/migrations" -Seed "${CMAKE_CURRENT_SOURCE_DIR}/seeds/001_demo_data.sql"
        -EvidencePath "${CMAKE_CURRENT_BINARY_DIR}/Testing/w5d1-evidence.json")
    set_tests_properties(work_order_review_service work_order_review_http PROPERTIES TIMEOUT 120)
    add_executable(campusfix_report_acceptance_fixture tests/integration/ReportAcceptanceFixture.cpp)
    target_link_libraries(campusfix_report_acceptance_fixture PRIVATE campusfix_database Drogon::Drogon)
    if(MSVC)
        target_compile_options(campusfix_report_acceptance_fixture PRIVATE /W4 /permissive- /utf-8)
    endif()
    find_program(CAMPUSFIX_POWERSHELL NAMES powershell.exe REQUIRED)
    add_test(NAME report_acceptance_http COMMAND "${CAMPUSFIX_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/ReportAcceptanceHttpTest.ps1"
        -Server "$<TARGET_FILE:campusfix_backend>" -Fixture "$<TARGET_FILE:campusfix_report_acceptance_fixture>"
        -Migrations "${CMAKE_CURRENT_SOURCE_DIR}/migrations" -Seed "${CMAKE_CURRENT_SOURCE_DIR}/seeds/001_demo_data.sql"
        -EvidencePath "${CMAKE_CURRENT_BINARY_DIR}/Testing/w4d5-evidence.json")
    set_tests_properties(report_acceptance_http PROPERTIES TIMEOUT 90)
    add_executable(campusfix_catalog_test tests/integration/CatalogTest.cpp)
    target_link_libraries(campusfix_catalog_test PRIVATE campusfix_database Drogon::Drogon)
    target_compile_definitions(campusfix_catalog_test PRIVATE
        CAMPUSFIX_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/migrations"
        CAMPUSFIX_SEED_FILE="${CMAKE_CURRENT_SOURCE_DIR}/seeds/001_demo_data.sql")
    if(MSVC)
        target_compile_options(campusfix_catalog_test PRIVATE /W4 /permissive- /utf-8)
    endif()
    add_test(NAME catalog_service COMMAND campusfix_catalog_test)
    find_program(CAMPUSFIX_POWERSHELL NAMES powershell.exe REQUIRED)
    add_test(NAME catalog_http COMMAND "${CAMPUSFIX_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/CatalogHttpTest.ps1"
        -Server "$<TARGET_FILE:campusfix_backend>" -Fixture "$<TARGET_FILE:campusfix_catalog_test>"
        -Migrations "${CMAKE_CURRENT_SOURCE_DIR}/migrations" -Seed "${CMAKE_CURRENT_SOURCE_DIR}/seeds/001_demo_data.sql")
    set_tests_properties(catalog_http PROPERTIES TIMEOUT 60)
    add_executable(campusfix_state_machine_test tests/unit/WorkOrderStateMachineTest.cpp)
    target_link_libraries(campusfix_state_machine_test PRIVATE campusfix_database)
    add_test(NAME work_order_state_machine COMMAND campusfix_state_machine_test)
    add_executable(campusfix_timeline_test tests/integration/TimelineServiceTest.cpp)
    target_link_libraries(campusfix_timeline_test PRIVATE campusfix_database)
    target_compile_definitions(campusfix_timeline_test PRIVATE
        CAMPUSFIX_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/migrations"
        CAMPUSFIX_SEED_FILE="${CMAKE_CURRENT_SOURCE_DIR}/seeds/001_demo_data.sql")
    add_test(NAME work_order_timeline_service COMMAND campusfix_timeline_test)
    add_executable(campusfix_timeline_http_fixture tests/integration/TimelineHttpFixture.cpp)
    target_link_libraries(campusfix_timeline_http_fixture PRIVATE campusfix_database Drogon::Drogon)
    if(MSVC)
        target_compile_options(campusfix_state_machine_test PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(campusfix_timeline_test PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(campusfix_timeline_http_fixture PRIVATE /W4 /permissive- /utf-8)
    endif()
    find_program(CAMPUSFIX_POWERSHELL NAMES powershell.exe REQUIRED)
    add_test(NAME work_order_timeline_http COMMAND "${CAMPUSFIX_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/TimelineHttpTest.ps1"
        -Server "$<TARGET_FILE:campusfix_backend>"
        -Fixture "$<TARGET_FILE:campusfix_timeline_http_fixture>"
        -Migrations "${CMAKE_CURRENT_SOURCE_DIR}/migrations"
        -Seed "${CMAKE_CURRENT_SOURCE_DIR}/seeds/001_demo_data.sql")
    set_tests_properties(work_order_timeline_http PROPERTIES TIMEOUT 60)
    add_executable(campusfix_work_order_read_test tests/integration/WorkOrderReadTest.cpp)
    target_link_libraries(campusfix_work_order_read_test PRIVATE campusfix_database)
    target_compile_definitions(campusfix_work_order_read_test PRIVATE
        CAMPUSFIX_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/migrations"
        CAMPUSFIX_SEED_FILE="${CMAKE_CURRENT_SOURCE_DIR}/seeds/001_demo_data.sql")
    add_test(NAME work_order_read_service COMMAND campusfix_work_order_read_test)
    add_executable(campusfix_work_order_read_http_fixture tests/integration/WorkOrderReadHttpFixture.cpp)
    target_link_libraries(campusfix_work_order_read_http_fixture PRIVATE campusfix_database Drogon::Drogon)
    if(MSVC)
        target_compile_options(campusfix_work_order_read_test PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(campusfix_work_order_read_http_fixture PRIVATE /W4 /permissive- /utf-8)
    endif()
    find_program(CAMPUSFIX_POWERSHELL NAMES powershell.exe REQUIRED)
    add_test(NAME work_order_read_http COMMAND "${CAMPUSFIX_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/WorkOrderReadHttpTest.ps1"
        -Server "$<TARGET_FILE:campusfix_backend>"
        -Fixture "$<TARGET_FILE:campusfix_work_order_read_http_fixture>"
        -Migrations "${CMAKE_CURRENT_SOURCE_DIR}/migrations"
        -Seed "${CMAKE_CURRENT_SOURCE_DIR}/seeds/001_demo_data.sql")
    set_tests_properties(work_order_read_http PROPERTIES TIMEOUT 60)
    add_executable(campusfix_work_order_test tests/integration/WorkOrderServiceTest.cpp)
    target_link_libraries(campusfix_work_order_test PRIVATE campusfix_database)
    target_compile_definitions(campusfix_work_order_test PRIVATE
        CAMPUSFIX_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/migrations"
        CAMPUSFIX_SEED_FILE="${CMAKE_CURRENT_SOURCE_DIR}/seeds/001_demo_data.sql")
    add_test(NAME work_order_service COMMAND campusfix_work_order_test)
    set_tests_properties(work_order_service PROPERTIES TIMEOUT 45)
    add_executable(campusfix_work_order_http_fixture tests/integration/WorkOrderHttpFixture.cpp)
    target_link_libraries(campusfix_work_order_http_fixture PRIVATE campusfix_database Drogon::Drogon)
    if(MSVC)
        target_compile_options(campusfix_work_order_test PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(campusfix_work_order_http_fixture PRIVATE /W4 /permissive- /utf-8)
    endif()
    find_program(CAMPUSFIX_POWERSHELL NAMES powershell.exe REQUIRED)
    add_test(NAME work_order_http COMMAND "${CAMPUSFIX_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/WorkOrderHttpTest.ps1"
        -Server "$<TARGET_FILE:campusfix_backend>"
        -Fixture "$<TARGET_FILE:campusfix_work_order_http_fixture>"
        -Migrations "${CMAKE_CURRENT_SOURCE_DIR}/migrations"
        -Seed "${CMAKE_CURRENT_SOURCE_DIR}/seeds/001_demo_data.sql")
    set_tests_properties(work_order_http PROPERTIES TIMEOUT 90)
    add_executable(campusfix_authorization_test tests/integration/AuthorizationTest.cpp)
    target_link_libraries(campusfix_authorization_test PRIVATE campusfix_database)
    target_compile_definitions(campusfix_authorization_test PRIVATE
        CAMPUSFIX_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/migrations"
        CAMPUSFIX_SEED_FILE="${CMAKE_CURRENT_SOURCE_DIR}/seeds/001_demo_data.sql")
    if(MSVC)
        target_compile_options(campusfix_authorization_test PRIVATE /W4 /permissive-)
    endif()
    add_test(NAME authorization_policy COMMAND campusfix_authorization_test)

    add_executable(campusfix_guard_test_server tests/integration/GuardTestServer.cpp)
    target_link_libraries(campusfix_guard_test_server PRIVATE campusfix_http)
    if(MSVC)
        target_compile_options(campusfix_guard_test_server PRIVATE /W4 /permissive- /utf-8)
    endif()
    find_program(CAMPUSFIX_POWERSHELL NAMES powershell.exe REQUIRED)
    add_test(NAME guard_http COMMAND "${CAMPUSFIX_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/GuardHttpTest.ps1"
        -Server "$<TARGET_FILE:campusfix_guard_test_server>"
        -Migrations "${CMAKE_CURRENT_SOURCE_DIR}/migrations"
        -Seed "${CMAKE_CURRENT_SOURCE_DIR}/seeds/001_demo_data.sql")
    set_tests_properties(guard_http PROPERTIES TIMEOUT 60)

    add_executable(campusfix_sodium_test tests/integration/SodiumTest.cpp)
    target_link_libraries(campusfix_sodium_test PRIVATE unofficial-sodium::sodium)
    if(MSVC)
        target_compile_options(campusfix_sodium_test PRIVATE /W4 /permissive-)
    endif()
    add_test(NAME sodium_crypto COMMAND campusfix_sodium_test)

    add_executable(campusfix_auth_fixture tests/integration/AuthFixture.cpp)
    target_link_libraries(campusfix_auth_fixture PRIVATE campusfix_database)
    if(MSVC)
        target_compile_options(campusfix_auth_fixture PRIVATE /W4 /permissive-)
    endif()

    add_executable(campusfix_auth_service_test tests/integration/AuthServiceTest.cpp)
    target_link_libraries(campusfix_auth_service_test PRIVATE campusfix_database)
    target_compile_definitions(campusfix_auth_service_test PRIVATE
        CAMPUSFIX_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/migrations"
        CAMPUSFIX_SEED_FILE="${CMAKE_CURRENT_SOURCE_DIR}/seeds/001_demo_data.sql")
    if(MSVC)
        target_compile_options(campusfix_auth_service_test PRIVATE /W4 /permissive-)
    endif()
    add_test(NAME auth_service COMMAND campusfix_auth_service_test)

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
