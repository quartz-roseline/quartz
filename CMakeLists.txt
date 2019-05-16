# Minimum version of CMake required to build this file
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

# Name and version of this project
PROJECT(qot-stack)

# Build for an x86-64 platform
OPTION(X86_64 "Build for x86_64" OFF)
IF (X86_64)
	SET(CROSS_COMPILE_ROOTFS "")
	SET(CMAKE_SYSTEM_NAME Linux)
	SET(CMAKE_SYSTEM_PROCESSOR x86_64)
	SET(CMAKE_C_COMPILER gcc)
	SET(CMAKE_CXX_COMPILER g++)
	LINK_DIRECTORIES(
		"/usr/lib"
	)
	ENABLE_TESTING()
	# Find the root path
    SET(CMAKE_FIND_ROOT_PATH ${CROSS_COMPILE_ROOTFS})
ENDIF (X86_64)

# Cross-compile for the Raspberry Pi 3
OPTION(CROSS_RPI "Cross compile for Raspberry Pi" OFF)
IF (CROSS_RPI)
	SET(CROSS_COMPILE_ROOTFS "/rpi-newer-crosstools/x64-gcc-6.3.1/arm-rpi-linux-gnueabihf/arm-rpi-linux-gnueabihf/sysroot")	
	SET(CMAKE_SYSTEM_NAME Linux)
	SET(CMAKE_SYSTEM_PROCESSOR armv7l)
	SET(CMAKE_C_COMPILER   /rpi-newer-crosstools/x64-gcc-6.3.1/arm-rpi-linux-gnueabihf/bin/arm-rpi-linux-gnueabihf-gcc)
	SET(CMAKE_CXX_COMPILER /rpi-newer-crosstools/x64-gcc-6.3.1/arm-rpi-linux-gnueabihf/bin/arm-rpi-linux-gnueabihf-g++)
	LINK_DIRECTORIES(
		"${CROSS_COMPILE_ROOTFS}/usr/lib"
	)
	# Find the root path
    SET(CMAKE_FIND_ROOT_PATH ${CROSS_COMPILE_ROOTFS})
ENDIF (CROSS_RPI)

# Cross-compile for the Beaglebone Black
OPTION(CROSS_BBB "Cross compile for Beaglebone Black" OFF)
IF (CROSS_BBB)
	SET(CROSS_COMPILE_ROOTFS /opt/qot-stack/DockerFiles/dev-image/bbb-rootfs/gcc-linaro-5.4.1-2017.01-x86_64_arm-linux-gnueabihf/arm-linux-gnueabihf/libc)
	SET(CROSS_PREFIX /opt/qot-stack/DockerFiles/dev-image/bbb-rootfs/gcc-linaro-5.4.1-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-)
	SET(tools /opt/qot-stack/DockerFiles/dev-image/bbb-rootfs/gcc-linaro-5.4.1-2017.01-x86_64_arm-linux-gnueabihf)
	SET(CMAKE_SYSTEM_NAME Linux)
	SET(CMAKE_SYSTEM_PROCESSOR armv7l)
	SET(CMAKE_C_COMPILER ${CROSS_PREFIX}gcc)
	SET(CMAKE_CXX_COMPILER ${CROSS_PREFIX}g++)
	LINK_DIRECTORIES(
		"${CROSS_COMPILE_ROOTFS}/usr/lib"
	)
	# Find the root path
    SET(CMAKE_FIND_ROOT_PATH ${CROSS_COMPILE_ROOTFS})
ENDIF (CROSS_BBB)

# Build for a containerized deployment (if disabled installed to local /usr/local directory)
OPTION(CONTAINER_BUILD "Build for Linux Containers" ON)
IF (CONTAINER_BUILD)
	IF (CROSS_RPI)
		# Run a script to create the directories to dump the binaries
		execute_process(COMMAND mkdir rpi-binaries WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles OUTPUT_QUIET ERROR_QUIET)
		execute_process(COMMAND mkdir bin WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles/rpi-binaries OUTPUT_QUIET ERROR_QUIET)
		execute_process(COMMAND mkdir lib WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles/rpi-binaries OUTPUT_QUIET ERROR_QUIET) 
		execute_process(COMMAND mkdir include WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles/rpi-binaries OUTPUT_QUIET ERROR_QUIET)
		execute_process(COMMAND cp ${CROSS_COMPILE_ROOTFS}/usr/lib/libnats.so ${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles/rpi-binaries/lib/ WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_QUIET ERROR_QUIET)
		SET(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles/rpi-binaries")
	ELSEIF (CROSS_BBB)
		# Run a script to create the directories to dump the binaries
		execute_process(COMMAND mkdir bbb-binaries WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles OUTPUT_QUIET ERROR_QUIET)
		execute_process(COMMAND mkdir bin WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles/bbb-binaries OUTPUT_QUIET ERROR_QUIET)
		execute_process(COMMAND mkdir lib WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles/bbb-binaries OUTPUT_QUIET ERROR_QUIET) 
		execute_process(COMMAND mkdir include WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles/bbb-binaries OUTPUT_QUIET ERROR_QUIET)
		execute_process(COMMAND cp ${CROSS_COMPILE_ROOTFS}/usr/lib/libnats.so ${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles/bbb-binaries/lib/ WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_QUIET ERROR_QUIET)
		SET(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles/bbb-binaries")
	ELSE()
	    # Run a script to create the directories to dump the binaries
		execute_process(COMMAND mkdir binaries WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles OUTPUT_QUIET ERROR_QUIET)
		execute_process(COMMAND mkdir bin WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles/binaries OUTPUT_QUIET ERROR_QUIET)
		execute_process(COMMAND mkdir lib WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles/binaries OUTPUT_QUIET ERROR_QUIET) 
		execute_process(COMMAND mkdir include WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles/binaries OUTPUT_QUIET ERROR_QUIET)
		execute_process(COMMAND cp /usr/local/lib/libnats.so ${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles/binaries/lib/ WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_QUIET ERROR_QUIET)
		SET(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/DockerFiles/binaries")
	ENDIF (CROSS_RPI)
ELSE ()
	SET(CMAKE_INSTALL_PREFIX "${CROSS_COMPILE_ROOTFS}/usr/local")
ENDIF (CONTAINER_BUILD)

# Use our definitions for compiler tools
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers in the target directories only
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Platform-wide C and C++ options
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")

# Uninstall target
CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)
ADD_CUSTOM_TARGET(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

# All source code
ADD_SUBDIRECTORY(src)
