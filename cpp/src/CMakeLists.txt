include_directories(${CMAKE_SOURCE_DIR}/cpp)
add_executable(iPerfer main.cpp)

# Add your setup code above this line, then uncomment the line below

target_link_libraries(iPerfer spdlog::spdlog cxxopts::cxxopts)
