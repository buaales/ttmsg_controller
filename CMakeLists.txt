project("ttmsg_controller")
cmake_minimum_required(VERSION 3.12)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_library(ptp src/ptp/master.c src/ptp/slave.c)


file(GLOB_RECURSE f ${PROJECT_SOURCE_DIR}/lib/*)
add_executable(msg_server src/server.c ${f})
add_executable(msg_app src/app/main.c ${f})
target_link_libraries(msg_server ptp)

if (UNIX AND NOT APPLE)
    target_link_libraries(msg_server rt pthread)
    target_link_libraries(msg_app rt pthread)
endif()
