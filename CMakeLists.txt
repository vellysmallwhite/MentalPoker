cmake_minimum_required(VERSION 3.10)

# Define the project name
project(MentalPoker)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(jsoncpp REQUIRED)

# Add executable
add_executable(MentalPoker 
    main.cpp
    src/network/NetworkManager.cpp
    src/application/MembershipList.cpp
)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/network
    ${CMAKE_SOURCE_DIR}/src/application
)

# Link libraries
target_link_libraries(MentalPoker jsoncpp)