# @file CMakeLists.txt
# @brief Base CMake build script for the qot-stack project
# @author Anon D'Anon
# 
# Copyright (c) Anon 2018.
# Copyright (c) Anon Inc. 2018.
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 	1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

# Locate Google Test framework
#FIND_PACKAGE(GTest REQUIRED)

# Everything in our system must know about qot_types.h
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

# Build the programmer interface
OPTION(BUILD_PROGAPI "Build programmer interface" ON)
IF (BUILD_PROGAPI)
	ADD_SUBDIRECTORY(api)
ENDIF (BUILD_PROGAPI)

# Build example applications
OPTION(BUILD_EXAMPLE "Build example applications" ON)
IF (BUILD_EXAMPLE)
	ADD_SUBDIRECTORY(examples)
ENDIF (BUILD_EXAMPLE)

# Build the Sync Service and the API with NATS support
OPTION(BUILD_NATS_CLIENT "Build Sync Service/APIs with NATS support" OFF)

# Build the Sync Service for Priveleged Mode Operation (required for CLOCK_REALTIME discipline and hardware timestamping)
OPTION(BUILD_SYNC_PRIVELEGED "Build Sync Service for priveleged mode operation" ON)

# Build the Microservice
OPTION(BUILD_MICROSERVICES "Build Microservice" ON)
IF (BUILD_MICROSERVICES)
	# Run a script to configure the CMake files for building Chrony NTP Service
	execute_process(COMMAND ./configure --host-os=${CMAKE_SYSTEM_NAME} --host-machine=${CMAKE_SYSTEM_PROCESSOR} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/micro-services/sync-service/sync/ntp/chrony-3.2/ OUTPUT_FILE chronyconf_out.txt)
	
	# Override the CMakeLists.txt for the sync service and the config.h for chrony if the target is the BBB
	IF (CROSS_BBB)
		execute_process(COMMAND cp configBBB.h config.h WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/micro-services/sync-service/sync/ntp/chrony-3.2/ OUTPUT_FILE chronybbbconf_out.txt)
		execute_process(COMMAND cp CMakeListsBBB.txt CMakeLists.txt WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/micro-services/sync-service/ OUTPUT_FILE chronybbbcmake_out.txt)
	ENDIF (CROSS_BBB)
	IF (CROSS_RPI)
		execute_process(COMMAND cp configRPI.h config.h WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/micro-services/sync-service/sync/ntp/chrony-3.2/ OUTPUT_FILE chronyrpiconf_out.txt)
		execute_process(COMMAND cp CMakeListsRPI.txt CMakeLists.txt WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/micro-services/sync-service/ OUTPUT_FILE chronyrpicmake_out.txt)
	ENDIF (CROSS_RPI)
	ADD_SUBDIRECTORY(micro-services)
ENDIF (BUILD_MICROSERVICES)



