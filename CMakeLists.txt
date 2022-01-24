project(AttachToProcessCLI)
add_executable(AttachToProcessCLI WIN32 main.cpp)
set_target_properties(AttachToProcessCLI PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
