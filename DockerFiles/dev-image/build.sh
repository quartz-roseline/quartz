#!/bin/sh
# The argument is the absolute path to the repo
echo Building the Dev Image
docker build -t qot-devimage .
