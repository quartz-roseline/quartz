FROM ubuntu:16.04

# Install dependencies
RUN apt-get update -y && apt-get install -y iputils-ping build-essential libboost-dev libboost-thread-dev libboost-log-dev libboost-program-options-dev libcpprest-dev \
    && apt-get install -y locales && rm -rf /var/lib/apt/lists/* \
    && localedef -i en_US -c -f UTF-8 -A /usr/share/locale/locale.alias en_US.UTF-8 \
    && ldconfig

# Set the language environment variable
ENV LANG en_US.utf8

# Copy the timeline service
COPY binaries/bin/qot_timeline_service /usr/local/bin

# Copy the Library files
COPY binaries/lib/ /usr/local/lib

# Run ldconfig
RUN ldconfig

# overwrite this with 'CMD []' in a dependent Dockerfile -> opens a bash terminal now
#CMD ["/bin/bash"]
CMD ["/bin/ping","localhost"]
