cmake_minimum_required(VERSION 3.16)

project(neon_arena LANGUAGES CXX)

add_executable(neon_arena WIN32
    src/app/main.cpp
    src/game/game.cpp
    src/platform/win32_app.cpp
)

target_compile_features(neon_arena PRIVATE cxx_std_17)
target_include_directories(neon_arena PRIVATE src)
target_link_libraries(neon_arena PRIVATE gdi32)
