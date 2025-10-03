cmake_minimum_required(VERSION 3.23.0..4.1.1)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-D _CRT_SECURE_NO_WARNINGS)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../app/bin)

file(MAKE_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/include)

#Prohibit VS compiler from adding binaries into Debug/ Release/ directories
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY} )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

project(chess-engine VERSION 0.1 LANGUAGES CXX)

include(CTest)
enable_testing()

add_subdirectory(src/MoveGeneration)
add_subdirectory(src/MoveEvaluation)
add_subdirectory(src/Tests)
add_subdirectory(src/Visualization)

# Basic settings
set(CPACK_PACKAGE_NAME "ChessEngine")
set(CPACK_PACKAGE_VERSION "0.0.5")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Chess engine Installer")
set(CPACK_PACKAGE_VENDOR "Andlepiha")
set(CPACK_PACKAGE_CONTACT "rubcov23033@gmail.com")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "ChessEngine ${CPACK_PACKAGE_VERSION}")

# Installation prefix
set(CPACK_INSTALL_PREFIX "C:/Program Files/${CPACK_PACKAGE_NAME}")

# Optional license and readme files
#set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
#set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# Set generator (NSIS in this case)
set(CPACK_GENERATOR "NSIS")

# NSIS-specific options
set(CPACK_NSIS_DISPLAY_NAME "ChessEngine")
set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/icon.ico")
set(CPACK_NSIS_INSTALLER_ICON "${CMAKE_SOURCE_DIR}/icon.ico")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

# Include CPack at the end
include(CPack)
