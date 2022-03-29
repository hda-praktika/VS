cmake_minimum_required(VERSION 3.20)

project(VerteilteSysteme VERSION 1.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

add_compile_options(-fcoroutines-ts)
add_compile_options(-fdiagnostics-show-template-tree)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi -lpthread")
set(CMAKE_CXX_FLAGS "-fcoroutines-ts --stdlib=libc++ -Xclang -fconcepts-ts")

add_subdirectory(core)
add_subdirectory(hub)
add_subdirectory(prosumer)
