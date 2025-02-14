cmake_minimum_required(VERSION 3.15)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Define source files
set(SRC "Conversation.cpp" "WebRTCClient.cpp" "main.cpp" "http.cpp" "media.cpp" "WebSocketClient.cpp" "Utils.cpp" "ubus.cpp" "RealTimeClient.cpp")

# Add the flag for all targets
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi")

# Create an executable from source files
add_executable(chat ${SRC})

# Detect architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    message(STATUS "Detected ARM architecture.")
    target_link_directories(chat PRIVATE "${CMAKE_SOURCE_DIR}/install/lib" "${CMAKE_SOURCE_DIR}/build/src" "${CMAKE_SOURCE_DIR}/build/dist/lib")
    target_link_libraries(chat PRIVATE peer curl z pthread asound opus crypto ubus ubox blobmsg_json json-c websockets)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    message(STATUS "Detected x86 architecture.")
    find_package(nlohmann_json  REQUIRED)
    target_link_directories(chat PRIVATE "${CMAKE_SOURCE_DIR}/build/src" "${CMAKE_SOURCE_DIR}/build/dist/lib")
    target_link_libraries(chat PRIVATE nlohmann_json::nlohmann_json peer curl asound opus)
else()
    message(WARNING "Unknown architecture: ${CMAKE_SYSTEM_PROCESSOR}. Defaulting to generic configuration.")
endif()

# Add include directory for headers
include_directories("../install/include" "../deps/libpeer/src")
