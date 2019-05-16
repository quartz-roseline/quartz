# Quartz: Time-as-a-Service for Containerized Applications #

# Overview #
Having a shared sense of time is critical to distributed cyber-physical systems. Although time synchronization is a mature field of study, the manner in which time-based applications interact with time has remained largely the same --synchronization runs as a best-effort background service that consumes resources independently of application demand, and applications are not aware of how accurate their current estimate of time actually is.

Motivated by Cyber-Physical Systems (CPS), in which application requirements need to be balanced with system resources, we advocate for a new way of thinking about how applications interact with time. We introduce the new notion of Quality of Time (QoT) as the, *"end-to-end uncertainty in the notion of time delivered to an application by the system"*. We propose that the notion of Quality of Time (QoT) be both *observable* and *controllable*. A consequence of this being that time synchronization must now satisfy timing demands across a network, while minimizing the system resources, such as energy or bandwidth.

Quartz realizes this vision, by providing Time-as-a-Service (TaaS). Quartz takes in application-specific QoT requirements, and orchestrates the underlying infrastructure to meet them, while exposing the delivered QoT back to the application. Our stack features a rich application programming interface (API) that is centered around the notion of a *timeline* -- a virtual sense of time to which applications bind with their desired *accuracy interval* and *minimum resolution* timing requirements. This enables developers to easily write choreographed applications that are capable of requesting and observing local time uncertainty.

Quartz works by constructing a timing subsystem in Linux that runs parallel to timekeeping and POSIX timers. In this stack quality metrics are passed alongside time calls. Quartz features a containerized micro-service-based implementation which builds upon existing open-source software, such as NTP (chrony), PTP (linuxptp), Apache Zookeeper and NATS to provide a limited subset of features to illustrate the usefulness of our Time-as-a-Service


# Table of contents #

[TOC]

# Setup #

This section describes how to install a containerized development environment for Quartz. We assume that you have an OS which supports Docker. To learn more about Docker go to https://www.docker.com/. As of the time of when this Wiki was written, Docker supports Linux, Mac OS and Microsoft Windows. However, the instructions have been tested on Ubuntu 16.04 and Mac OS High Sierra. Based on your OS, instructions to install Docker can be found at https://docs.docker.com/install/

Once Docker is installed, the Ubuntu 16.04-based Quartz development image with all the required dependencies can be built and deployed using the following instructions. 

```
# Clone the Quartz Stack repo
$> git clone https://sandeepdAnon93@bitbucket.org/sandeepdAnon93/quartz.git
# Navigate to the dev-image directory
$> cd quartz # Or the root of the repo
$> cd Dockerfiles/dev-image
# Building the Dev Image
$> docker build -t qot-devimage .
# Deploy the Built image as a container
$> REPO_PATH=/absolute-path-to-quartz # Replace with the absolute path to the repo root directory
$> docker run -ti --rm -v $REPO_PATH:/opt/qot-stack:z qot-devimage
```

You should now have a shell to the Docker development container. Note that, the  `docker run` command mounts the Quartz repo into the container at the path `/opt/qot-stack`, which allows the stack components to be built, deployed and tested inside the container. 

# Build and Install Instructions #

This section provides the build and install instructions to build and install Quartz. All the instructions below must be carried out inside the development container. 

## STEP 1 : Initialialize the sub modules ##
Intialize the third-party code (CNATS etc.)

```
$> cd /opt/qot-stack # Navigate to the root directory of the repo in the container.
$> git submodule init
$> git submodule update
```

## STEP 2 : Install the NATS C/C++ Client ##
```
$> cd thirdparty/cnats
# Create the build directory
$> mkdir build
$> cd build
# Configure the CMake Build system
$> cmake .. 
# Install the built libraries
$> make install
# Navigate back to the repo home directory
$> cd ../../..
```

## STEP 3 : Build configuration and install ##
The entire project is cmake-driven, and so the following should suffice:
```
$> mkdir -p build % Do this in the top most project directory /qot-stack %
$> pushd build
$> ccmake ..
```
You should now see a cmake screen in your terminal with multiple configuration options.

