# Quartz: Time-as-a-Service for Containerized Applications #

# Overview #
Having a shared sense of time is critical to distributed systems. Although time synchronization is a mature field of study, the manner in which time-based applications interact with time has remained largely the same --synchronization runs as a best-effort background service that consumes resources independently of application demand, and applications are not aware of how accurate their current estimate of time actually is.

Motivated by Cyber-Physical Systems (CPS) and the Internet of Things (IoT), in which application requirements need to be balanced with system resources, we advocate for a new way of thinking about how applications interact with time. We introduce the new notion of Quality of Time (QoT) as the, *"end-to-end uncertainty in the notion of time delivered to an application by the system"*. We propose that the notion of Quality of Time (QoT) be both *observable* and *controllable*. A consequence of this being that time synchronization must now satisfy timing demands across a network, while minimizing the system resources, such as energy or bandwidth.

Quartz is a user-space implementation of the Quality of Time (QoT) Stack for Linux, and has been built from the ground up for containerized applications. It features a rich application programming interface (API) that is centered around the notion of a *timeline* -- a virtual sense of time to which applications bind with their desired *accuracy interval* and *minimum resolution* timing requirements. A timeline can span accross multiple nodes, and provides all applications *bound* to it a shared notion of time. This enables developers to easily write choreographed applications that are capable of specifying and observing timing uncertainty. 

Based on application QoT requirements, Quartz orchestrates the various system components to deliver the required Quality of Time to applications. Thus, Quartz exposes 'Time-as-a-Service' to time-aware applications. In addition, the ability to observe timing uncertainty allows applications to order timestamped events with greater confidence, and adapt if QoT degrades beyond a desired specification. We believe that a host of applications ranging from databases and logging applications in cloud/edge-computing systems, to distributed coordinated IoT applications can benefit from utilizing Quartz. A video showing a prototype distributed application involving multi-robot coordination, developed using the QoT Stack for Linux can be found at https://youtu.be/7NoxnZEWDrM

Quartz features a distributed implementation, composed of containerized services. It works by constructing a timing subsystem in Linux that runs parallel to timekeeping and POSIX timers. In this stack quality metrics are passed alongside time calls. The implementation builds upon existing open-source software, such as NTP (chrony), LinuxPTP, Zookeeper, and NATS to provide a limited subset of features to illustrate the usefulness of our architecture.

# Quartz Architecture and Implementation #
This section describes the components of Quartz in details. Specifically, we focus on:
1. The Quartz API 
2. The Clock and QoT model
3. The Quartz Services

The architectural diagram of Quartz, showing the interaction between the services and applications, is presented below. 

![Alt text](architecture-v1.png?raw=true "Quartz Architecture")

## Quartz API ## 
Quartz provides an API for applications to:
* Bind/unbind to/from a timeline
* Specify/Update QoT requirements
* Timestamp events with the QoT estimate
* Schedule events after a specified absolute time or time duration.

The Quartz API is implemented as a `TimelineBinding` class with public methods. The `TimelineBinding` class definition and implementation is present in a library, which must be imported/linked by the application. The current version of Quartz supports a C++ and Python implementation of the API. Check the files `src/api/cpp/qot_coreapi.hpp` and `src/api/python/qot_coreapi.py` for a description of each of the individual API calls implemented in C++ and Python respectively.

### Example Basic API Usage ###
Below is a few lines of Python code from `src/examples/python/helloworld_app.py` which explains the usage of some of the basic API calls.

