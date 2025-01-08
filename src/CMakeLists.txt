cmake_minimum_required(VERSION 3.15)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Define source files
set(SRC "webrtc.cpp" "main.cpp" "http.cpp" "media.cpp")

# Add include directory for headers
include_directories("../install/include" "../deps/libpeer/src")

# Create an executable from source files
add_executable(chat ${SRC})

# Link the external library (peer)
target_link_directories(chat PRIVATE "${CMAKE_SOURCE_DIR}/install/lib" "${CMAKE_SOURCE_DIR}/build/src" "${CMAKE_SOURCE_DIR}/build/dist/lib")
target_link_libraries(chat PRIVATE peer curl z pthread asound opus crypto)

