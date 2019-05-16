FROM ubuntu:16.04

# Install dependencies
RUN apt-get update -y && apt-get install -y build-essential libboost-dev libboost-thread-dev libboost-log-dev libboost-program-options-dev cmake cmake-curses-gui git libcpprest-dev curl ethtool iputils-ping wget autoconf bison flex gdb \
    && apt-get install -y locales \
    && apt-get install -y python3-pip python3-dev \
    && pip3 install pause \
    && pip3 install asyncio-nats-client \
    && pip3 install networkx \
    && pip3 install numpy \
    && rm -rf /var/lib/apt/lists/* \
    && localedef -i en_US -c -f UTF-8 -A /usr/share/locale/locale.alias en_US.UTF-8

# Get the Raspberry-pi Cross compilation toolchain (4.9.3)
RUN git clone --depth 3 https://github.com/raspberrypi/tools.git rpi-tools

# Get the new Raspberry-pi Cross compilation toolchain (6.3.1)
RUN git clone https://github.com/rvagg/rpi-newer-crosstools.git

# Copy Libraries and Headers from the RPI directories into the sysroot
COPY rpi-headerslibs /rpi-headerslibs-mount 
RUN cp -rn /rpi-headerslibs-mount/lib/. /rpi-tools/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/arm-linux-gnueabihf/sysroot/usr/lib \
  && cp -rn /rpi-headerslibs-mount/include/. /rpi-tools/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/arm-linux-gnueabihf/sysroot/usr/include \
  && cp -rn /rpi-headerslibs-mount/lib/. /rpi-newer-crosstools/x64-gcc-6.3.1/arm-rpi-linux-gnueabihf/arm-rpi-linux-gnueabihf/sysroot/usr/lib \
  && cp -rn /rpi-headerslibs-mount/include/. /rpi-newer-crosstools/x64-gcc-6.3.1/arm-rpi-linux-gnueabihf/arm-rpi-linux-gnueabihf/sysroot/usr/include 

# Download and compile OpenSSL for Raspberry-Pi (with the 4.9.3 toolchain)
RUN cd /rpi-tools && wget https://www.openssl.org/source/openssl-1.0.2l.tar.gz \
  && tar xzf openssl-1.0.2l.tar.gz \
  && cd openssl-1.0.2l \
  && export PATH=$PATH:/rpi-tools/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/bin \
  && ./Configure --prefix=/rpi-tools/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/arm-linux-gnueabihf/sysroot/openssl os/compiler:arm-linux-gnueabihf shared\
  && make CC="arm-linux-gnueabihf-gcc" AR="arm-linux-gnueabihf-ar r" RANLIB="arm-linux-gnueabihf-ranlib" \
  && make install \
  && cp -a /rpi-tools/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/arm-linux-gnueabihf/sysroot/openssl/include/. /rpi-tools/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/arm-linux-gnueabihf/sysroot/usr/include \
  && cp -a /rpi-tools/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/arm-linux-gnueabihf/sysroot/lib/. /rpi-tools/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/arm-linux-gnueabihf/sysroot/usr/lib/ \
  && cp -a /rpi-tools/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/arm-linux-gnueabihf/sysroot/openssl/include/. /rpi-newer-crosstools/x64-gcc-6.3.1/arm-rpi-linux-gnueabihf/arm-rpi-linux-gnueabihf/sysroot/usr/include

# Download and compile C++ Rest SDK for Raspberry-Pi (with the 4.9.3 toolchain)
#RUN cd /rpi-tools && wget https://github.com/Microsoft/cpprestsdk/archive/v2.9.0.tar.gz && tar xvf v2.9.0.tar.gz && rm v2.9.0.tar.gz \
#  && cd cpprestsdk-2.9.0/Release && mkdir build && cd build \
#  && cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/rpi-tools/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/bin/arm-linux-gnueabihf-g++ -DCMAKE_INSTALL_PREFIX=/rpi-tools/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/arm-linux-gnueabihf/sysroot/usr/ -DCMAKE_FIND_ROOT_PATH=/rpi-tools/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/arm-linux-gnueabihf/sysroot/ -DBUILD_TESTS=OFF -DBUILD_SAMPLES=OFF \
#  && make && make install

# Copy over the chrony NTP config file 
COPY chrony.conf /etc/chrony.conf

# Set the language environment variable
ENV LANG en_US.utf8

# overwrite this with 'CMD []' in a dependent Dockerfile -> opens a bash terminal now
CMD ["/bin/bash"]

