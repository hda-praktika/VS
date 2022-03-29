add_library(core STATIC src/core.cpp)
target_include_directories(core PUBLIC include/)

find_path(BOOST_ASIO_INCLUDE_DIRS "boost/asio.hpp")
target_include_directories(core PRIVATE ${BOOST_ASIO_INCLUDE_DIRS})

# Boost.Asio hat einen Bug mit Concepts: https://github.com/boostorg/asio/issues/312
target_compile_definitions(core PUBLIC -DBOOST_ASIO_DISABLE_CONCEPTS)

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(core PRIVATE nlohmann_json nlohmann_json::nlohmann_json)
