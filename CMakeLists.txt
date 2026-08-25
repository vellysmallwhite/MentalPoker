cmake_minimum_required(VERSION 3.28)

project(MentalPoker VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

option(
    MENTALPOKER_ENABLE_ASAN_UBSAN
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer for legacy targets"
    OFF
)

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GMP REQUIRED IMPORTED_TARGET gmp)
pkg_check_modules(GMPXX REQUIRED IMPORTED_TARGET gmpxx)

add_executable(legacy_v1_smoke
    src/main.cpp
    src/application/EventQueue.cpp
    src/application/GameEngine.cpp
    src/application/MembershipList.cpp
    src/network/Consensus.cpp
    src/network/NetworkManager.cpp
    src/utils/CryptoUtils.cpp
)

set_target_properties(legacy_v1_smoke PROPERTIES
    CXX_EXTENSIONS OFF
    OUTPUT_NAME mental-poker-legacy
)

target_compile_features(legacy_v1_smoke PRIVATE cxx_std_20)

target_include_directories(legacy_v1_smoke PRIVATE
    "${PROJECT_SOURCE_DIR}/src/application"
    "${PROJECT_SOURCE_DIR}/src/network"
    "${PROJECT_SOURCE_DIR}/src/utils"
)

target_compile_options(legacy_v1_smoke PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
)

target_link_libraries(legacy_v1_smoke PRIVATE
    Boost::system
    Boost::thread
    JsonCpp::JsonCpp
    PkgConfig::GMP
    PkgConfig::GMPXX
    Threads::Threads
)

if(MENTALPOKER_ENABLE_ASAN_UBSAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(legacy_v1_smoke PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(legacy_v1_smoke PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
    else()
        message(FATAL_ERROR "ASan/UBSan preset requires GCC or Clang")
    endif()
endif()

if(BUILD_TESTING)
    add_test(
        NAME legacy_v1_missing_environment
        COMMAND
            "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:legacy_v1_smoke>"
            -P "${PROJECT_SOURCE_DIR}/cmake/AssertMissingEnvironment.cmake"
    )
endif()

install(TARGETS legacy_v1_smoke RUNTIME DESTINATION bin)