```python
# Initialize the TimelineBinding API class in "app" mode (the other mode is "transform" mode)
binding = TimelineBinding("app")

# "test_app" binds to the timeline "my_test_timeline" with accuracy and resolution requirements of 1000ns
timeline_uuid = "my_test_timeline"
app_name = "test_app"
binding.timeline_bind(timeline_uuid, app_name, 1000, 1000)

# Set the Period and Offset (1 second and 0 ns repectively)
binding.timeline_set_schedparams(1000000000, 0)

while True:
    # Read a Timeline Timestamp with the QoT
    tl_time = binding.timeline_gettime()

    # Print the time with the QoT bounds
    print('Timeline time is                %f' % tl_time["time_estimate"])
    print('Upper Bound is                  %f' % tl_time["interval_above"])
    print('Lower Bound is                  %f' % tl_time["interval_below"])

    # Wait until the next period (with the period set using timeline_set_schedparams)
    # -> This example wakes up at every "second" boundary
    binding.timeline_waituntil_nextperiod()

# Unbind from the timeline
binding.timeline_unbind()
```

## Quartz Clocks and QoT ##
From the perspective of applications, each timeline has its own clock. These timeline clocks in Quartz are implemented as a wrapper around the Linux system time, i.e., `CLOCK_REALTIME`. Given a system timestamp `t`, the timeline notion of time `t`<sub>timeline</sub> is given by:

`t`<sub>timeline</sub> = `t` + `offset` + `drift`*(`t`-`t`<sub>last</sub>) 

where, `offset` is the clock offset computed by Quartz, `drift` is the clock frequency drift computed by Quartz, and `t`<sub>last</sub> is the nearest system time instant at which the clock was disciplined.

In addition to the time, each timestamp on a timeline also has the a confidence interval in which the timestamp lies. These upper and lower bounds are the estimated QoT, and are given by <a href="https://www.codecogs.com/eqnedit.php?latex=\epsilon_{lower}" target="_blank"><img src="https://latex.codecogs.com/gif.latex?\epsilon_{lower}" title="\epsilon_{lower}" /></a> and <a href="https://www.codecogs.com/eqnedit.php?latex=\epsilon_{higher}" target="_blank"><img src="https://latex.codecogs.com/gif.latex?\epsilon_{lower}" title="\epsilon_{higher}" /></a>, which implies that the true time `t`<sub>timeline, true</sub> lies in the range <a href="https://www.codecogs.com/eqnedit.php?latex=[t_{timeline}&space;-&space;\epsilon_{lower},&space;t_{timeline}&space;&plus;&space;\epsilon_{higher}]" target="_blank"><img src="https://latex.codecogs.com/gif.latex?[t_{timeline}&space;-&space;\epsilon_{lower},&space;t_{timeline}&space;&plus;&space;\epsilon_{higher}]" title="[t_{timeline} - \epsilon_{lower}, t_{timeline} + \epsilon_{higher}]" /></a>.

The QoT (upper and lower bounds) is computed using a statistically estimated bound on the clock drift `drift`<sub>bound</sub>, and the clock offset `offset`<sub>bound</sub>:

<a href="https://www.codecogs.com/eqnedit.php?latex=\epsilon&space;=&space;offset_{bound}&space;&plus;&space;drift_{bound}*(t&space;-&space;t_{last})" target="_blank"><img src="https://latex.codecogs.com/gif.latex?\epsilon&space;=&space;offset_{bound}&space;&plus;&space;drift_{bound}*(t&space;-&space;t_{last})" title="\epsilon = offset_{bound} + drift_{bound}*(t - t_{last})" /></a>

The implementation for computing these bounds can be found in the `SyncUncertainty` class in the file `src/micro-service/sync-service/SyncUncertainty.cpp`.

In Quartz, each clock is implemented as POSIX shared memory, i.e., all the clock parameters (drift, offset, and their bounds) are stored in a shared-memory region. This memory region is mapped into every application's virtual memory with read-only priveleges, by the API implementation. Therefore, an application can compute the timeline notion of time along with the asociated QoT, with low latency, by reading the Linux system time and applying the clock parameters from the shared-memory region. Note that, all these details are hidden from the application by the API implementation. The parameters in the shared-memory region are updated by the clock-synchronization service, and this will be elaborated in later sections.

## Quartz Services ##
Quartz consists of 3 distinct services:

