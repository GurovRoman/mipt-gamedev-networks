cmake_minimum_required(VERSION 3.20)

get_filename_component(target_name ${CMAKE_CURRENT_SOURCE_DIR} NAME)

add_executable("${target_name}_client" client.cpp)
target_link_libraries("${target_name}_client" enet spdlog)

add_executable("${target_name}_server" server.cpp)
target_link_libraries("${target_name}_server" enet spdlog)

add_executable("${target_name}_lobby" lobby.cpp)
target_link_libraries("${target_name}_lobby" enet spdlog)
