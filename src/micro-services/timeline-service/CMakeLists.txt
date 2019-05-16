# Add some helper CMake scripts
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

#################################################################################################

# We need to change a few things to enable compilation
IF (X86_64)
	SET(Boost_USE_STATIC_LIBS        OFF)
	SET(Boost_USE_STATIC_RUNTIME     OFF)
	SET(Boost_USE_MULTITHREADED      ON)
	SET(Boost_ALL_DYN_LINK           ON)
	SET(Boost_NO_SYSTEM_PATHS        ON)
	SET(BOOST_ROOT       "${CROSS_COMPILE_ROOTFS}/usr")
	SET(BOOST_INCLUDEDIR "${CROSS_COMPILE_ROOTFS}/usr/include")
ENDIF (X86_64)
IF (CROSS_RPI)
	SET(Boost_USE_STATIC_LIBS        OFF)
	SET(Boost_USE_STATIC_RUNTIME     OFF)
	SET(Boost_USE_MULTITHREADED      ON)
	SET(Boost_ALL_DYN_LINK           ON)
	SET(Boost_NO_SYSTEM_PATHS        ON)
	SET(BOOST_ROOT       "${CROSS_COMPILE_ROOTFS}/usr")
	SET(BOOST_INCLUDEDIR "${CROSS_COMPILE_ROOTFS}/usr/include")
ENDIF (CROSS_RPI)
IF (CROSS_BBB)
	SET(Boost_USE_STATIC_LIBS        OFF)
	SET(Boost_USE_STATIC_RUNTIME     OFF)
	SET(Boost_USE_MULTITHREADED      ON)
	SET(Boost_ALL_DYN_LINK           ON)
	SET(Boost_NO_SYSTEM_PATHS        ON)
	SET(BOOST_ROOT       "${CROSS_COMPILE_ROOTFS}/usr")
	SET(BOOST_INCLUDEDIR "${CROSS_COMPILE_ROOTFS}/usr/include")
	SET(BOOST_LIBRARYDIR "${CROSS_COMPILE_ROOTFS}/usr/lib")
ENDIF (CROSS_BBB)

# This is required for boost::log
ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)

#debug symbols -sean
ADD_DEFINITIONS(-ggdb)

# Preprocessor directive required to link boost trivial logging
FIND_PACKAGE(Boost REQUIRED
	COMPONENTS thread system program_options log date_time)
FIND_PACKAGE(Threads REQUIRED)

# Set Variables for C++ Rest SDK
set(CPPRESTSDK_INCLUDE_DIR "${CROSS_COMPILE_ROOTFS}/usr/include/cpprest")

# Find the C++ Rest SDK Library
IF (CROSS_BBB)
	find_library(CPPREST_LIB cpprest HINTS "${CROSS_COMPILE_ROOTFS}/usr/lib")
ELSE ()
	find_library(CPPREST_LIB cpprest)
ENDIF(CROSS_BBB)

# Find OpenSSL
IF (CROSS_RPI)
	SET(OPENSSL_ROOT_DIR "${CROSS_COMPILE_ROOTFS}/usr/lib/openssl-1.0.0")
ENDIF (CROSS_RPI)
IF (CROSS_BBB)
	SET(OPENSSL_ROOT_DIR "${CROSS_COMPILE_ROOTFS}/usr/lib")
ENDIF (CROSS_BBB)
find_package(OpenSSL REQUIRED)

# Location of header files for the entire project
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR} ${CPPRESTSDK_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})

# Location of boost libraries
IF (X86_64)
	LINK_DIRECTORIES(
		"${CROSS_COMPILE_ROOTFS}/usr/lib/x86_64-linux-gnu"
		"${Boost_LIBRARY_DIRS}"
	)
ENDIF (X86_64)
IF (CROSS_RPI)
	LINK_DIRECTORIES(
		"${CROSS_COMPILE_ROOTFS}/usr/lib"
		"${Boost_LIBRARY_DIRS}"
	)
ENDIF (CROSS_RPI)
IF (CROSS_BBB)
	LINK_DIRECTORIES(
		"${CROSS_COMPILE_ROOTFS}/usr/lib"
		"${Boost_LIBRARY_DIRS}"
	)
ENDIF (CROSS_BBB)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
##### Timeline Book-keeping Library #####
ADD_LIBRARY(qot_timeline SHARED 
	       qot_timeline_clock.cpp 
	       qot_timeline_clock.hpp
	       qot_timeline_registry.cpp
	       qot_timeline_registry.hpp
	       qot_synccomm.cpp
	       qot_synccomm.hpp
	       qot_timeline.cpp
	       qot_timeline.hpp
	       qot_tlmsg_serialize.cpp
	       qot_tlmsg_serialize.hpp
	       qot_timeline_rest.cpp
	       qot_timeline_rest.hpp
	       qot_timeline_subscriber.cpp
	       qot_timeline_subscriber.hpp)
TARGET_LINK_LIBRARIES(qot_timeline nats qot_syncmsg_serialize ${CPPREST_LIB} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} rt)
INSTALL(TARGETS qot_timeline DESTINATION lib COMPONENT libraries)

##### Timeline Message Serialization #####
ADD_LIBRARY(qot_timeline_serialize SHARED 
	       qot_tlmsg_serialize.cpp
	       qot_tlmsg_serialize.hpp)
TARGET_LINK_LIBRARIES(qot_timeline_serialize rt)
INSTALL(TARGETS qot_timeline_serialize DESTINATION lib COMPONENT libraries)

#########################################################################################################
# QoT Timeline Service
ADD_EXECUTABLE(qot_timeline_service
			   qot_timeline_service.cpp
		       qot_timeline_service.hpp)
TARGET_LINK_LIBRARIES(qot_timeline_service qot_timeline qot_timeline_serialize qot_syncmsg_serialize ${CPPREST_LIB} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} rt)
INSTALL(TARGETS qot_timeline_service DESTINATION bin COMPONENT applications)

