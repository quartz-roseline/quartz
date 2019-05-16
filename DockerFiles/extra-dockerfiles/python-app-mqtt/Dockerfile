FROM python:3

RUN pip install asyncio-nats-client && pip install paho-mqtt && pip install protobuf && pip install pause

# Copy the helloworld-app
COPY binaries/bin/helloworld_mqtt.py /usr/local/bin

# Copy the Library files
COPY binaries/lib/qot_coreapi.py /usr/local/lib

# Copy the Sherlock Protobuf files
COPY binaries/lib/datastream_pb2.py /usr/local/lib

# COPY the certificates
COPY certificates /

CMD ["/bin/ping","localhost"]