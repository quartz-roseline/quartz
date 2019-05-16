FROM python:3

RUN pip install paho-mqtt

# Copy the mqtt dummy actor
COPY binaries/bin/mqtt_dummy_sensor.py /usr/local/bin

# COPY the certificates
COPY certificates /

CMD ["/bin/ping","localhost"]