1. **Timeline Service**: is the central component of the stack, responsible for interacting with applications. It is also tasked with performing the bookkeeping of the timelines that exist on a physical node, the applications bound to each timeline, and the Quality of Time requirements of each application and timeline.
2. **QoT Clock-Synchronization Service**: synchronizes the per-timeline clock and computes the QoT estimates.
3. **Coordination Service**: is the distributed component of the stack, responsible for discovering other nodes on a timeline, and conveying QoT requirements accross nodes. This information can be used by the timeline service to orchestrate the clock-synchronization service based on application requirements.

We now describe the Quartz Services in greater details.

### Timeline Service ###
The timeline service is the core component of Quartz and maintains a list of the timelines that exist on a physical node, the applications bound to each timeline, and the Quality of Time requirements of each application and timeline. 

The timeline service exposes an interface over a UNIX Domain Socket (UDS) `/tmp/qot_timeline`. Over this interface: timelines can be created/destroyed, applications can bind/unbind to/from a timeline, and applications can specify/update QoT requirements. Note that, the API implementation hides the details of accessing the timeline service API over the Unix Domain Socket from user applications. 

When a request to create a timeline is registered, the timeline service creates a local instance of the timeline, along with the POSIX shared-memory region to hold the clock parameters. Since, shared memory is only 'shared' within the confines of a physical node. Therefore, we spawn a separate instance of the timeline service on each physical node. 

A read-only file descriptor to the shared-memory region is then passed to the applications bound to the timeline, over the UDS. This particular capability of a UDS to pass file descriptors  between processes, makes it our medium of choice for the timeline service. In addition, using a UDS restricts applications not running on the same node from interacting with the timeline service accross nodes. The timeline service also sends the clock-synchronization service a read-write file descriptor to the shared-memory region, for it to update the clock projection and QoT parameters.

The timeline service is implemented in C++, and the implementation can be found in the `src/micro-services/timeline-service` directory.

### QoT Clock-Synchronization Service ###
The clock-synchronization service is responsible for continuously disciplining the local timeline clock to the global timeline reference. It is also responsible for computing the parameters used to estimate the QoT bounds. 