**Component Selection**: These options specify which components of the stack must be built and installed, and must be set to `ON` to be built/installed. 
```
1. BUILD_MICROSERVICES              - Build the Microservices (Timeline, ClockSync and Coordination)
2. BUILD_PROGAPI                    - Build the API Libraries
3. BUILD_EXAMPLE                    - Build the examples
4. BUILD_CPP_API                    - Build the C++ API Library   
5. BUILD_CPP_EXAMPLE                - Build the C++ examples (C++ API lib must also be built)  
6. INSTALL_PYTHON_API               - Install the Python 3 API
7. INSTALL_PYTHON_EXAMPLE           - Install the Python 3 Examples (Python API must also be installed)
```

To build the basic demo scenario, set all the above options to `ON`.

**Extra Options**: These options specify some extra building/installation configuration parameters. Set to `ON` to enable. 
```
1. BUILD_NATS_CLIENT                - Build the API/Services with NATS Client Support (reccomended ON)
2. BUILD_SYNC_PRIVELEGED            - Build the clock-sync service to use features enabled by root priveleges
3. CONTAINER_BUILD                  - Option to install scripts to a directory, from where scripts can build micro-service containers
```

To build the basic demo scenario, set **BUILD_NATS_CLIENT** and **CONTAINER_BUILD** to `ON`, and **BUILD_SYNC_PRIVELEGED** to `OFF`.

