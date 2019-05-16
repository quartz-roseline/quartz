#!/bin/sh
# The argument is the absolute path to the repo
echo Running the QoT Dev Docker Container
docker run -ti --rm -v $1:/opt/qot-stack:z --name devimage -v $2:/rpi-mount/usr:z qot-devimage
