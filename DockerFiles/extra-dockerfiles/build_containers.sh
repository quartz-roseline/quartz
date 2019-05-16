#!/bin/sh
# Copy binaries from binaries folder
cp -r ../binaries .

# Build the containers
echo Building The Helloworld Python app with MQTT support
docker build -t qot-helloworld-mqtt -f python-app-mqtt/Dockerfile .

echo Building Python Dummy MQTT Actuator
docker build -t dummy-mqtt-actor -f python-mqtt-dummy-actor/Dockerfile .

echo Building Python Dummy MQTT Sensor
docker build -t dummy-mqtt-sensor -f python-mqtt-dummy-sensor/Dockerfile .

echo Building the Python Traffic App
docker build -t qot-traffic-mqtt -f python-traffic-app-mqtt/Dockerfile .

echo Building the Python QoT Transform
docker build -t qot-python-transform -f python-transform/Dockerfile .