add_executable(hub hub.cpp)

find_path(BOOST_BEAST_INCLUDE_DIRS "boost/beast.hpp")
target_include_directories(hub PRIVATE ${BOOST_BEAST_INCLUDE_DIRS})

target_include_directories(hub PRIVATE "../vendor")

target_link_libraries(hub PRIVATE core)
