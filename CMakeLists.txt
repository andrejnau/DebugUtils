project(DebugUtils)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(DebugUtilsCLI WIN32 main.cpp)
set_target_properties(DebugUtilsCLI PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")

add_executable(DebugUtilsExample WIN32 example.cpp)
set_target_properties(DebugUtilsExample PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
