# Add debug symbols
ADD_DEFINITIONS(-ggdb)

# Simple application to demonstrate how to connect to a timeline
ADD_EXECUTABLE(helloworld_core_cpp
 	helloworld.cpp
)
TARGET_LINK_LIBRARIES(helloworld_core_cpp qot_core_cpp)

# Install the cpp core test applications to the given prefix
INSTALL(
	TARGETS 
		helloworld_core_cpp
	DESTINATION 
		bin 
	COMPONENT 
		applications
)
