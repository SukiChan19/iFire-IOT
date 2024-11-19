# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/Espressif/frameworl/v5.3/esp-idf/components/bootloader/subproject"
  "D:/EspExamp/A_Project_test/Proj_1109/build/bootloader"
  "D:/EspExamp/A_Project_test/Proj_1109/build/bootloader-prefix"
  "D:/EspExamp/A_Project_test/Proj_1109/build/bootloader-prefix/tmp"
  "D:/EspExamp/A_Project_test/Proj_1109/build/bootloader-prefix/src/bootloader-stamp"
  "D:/EspExamp/A_Project_test/Proj_1109/build/bootloader-prefix/src"
  "D:/EspExamp/A_Project_test/Proj_1109/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/EspExamp/A_Project_test/Proj_1109/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/EspExamp/A_Project_test/Proj_1109/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
