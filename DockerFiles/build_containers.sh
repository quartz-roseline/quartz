#!/bin/sh
echo Building Test Containers using Docker
echo Building C++ Helloworld App Container
docker build -t qot-helloworld -f qot-helloworld-app/Dockerfile .

echo Building Python Helloworld App Container
docker build -t qot-python-app -f python-app/Dockerfile .

echo Building Timeline Service Container
docker build -t qot-timeline-service -f timeline-service/Dockerfile .

echo Building QoT Sync Service Container
cp sync-service/chrony.conf .
docker build -t qot-sync-service -f sync-service/Dockerfile .

echo Building Coordination Service Container
docker build -t qot-coord-service -f coordination-service/Dockerfile .
