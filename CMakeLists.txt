# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Proj_1109)

target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "main/Cert/db611f9b28db2b68141d0945315bed82723ecb3bf02f8b45fcaf488f879169f3_certificate.pem.crt" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "main/Cert/db611f9b28db2b68141d0945315bed82723ecb3bf02f8b45fcaf488f879169f3_private.pem.key" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "main/Cert/AmazonRootCA1.pem" TEXT)