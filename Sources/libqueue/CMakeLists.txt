project(LibQueue LANGUAGES C CXX)

add_library(LibQueue STATIC Src/queue.cpp Inc/queue.hpp)
target_include_directories(LibQueue PUBLIC Inc)