cmake_minimum_required(VERSION 3.13.1)

if(NOT TARGET app)
     message(FATAL_ERROR "Please create a target named 'app' (ex: add_executable(app)) before adding this file")
endif()

target_include_directories(app PRIVATE .)

# add all sources to the project
RECURSIVE_FIND_FILE_APPEND(MODEL_SOURCE "." "*.cpp")
RECURSIVE_FIND_FILE_APPEND(MODEL_C_SOURCE "." "*.c")
target_sources(app PRIVATE ${MODEL_SOURCE} ${MODEL_C_SOURCE})
