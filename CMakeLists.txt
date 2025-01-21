cmake_minimum_required(VERSION 3.7)
project(Mon20Assignment)

set(CMAKE_CXX_STANDARD 20)
set(SDL2_ttf_DIR "/home/s5614562/CLionProjects/Mon20Assignment/SDL2/lib64/cmake/SDL2_ttf")

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS} ${SDL_TTF_INCLUDE_DIR})
add_executable(Mon20Assignment main.cpp)
target_link_libraries(Mon20Assignment SDL2::SDL2 SDL2_image::SDL2_image-static SDL2_ttf::SDL2_ttf-static)
