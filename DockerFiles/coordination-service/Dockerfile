FROM python:3

# Copy the helloworld-app
COPY binaries/bin/coordination-service coordination-service

WORKDIR coordination-service

# Create the Virtual Environment
RUN pip install -r requirements.txt
RUN python setup.py develop

#CMD ["/bin/bash"]
CMD ["/bin/ping","localhost"]
#CMD [ "python", "./test.py" ]