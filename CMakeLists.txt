cmake_minimum_required(VERSION 3.10)

project(ex1 C)

set(CMAKE_C_STANDARD 11)
#set(THREADS_PREFER_PTHREAD_FLAG ON)


#find_package(Threads)

#set(CMAKE_CXX_FLAGS "-lpthread")

add_executable(ex1
        prod-cons.c)

target_link_libraries(ex1 pthread)
target_link_libraries(ex1 m)

unset(CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES)
unset(CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES)
