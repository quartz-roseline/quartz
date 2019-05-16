FROM python:3

RUN pip install asyncio-nats-client && pip install pause

# Copy the helloworld-app
COPY binaries/bin/helloworld.py /usr/local/bin

# Copy the Library files
COPY binaries/lib/qot_coreapi.py /usr/local/lib

CMD ["/bin/ping","localhost"]
#CMD [ "python", "./test.py" ]