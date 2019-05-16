# Add the timeline service
ADD_SUBDIRECTORY(timeline-service)

# Add the synchronization service
ADD_SUBDIRECTORY(sync-service)

# Add the coordination service (install the flask service)
INSTALL(DIRECTORY coordination-service DESTINATION bin
        PATTERN "coordination-service/venv" EXCLUDE 
        PATTERN "coordination-service/.git" EXCLUDE
        PATTERN "coordination-service/coordination_service.egg-info" EXCLUDE)