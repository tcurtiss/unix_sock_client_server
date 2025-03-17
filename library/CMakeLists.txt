add_library(library SHARED "library.c" "library.h")
target_include_directories(library
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)