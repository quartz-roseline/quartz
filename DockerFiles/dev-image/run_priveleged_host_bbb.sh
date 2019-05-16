#!/bin/sh
# The argument is the absolute path to the repo
echo Running the QoT Dev Docker Container with priveleges
docker run -ti --rm --network host --privileged --name devimage -v $1:/opt/qot-stack:z 192.168.1.102:443/qot-devimage-bbb
