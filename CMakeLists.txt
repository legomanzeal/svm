cmake_minimum_required(VERSION 2.8.12)
macro(print_all_variables)
    message(STATUS "print_all_variables------------------------------------------{")
    get_cmake_property(_variableNames VARIABLES)
    foreach (_variableName ${_variableNames})
        message(STATUS "${_variableName}=${${_variableName}}")
    endforeach()
    message(STATUS "print_all_variables------------------------------------------}")
endmacro()
# log the variable passed for easier diagnosis


# project name and <language-name>
project(svm CXX)

set(CMAKE_CXX_STANDARD 14)

find_package(OpenMP)
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

add_library(svm STATIC src/svm.cpp)
target_include_directories(svm PUBLIC include)

option(SVM__ENABLE_TESTS "If tests should be compiled" ON)
if(SVM__ENABLE_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()


# Override the default pdb name to be the same as the library name.
# Static libraries have no linker PDBs, thus the compiler PDBs are relevant. use COMPILE_PDB_NAME and COMPILE_PDB_OUTPUT_DIRECTORY

set_target_properties(svm PROPERTIES
                    COMPILE_PDB_NAME "svm${PDB_POSTFIX}"
                    COMPILE_PDB_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
                    )


get_target_property(propCOMPILE_PDB_NAME svm COMPILE_PDB_NAME)
get_target_property(propCOMPILE_PDB_OUTPUT_DIRECTORY svm COMPILE_PDB_OUTPUT_DIRECTORY)

set(_pdb_path "${propCOMPILE_PDB_OUTPUT_DIRECTORY}/${CMAKE_CONFIGURATION_TYPES}/${propCOMPILE_PDB_NAME}.pdb")

# debug pdb path used:
# message ("_pdb_path = ${_pdb_path}")

# we want to differentiate different output paths for x86 and x64
# we only use vs generators (for the time being)
# old way of detecting platform fails: 'if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")' -> Visual Studio 2019's generator has no bitness suffix (and defaults to 64 bit) unless the user manually specifies a "generator target platform" in CMake
# https://stackoverflow.com/questions/39258250/how-to-detect-if-64-bit-msvc-with-cmake

# install static libs and pdbs      
if("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "x64")
    # x64
    message ("installing to x64 outputs due to CMAKE_GENERATOR_PLATFORM == 'x64'")
    install(TARGETS svm PUBLIC_HEADER DESTINATION lib/x64 ARCHIVE DESTINATION lib/x64)
    install(FILES "${_pdb_path}" DESTINATION lib/x64)
elseif("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")
    # x86
    message ("installing to x86 outputs due to CMAKE_GENERATOR_PLATFORM == 'Win32'")
    install(TARGETS svm PUBLIC_HEADER DESTINATION lib ARCHIVE DESTINATION lib)
    install(FILES "${_pdb_path}" DESTINATION lib)
else() 
    message ("installing unknown outputs due to CMAKE_GENERATOR_PLATFORM == ${CMAKE_GENERATOR_PLATFORM} which is not handled")
    install(TARGETS svm PUBLIC_HEADER DESTINATION lib ARCHIVE DESTINATION lib)
    install(FILES "${_pdb_path}" DESTINATION lib)
endif()

# install headers
install(DIRECTORY include/svm DESTINATION svm/include)
install(DIRECTORY include/combinatorics DESTINATION svm/include)
