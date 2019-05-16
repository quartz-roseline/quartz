# Install the Python Example applications
INSTALL(FILES helloworld.py DESTINATION bin COMPONENT applications)
INSTALL(FILES helloworld_app.py DESTINATION bin COMPONENT applications)
INSTALL(FILES helloworld_mqtt.py DESTINATION bin COMPONENT applications)
INSTALL(FILES mqtt_dummy_actor.py DESTINATION bin COMPONENT applications)
INSTALL(FILES mqtt_dummy_sensor.py DESTINATION bin COMPONENT applications)
INSTALL(FILES traffic_mqtt.py DESTINATION bin COMPONENT applications)

# Install Protobuf Lib for Sherlock Apps
INSTALL(FILES lib/datastream_pb2.py DESTINATION lib COMPONENT library)



