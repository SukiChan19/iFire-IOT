# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "AmazonRootCA1.pem.S"
  "Proj_1109.bin"
  "Proj_1109.map"
  "bootloader\\bootloader.bin"
  "bootloader\\bootloader.elf"
  "bootloader\\bootloader.map"
  "config\\sdkconfig.cmake"
  "config\\sdkconfig.h"
  "db611f9b28db2b68141d0945315bed82723ecb3bf02f8b45fcaf488f879169f3_certificate.pem.crt.S"
  "db611f9b28db2b68141d0945315bed82723ecb3bf02f8b45fcaf488f879169f3_private.pem.key.S"
  "esp-idf\\esptool_py\\flasher_args.json.in"
  "esp-idf\\mbedtls\\x509_crt_bundle"
  "flash_app_args"
  "flash_bootloader_args"
  "flash_project_args"
  "flasher_args.json"
  "ldgen_libraries"
  "ldgen_libraries.in"
  "project_elf_src_esp32c3.c"
  "x509_crt_bundle.S"
  )
endif()
