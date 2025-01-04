cmake_minimum_required(VERSION 3.5)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Define source files
set(SRC "webrtc.cpp" "main.cpp" "http.cpp")

# Add include directory for headers
include_directories("../install/include")

# Specify the library directory where libpeer.so is located
link_directories("${CMAKE_SOURCE_DIR}/install/lib")

# Create an executable from source files
add_executable(chat ${SRC})

# Link the external library (peer)
target_link_libraries(chat peer)
target_link_libraries(chat curl)
target_link_libraries(chat z)

