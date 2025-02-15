cmake_minimum_required(VERSION 3.15)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Define source files
set(SRC "Conversation.cpp" "WebRTCClient.cpp" "main.cpp" "http.cpp" "media.cpp" "WebSocketClient.cpp" "Utils.cpp" "RealTimeClient.cpp")

set(LIBS "peer" "curl" "z" "pthread" "asound" "opus" "crypto" "websockets")

# Add the flag for all targets
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi")

# Create an executable from source files
add_executable(chat ${SRC})

target_link_libraries(chat PRIVATE ${LIBS})
# Detect architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    message(STATUS "Detected ARM architecture.")
    list(APPEND SRC "ubus.cpp")
    list(APPEND LIBS "ubus" "blobmsg_json")
    target_link_directories(chat PRIVATE "${CMAKE_SOURCE_DIR}/install/lib" "${CMAKE_SOURCE_DIR}/build/src" "${CMAKE_SOURCE_DIR}/build/dist/lib")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    message(STATUS "Detected x86 architecture.")
    find_package(nlohmann_json  REQUIRED)
    target_link_directories(chat PRIVATE "${CMAKE_SOURCE_DIR}/build/src" "${CMAKE_SOURCE_DIR}/build/dist/lib")
else()
    message(WARNING "Unknown architecture: ${CMAKE_SYSTEM_PROCESSOR}. Defaulting to generic configuration.")
endif()

# Add include directory for headers
include_directories("../install/include" "../deps/libpeer/src")
