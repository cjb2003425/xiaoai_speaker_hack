cmake_minimum_required(VERSION 3.19)

project(chat)

# Add source directories
set(SRC_DIRS
  "deps/libpeer"
  "src"
)

# Disable building of examples
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/deps/libpeer/CMakeLists.txt INPUT_CONTENT)
string(REPLACE "add_subdirectory(examples)" "" MODIFIED_CONTENT ${INPUT_CONTENT})
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/deps/libpeer/CMakeLists.txt ${MODIFIED_CONTENT})

# Add each source directory to the build
foreach(DIR ${SRC_DIRS})
    add_subdirectory(${DIR})
endforeach()