The clock-synchronization service currently supports the Network Time Protocol (NTP), based on the `chrony` (https://chrony.tuxfamily.org/) implementation, and the Precision Time Protocol (PTP), based on the `LinuxPTP` (http://linuxptp.sourceforge.net/) implementation. The current implementation defaults to choosing NTP as the default protocol, however in the future we can dynamically chose between NTP and PTP based on the network type and topology. 

The clock-synchronization service also exposes an interface over a UNIX Domain Socket (UDS) `/tmp/qot_clocksync`. This socket is used by the timeline service to communicate with the sync service whenever a timeline is created/destroyed, or the desired QoT on a timeline is updated. This socket can also be used to orchestrate various parameters of the clock-synchronization service ranging from the synchronization rate, to the remote servers to which the clock should be synchronized. 

When the timeline service sends a message about the creation of a new timeline, the clock-synchronization service requests the timeline service for a file descriptor to the shared-memory region hosting the clock parameters. It then maps the shared-memory region into its own virtual-memory space. Thus, whenever the synchronization service updates the clock parameters, they can also be accessed by the user applications.

The clock-synchronization service is implemented in C++, and the implementation can be found in the `src/micro-services/sync-service` directory.

### Coordination Service ###
The coordination service acts as an interface for timeline services to coordinate between and across clusters, so as to maintain a timeline and a single notion of time accross multiple nodes.  It is also responsible for keeping a record of timelines at the cluster level. 

Every *cluster* has one coordination service, where a cluster is a collection of one or more physical nodes. The coordination service exposes a REST API. The per-node timeline services in the cluster can specify the timeline and QoT requirements of the applications running on the node they are responsible for, over this REST API. The API also allows a timeline-service to make queries about other nodes in the cluster which are part of the timeline, and their QoT reqiurements. Thus, providing opportunity for coordination between the services running on a single cluster, in order to maintain a common notion of time with the accuracy desired by the user applications.

For coordination between clusters, the coordination service uses Apache Zookeeper (https://zookeeper.apache.org/). Every coordination service, maintains a Zookeeper client, which connects to a pre-defined Zookeeper ensemble. This ensemble can be hosted in the cloud or a collection of edge nodes.  The Zookeeper ensemble maintains a list of timelines under the path `/timelines`. Each timeline maintains a list of which clusters are participating in the timeline along with their respective desired QoT requirements.

Whenever, one of the timeline services on a cluster first creates a timeline, the coordination service registers the cluster on zookeeper, by creating an *ephemeral* node at the path `/timeline/<name-of-timeline>/<name-of-cluster>` indicating that at least one application on the cluster is utilizing the timeline. The use of an ephemeral node guarantees that in case the coordination service disconnects from the Zookeeper ensemble, then the created node will disappear. The coordination service also sets a `watch` on the children of the `/timeline/<name-of-timeline>` zookeeper node, to discover when other clusters join or leave a particular timeline. Setting a `watch` tells Zookeeper to provide the coordination service an asynchronous notification whenever the clusters part-taking in the timeline change. The coordination service can use this information to perform cross-cluster coordination and maintain a timeline and its notion of time accross multiple clusters. This changing QoT information is also conveyed to the per-node timeline services on the cluster using the NATS messaging service (https://nats.io/). Thus, allowing the timeline service to orchestrate/update the clock-synchronization service on each node.

The coordination service is implemented in Python 3, and the implementation can be found in the `src/micro-services/coordination-service` directory. The service uses the Flask framework (http://flask.pocoo.org/) along with Flask-RESTful (https://flask-restful.readthedocs.io/en/latest/), and the Flask-SQLite database (http://flask.pocoo.org/docs/1.0/patterns/sqlite3/) to create the REST API server. The REST server is exposed on port `8502`, and the API can be accessed over `http:://<coordination-service-ip>:8502`. Opening this url in a browser provides the Swagger UI (https://swagger.io/) which describes the API. The service talks to Zookeeper using the Kazoo client (https://kazoo.readthedocs.io/en/latest/), and publishes to NATS using the Python 3 Asyncio client (https://github.com/nats-io/asyncio-nats).

# Quartz on Kubernetes #
Quartz is designed as a collection of micro-services, and works well with Docker and Kubernetes. In a typical Kubernetes deployment the services need to be deployed in the following manner:

* **Timeline Service:** In a Kubernetes pod running on *each* node in the cluster
* **QoT Clock-Synchronization Service:** In a Kubernetes pod running on *each* node in the cluster
* **Coordination Service:** In a Kubernetes pod, with a Kubernetes service frontend with a `ClusterIP`, running on only one node in the cluster. In the future we will replicate this service with passive replicas to failover to, incase of failure.

The diagram below shows multiple nodes in a Kubernetes cluster, each with their own private timeline and clock-sync service, and a cluster-wide coordination service.
![Alt text](kubernetes-services-v1.png?raw=true "Quartz Architecture")

On each node the timeline and clock-sync services mount the `/tmp/qot` directory on the host as a volume into their respective pods as `/tmp`. This allows them to create their respective Unix Domain Sockets and shared-memory regions, to communicate and share information with each other in the node context. Every application using the Quartz Services also mounts the `/tmp/qot` directory on the host as a volume into their respective pods as `/tmp`, which allows apps to communicate with the timeline service on the node through the API. 

The coordination service is accesible to the timeline services running on each node in the cluster, using the `ClusterIP` provided to it by Kubernetes.

# Quartz @ Scale #
The diagram below illustrates how coordination services running on different clusters, can discover each other using Zookeeper. Thus, allowing coordination accross different clusters, and allowing timelines to be set up and managed over a wide area.
![Alt text](multi-node-architecture-v1.png?raw=true "Quartz Architecture")

