# CMakeList.txt : CMake project for UnixSock, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("UnixSock")

# TODO: Add tests and install targets if needed.

add_subdirectory("library")

# Add source to this project's executable.
add_executable (server "server.c")
target_link_libraries(server library)
add_executable (client "client.c")
target_link_libraries(client library)
