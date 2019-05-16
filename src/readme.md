# Quartz Source Code #

This folder contains the Quartz source code, divided into the following distinct modules.

* **api** - the Application Programming Interface that developers use to create, bind, destroy and interact with timelines.
* **examples** - example applications showing how to use the QoT stack.
* **micro-services** - new-generation micro-services for the fully user-space implementation of the containerized stack
* **test** - unit tests for the system components.

All the  modules above rely on the file **qot_types.h**, which defines the fundamental time data types in the system, basic uncertain time mathematics, and kernel-userspace ioctl message types.