**Note**: Setting **CONTAINER_BUILD** to `OFF` sets the installation prefix for the built binaries, libraries and API headers to `/usr/` (`/usr/bin`, `/usr/lib`, `/usr/include`). To know more about building the clock sync service using **BUILD_SYNC_PRIVELEGED**=`ON`, please visit the [Priveleged Sync Service](#priveleged-sync-service) section

**Platform Type**: These options specify the compilation type based on platform. Currently native compilation is supported, as well as cross-compilation for the Raspberry Pi 3 and Beaglebone Black platforms. Note that, only one of the below options can be set to 'ON'.

```
1. X86_64                          - Build using the natively-available default compiler
2. CROSS_RPI                       - Cross-compile for the Raspberry Pi 3 platform
3. CROSS_BBB                       - Cross-compile for the Beaglebone Black platform
```

To build the basic demo scenario, set **X86_64** to `ON`, and the other two to `OFF`. 

**Note**: Setting **X86_64** to `ON` should work for natively building on any platform including non x86_64 platforms. However, some low-power platforms like the Beaglebone Black and the Raspberry Pi run out of resources during compilation, and hence require cross-compilation. To know more about cross compiling Quartz for these platforms check out the section on [Cross-Compilation for the Beaglebong Black and Raspberry Pi 3](#cross-compilation).

Once the options are configured press `c` to generate the configuration. You may have to configure (press `c`) multiple times in order to expose all the options. If the configuration is succesful, press `g` to generate the makefiles and exit (press `q`) the CMake Configuration screen. 

You are now ready to build the stack.
```
$> make
$> make install
```

All the components of the containerized stack are now ready to use. The next step is to containerize each individual component of the stack. 

**Note**: You can now exit the container. The following steps should be performed on the host/development machine.

## STEP 4 : Build the containers ##
To build the containers run the following script.
```
$> cd DockerFiles
$> ./build_containers.sh
```

The `build_containers.sh` script builds the following container images: 
* *qot-helloworld*: C++ QoT Helloworld App
* *qot-python-app*: Python QoT Helloworld App
* *qot-timeline-service*: Timeline Service
* *qot-sync-service*: QoT Clock Synchronization Service
* *qot-coord-service*: Coordination Service

The built images can be used locally or pushed to your favorite container repository.

## STEP 5 : Run a Test deployment of Quartz using Kubernetes ##
To run a test deployment of Quartz we use the Kubernetes container-orchestration framework. There are many ways to setup a Kubernetes cluster, the simplest way involves setting up a single VM cluster using `minikube`. The instructions to setup minikube can be found at https://kubernetes.io/docs/setup/minikube/

Applications and services are deployed as pods. Once you have a kubernetes cluster running, you will either need to re-build the containers locally in Kubernetes' Docker context, or you can substitute the container `image` field in the yaml with the appropriate path to an image in the container repository. The instructions below assume `minikube`, but they can be easily modified for any other kubernetes cluster.

Initialize the minikube cluster. Skip this step if you are not using minikube:
```
$> minikube start
```
Now, if we use minikube, we point all docker commands to `docker` inside the minikube cluster, and then we build all the container images in minikube's docker context,
```
$> eval $(minikube docker-env)
$> cd DockerFiles
$> ./build_containers.sh
```
Now, that we have built the containers in the minikube context, let us deploy the QoT services in a single pod
```
$> cd DockerFiles/kubernetes-yaml/k8s-pod-config
$> kubectl create -f pod-qot-services.yaml
```
**Note**: Each service can also be spun up in separate pods. We deploy them in a single pod only for demonstration purposes. Also, note that the services mount a host directory to enable inter-pod communication using UNIX Domain Sockets. In case you have pushed the container images for the services to a repository, replace the the container `image` field in the yaml to the container repository path.

Now, once the services are running, we can launch the Hello World application pod.
```
$> cd DockerFiles/kubernetes-yaml/k8s-pod-config
$> kubectl create -f pod-qot-app.yaml
```
You now have both the services and application pods running. However, to see the application/services in action, their deployment has deliberately not been automated. We still need to get a shell to the pod in order to launch the app and the services. Additionally, the QoT services make use of NATS and Zookeeper, therefor we also need to launch these pre-requisite services before-hand.

### STEP 5.1 : Setup NATS and Zookeeper ###
Any existing NATS server can be used by the services. However, the easiest way to deploy one inside the kubernetes cluster is as follows:
```
$> cd DockerFiles/kubernetes-yaml/nats-k8s-pod-config
$> kubectl create -f nats.yaml
```
You should now have a working NATS server, accesible within the Kubernetes cluster on `nats.default.svc.cluster.local:4222`.

Similarly, any existing Zookeeper server can be used by the services. However, the easiest way to deploy a single node instance inside the kubernetes cluster is as follows:
```
$> cd DockerFiles/kubernetes-yaml/zk-k8s-pod-config
$> kubectl create -f zk.yaml
```

You should now have a working Zookeeper server, accesible within the Kubernetes cluster on `simple-zk.default.svc.cluster.local:2181`.

### STEP 5.2 : Start the Services ###
Open 3 terminals, run the following batches of commands to start the services

**Terminal 1: Coordination Service**
```
$> kubectl exec -it qot-services --container qot-coord-service -- /bin/bash
# A new shell to the container should appear
# Launch the coordination service: the first argument is the zookeeper server, and the second is the NATS server
$> python coordination_service/app.py -z <zookeeper-ip> -p <nats-ip>
```

**Terminal 2: Clock Synchronization Service**
```
$> kubectl exec -it qot-services --container qot-sync-service -- /bin/bash
# A new shell to the container should appear
# Launch the clock sync service
$> qot_sync_service -v -m nats://<nats-ip>
```

**Terminal 3: Timeline Service**
```
$> kubectl exec -it qot-services --container qot-timeline-service -- /bin/bash
# A new shell to the container should appear
# Launch the timeline service: the first argument is the unique name of the node, and the second is the NATS server, and the third is the coordination server REST endpoint
$> qot_timeline_service <node-name> <nats-ip> http://localhost:8502
```
All three services should be running now.

### STEP 5.3 : Start the demo application ###
Open a new terminal and run the following commands to start the app

**Terminal 4: C++ Helloworld App**
```
$> kubectl exec -it qot-app --container qot-helloworld -- /bin/bash
# A new shell to the container should appear
# Launch the app
$> helloworld_core_cpp
```
Congratulations ! You have succefully run the demo and should now see the app waking up every second and printing the time.

# Priveleged Sync Service #
On some platforms hardware timestamping in the network interface is supported, which can increase the resolution at which synchronization packets are timestamped, and hence increase the accuracy of clock synchronization. However, hardware timestamping generally requires root access to the host machine. From the standpoint of Docker containers, to utilize hardware timestamping, we require a priveleged (root) container which uses the same network as the host (unlike other containers which use a virtual interface exposed by Docker). Additionally, root access also allows the clock-synchronization service to discipline the Linux system clock (`CLOCK_REALTIME`) on the host machine.

**Note**: While Docker support priveleged containers, Kubernetes currently does not support the notion of priveleged pods or containers. Therefore, to utilize a clock-synchronization service with root priveleges we currently only provide instructions to deploy the services using a priveleged development image container.

The steps to deploy the basic demo scenario in a single priveleged Docker container is described in the steps below:

## STEP 1: Build and Deploy a Priveleged Development Container ##
```
# Navigate to the dev-image directory
$> cd quartz # Or the root of the repo
$> cd Dockerfiles/dev-image
# Building the Dev Image (same image as the development container)
$> docker build -t qot-devimage . 
# Deploy the Built image as a priveleged container with host network access
$> REPO_PATH=/absolute-path-to-quartz # Replace with the absolute path to the repo root directory
$> ./run_priveleged_host.sh $REPO_PATH
```
You should now get a new shell to the priveleged container.

## STEP 2: Build and Install the services with priveleged container support ##
The steps are the same as the previously described, with the only change being set **BUILD_SYNC_PRIVELEGED** to `ON` in the cmake configuration step. After building and installing you should have the appropriate services built with hardware timestamping and priveleged mode support.

## STEP 3: Run the Service and Applications ##
You should now be able to run the services and the demo application (as described in previous sections), inside the development container, by getting a shell to the container using (`docker exec -it devimage /bin/bash`).

**Note**: You may need to run NATS, Zookeeper and the coordination service as Kubernetes pods with their interfaces exported as services and a port set using the `NodePort` option in the yaml specification. This allows these kubernetes services to be accessed by any other external node (including docker containers) using the exposed `NodePort`.

# Cross-Compilation for the Beaglebone Black and Raspberry Pi 3 #
The provided development image contains the necessary compilers, header libraries and sysroot required to compile Quartz for the Beaglebone Black and the Raspberry Pi 3 platforms. The entire project is cmake-driven, and so the following should suffice to build using the provided cross compilation tool-chains:

```
$> mkdir -p build % Do this in the top most project directory /qot-stack %
$> pushd build
# Run only one of the two following options based on platform
$> ccmake .. -DCROSS_RPI=ON # Build config for the Raspberry Pi 3
$> ccmake .. -DCROSS_BBB=ON # Build config for the Beaglebone Black
##
```

You should now see a cmake screen in your terminal with multiple configuration options, and the same instructions as in the [Setup](#setup) section can be used to configure the required build and installation options. Subsequently, once the options are configured, and the makefiles are generated using CMake, the following can be use to build and install Quartz:

```
$> make 
$> make install 
```

**Note**: On setting the **CONTAINER_BUILD** option to `ON` while cross-compiling, the generated binaries, libraries and headers are installed to `<repo-path>/DockerFiles/rpi-binaries` or `<repo-path>/DockerFiles/bbb-binaries` for the Raspberry Pi 3 and Beaglebone Black platforms respectively. Note that, as the above mentioned platforms are ARM devices the containers cannot be built in an x86 environment (on which cross-compilation is performed). Therefore, these binaries need to be copied to the `<repo-path>/DockerFiles/binaries` on the target platform, where the per-service containers can be built using the `build_containers.sh` script in the `<repo-path>/DockerFiles/` directory on the target platform. These, built containers can be pushed to a container repository for future re-use.

To run a demo on the Raspberry Pi 3, once the containers are built, the simple demo scenario described using Kubernetes can be used, as the Raspberry Pi 3 support kubernetes. To install Docker and Kubernetes on the Raspberry Pi 3 please refer to the following tutorial https://github.com/alexellis/k8s-on-raspbian.

To run a demo on the Beaglebone Black, once the containers are built, a demo can only be run using Docker, as the Raspberry Pi 3 does not have sufficient resources to install and run Kubernetes. To install Docker on a Beaglebone Black using a debian-based Linux distribution use the following link https://docs.docker.com/install/linux/docker-ce/debian/


# Support #

[//]: <> (Questions can be directed to Anon D'Anon (sandeepd@andrew.cmu.edu))