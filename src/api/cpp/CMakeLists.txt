# Add some helper CMake scripts
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

FIND_PACKAGE(Threads REQUIRED)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

# Add debug symbols
ADD_DEFINITIONS(-ggdb)

# If building the micro-services QoT Stack
IF (BUILD_MICROSERVICES)
	add_definitions(-DQOT_TIMELINE_SERVICE)
	# This is for building with the NATS C Client
	IF (BUILD_NATS_CLIENT)
		ADD_DEFINITIONS(-DNATS_SERVICE)
	ENDIF (BUILD_NATS_CLIENT)

	IF (BUILD_NATS_CLIENT)
		ADD_LIBRARY(qot_core_cpp SHARED qot_coreapi.hpp qot_coreapi.cpp clkparams_circbuffer.cpp clkparams_circbuffer.hpp ../../micro-services/timeline-service/qot_timeline_service.hpp)
		TARGET_LINK_LIBRARIES(qot_core_cpp qot_timeline_serialize nats qot_clkparams_serialize ${CMAKE_THREAD_LIBS_INIT})
	ELSE ()
		ADD_LIBRARY(qot_core_cpp SHARED qot_coreapi.hpp qot_coreapi.cpp ../../micro-services/timeline-service/qot_timeline_service.hpp)
		TARGET_LINK_LIBRARIES(qot_core_cpp qot_timeline_serialize ${CMAKE_THREAD_LIBS_INIT})
	ENDIF (BUILD_NATS_CLIENT)	
ELSE ()
	ADD_LIBRARY(qot_core_cpp SHARED qot_coreapi.hpp qot_coreapi.cpp)
	TARGET_LINK_LIBRARIES(qot_core_cpp qot_timeline_serialize ${CMAKE_THREAD_LIBS_INIT})
ENDIF (BUILD_MICROSERVICES)

INSTALL(FILES qot_coreapi.hpp DESTINATION include COMPONENT headers)
INSTALL(TARGETS qot_core_cpp DESTINATION lib COMPONENT libraries)


