cmake_minimum_required (VERSION 3.21)

# Maps to Visual Studio solution file (Tutorial.sln)
# The solution will have all targets (exe, lib, dll) 
# as Visual Studio projects (.vcproj)
PROJECT(BADChIMP LANGUAGES CXX)
SET(CXX_STD 17) 

set(LB_MAIN "" CACHE STRING "Choose the example to be built, if any")
#SET(LBMAIN rans)
#SET(LBMAIN std_one_phase)
#SET(LBMAIN laplace_fluid)
#SET(LBMAIN laplace_pressure)
#SET(LBMAIN six_dof)
#SET(LBMAIN co2)
#SET(LBMAIN std_case)

SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin )
# Turn on the ability to create folders to organize projects (.vcproj)
# It creates "CMakePredefinedTargets" folder by default and adds CMake
# defined projects like INSTALL.vcproj and ZERO_CHECK.vcproj
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#MPI STUFF
IF(WIN32)
if("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
        set(MPI_MSMPI_LIB_PATH_ENV_NATIVE "$ENV{MSMPI_LIB64}")
        file(TO_CMAKE_PATH "${MPI_MSMPI_LIB_PATH_ENV_NATIVE}" MPI_MSMPI_LIB_PATH)

        # Microsoft MPI might use backslashes in the environment variables,
        # so it's important to convert to CMake-standard forward slashes
        # before appending a subdirectory with a forward slash.
        set(MPI_MSMPI_INC_PATH_ENV_NATIVE "$ENV{MSMPI_INC}")
        file(TO_CMAKE_PATH "${MPI_MSMPI_INC_PATH_ENV_NATIVE}" MPI_MSMPI_INC_PATH_ENV)
        set(MPI_MSMPI_INC_PATH_EXTRA "${MPI_MSMPI_INC_PATH_ENV}/x64")
      else()
        set(MPI_MSMPI_LIB_PATH "$ENV{MSMPI_LIB32}")

        set(MPI_MSMPI_INC_PATH_ENV_NATIVE "$ENV{MSMPI_INC}")
        file(TO_CMAKE_PATH "${MPI_MSMPI_INC_PATH_ENV_NATIVE}" MPI_MSMPI_INC_PATH_ENV)
        set(MPI_MSMPI_INC_PATH_EXTRA "${MPI_MSMPI_INC_PATH_ENV}/x86")
      endif()
ENDIF(WIN32)

find_package(MPI REQUIRED)
IF(MPI_CXX_FOUND)
        INCLUDE_DIRECTORIES(${MPI_CXX_INCLUDE_PATH})
        LIST(APPEND SCR_EXTERNAL_LIBS ${MPI_CXX_LIBRARIES})
ENDIF(MPI_CXX_FOUND)

# Try to find Eigen3
find_package(Eigen3)
if(NOT EIGEN3_FOUND)
	message(CHECK_START "Fetching external Eigen3")
	include(FetchContent)
	FetchContent_Declare(
		eigen3
		URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
		URL_HASH SHA256=8586084f71f9bde545ee7fa6d00288b264a2b7ac3607b974e54d13e7162c1c72
	)
	FetchContent_MakeAvailable(eigen3)
	message(CHECK_PASS "Done")
endif()

if(BUILD_TESTS)
	# Try to find gtest
	find_package(GTest 1.12)
	if(NOT GTest_FOUND)
		message(CHECK_START "Fetching external GTest")
		include(FetchContent)
		FetchContent_Declare(
			googletest
			URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.tar.gz
			URL_HASH MD5=e8219374acdfda3f425331028eb4e2a
		)
		FetchContent_MakeAvailable(googletest)
		message(CHECK_PASS "Done")
	endif()
endif()

#if ( CMAKE_COMPILER_IS_GNUCC )
    # This is (not) tested
    # set_property( TARGET LBSOLVER APPEND_STRING PROPERTY COMPILE_FLAGS
    # -Wwrite-strings )
    #  add_definitions("-pg")
#endif ( CMAKE_COMPILER_IS_GNUCC )
#######################################################
## For debugging and memory checking on Linux        ##
## run with >valgrind --leak-check=full <executable> ##
#######################################################
IF(WIN32)
# Set compiler flags and options. 
# Here it is setting the Visual Studio warning level to 4
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 ")
ELSEIF(UNIX AND NOT APPLE)
  IF(CMAKE_BUILD_TYPE MATCHES Debug)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -pg -std=c++${CXX_STD} -Wall")
  ELSE()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++${CXX_STD} -Wall")
  ENDIF(CMAKE_BUILD_TYPE MATCHES Debug)
  #SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -pg -std=c++11")
  #SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O3 -std=c++${CXX_STD}")
  #SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -O3 -std=c++${CXX_STD}")
ELSE()
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++${CXX_STD} -Wall")
ENDIF(WIN32)



# Command to output information to the console
# Useful for displaying errors, warnings, and debugging

message ("cxx Flags: " ${CMAKE_CXX_FLAGS})

# Sub-directories where more CMakeLists.txt exist
add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(PythonScripts)

message("BUILD:" ${CMAKE_BUILD_TYPE})
