# Dockerfiles #

This directory contains different Dockerfiles and Kubernetes yaml configurations to deploy and test various stack components and applications

The Dockerfile containing a fully functional development environment is in the `dev-image` folder.

The Dockerfiles for the core Quartz services are in the following folders:

* **sync-service**: QoT Synchronization Service
* **timeline-service**: QoT Timeline Service
* **coordination-service**: QoT Coordination Service

The Dockerfiles for C++ Applications are in the following folders:

* **qot-helloworld-app**: C++ QoT Helloworld Application

The Dockerfiles for Python Applications are in the following folders:

* **python-app**: Python QoT Helloworld Application

The Dockerfiles for some extra Python Applications are found in the **extra-dockerfiles** folder:

* **python-transform**: Python QoT Helloworld transform
* **python-app-mqtt**: Python QoT Helloworld Application with Paho MQTT support
* **python-traffic-app-mqtt**: Python Traffic-Management Application with SUMO + Paho MQTT Support
* **python-mqtt-dummy-actor**: Python MQTT Dummy Sensor
* **python-mqtt-dummy-sensor**: Python MQTT Dummy Actuator

The Kubernetes Pod Specification .yaml files are in the **kubernetes-yaml** folder. It contains the following pod descriptions:

* **k8s-pod-config-python**: Python Helloworld application with QoT services packaged in a kubernets pod
* **k8s-pod-config**: C++ Helloworld application with QoT services packaged in a kubernets pod
* **nats-k8s-pod-config**: NATS Server Kubernetes pod
* **zk-k8s-pod-config**: One node Zookeeper Cluster Kubernetes pod
* **python-transform-k8s-pod-config**: Python QoT Transform pod

