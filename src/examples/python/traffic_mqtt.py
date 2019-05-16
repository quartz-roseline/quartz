# @file traffic_mqtt.py
# @brief Helloworld QoT Python Applications with Simple MQTT Traffic Light actor
# @author Anon D'Anon
#
# Copyright (c) Anon, 2018.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

import sys 
import os
import time
import argparse
from enum import Enum

# Import the QoT API Library
sys.path.append(os.path.abspath("/usr/local/lib"))
from qot_coreapi import TimelineBinding
from qot_coreapi import ReturnTypes

# Import Signal
import signal

# MQTT Client
import paho.mqtt.client as mqtt

# Import AsyncIO
import asyncio

# NATS Python 3 Client
from nats.aio.client import Client as NATS
from nats.aio.errors import ErrConnectionClosed, ErrTimeout, ErrNoServers

# Disable SSL hostname matching
import ssl
ssl.match_hostname = lambda cert, hostname: True

# Import Sherlock-compatible protobuf data type
import datastream_pb2

# Global Variable used to terminate program on SIGINT
running = 1

# Global Timeline Binding Variable
binding = TimelineBinding("app")

# Setup Global MQTT CLient 
client = mqtt.Client()

# Global Timeline UUID
timeline_uuid = "my_test_timeline"

# MQTT On Connect Handler
def on_connect(client, userdata, flags, rc):
	if rc==0:
		print("Connected to MQTT Broker Returned code %d ",rc)
	else:
		print("Bad MQTT connection Returned code %d", rc)

# SIGINT signal handler
def signal_handler(signal, frame):
	print('Program Exiting')
	global running
	running = 0

def nats_subscribe(loop, nats_broker: str, nats_topic: str):
	'''Run the NATS Subscriber '''
	nc = NATS()

	# Connect to the NATS Server
	print ('Connecting to NATS broker %s' % nats_broker)
	yield from nc.connect(servers=[nats_broker], io_loop=loop)

	@asyncio.coroutine
	def message_handler(msg):
		# Do Protobuf de-serialization
		pb_msg = datastream_pb2.DataStreamMessage()
		pb_msg.ParseFromString(msg.data)

		print ('-------------------------------------------------------------')
		print ('Received Timestamp from QoT Transform %s' % pb_msg.payload)

		# Read a Timeline Timestamp
		global binding
		global client
		tl_time = binding.timeline_gettime()
		print ('Sending Command with timestamp %f to traffic light' % (tl_time["time_estimate"]+1))

		# Publish to a topic
		global timeline_uuid
		topic = "qot/timeline/" + timeline_uuid + "/traffic_light0"

		# Publish to Dummy Actuator
		client.publish(topic, str(tl_time["time_estimate"]+1))


	# Simple publisher and async subscriber via coroutine.
	print ('Subscribing to NATS topic %s' % nats_topic)
	sid = yield from nc.subscribe(nats_topic, cb=message_handler)

	# Loop until the process invokes ctrl + c
	global running
	while running:
		yield from asyncio.sleep(1, loop=loop)

	# Disconnect from NATS Server
	yield from nc.close()

def dispatch_nats(nats_broker: str, nats_topic: str):
	''' Thread Handler to dispatch NATS Subscriber'''
	loop = asyncio.get_event_loop()

	# Run AsynIO Loop
	loop.run_until_complete(nats_subscribe(loop, nats_broker, nats_topic))


def main_func(timeline_uuid: str, app_name: str, mqtt_broker: str, port: int, ca_certs: str, certfile: str, keyfile: str, nats_broker: str, nats_topic: str):

	# Register signal handler
	signal.signal(signal.SIGINT, signal_handler)

	print("Hello World !")

	# Configure TLS Certificate
	global client
	if ca_certs != None and certfile != None and keyfile != None:
		client.tls_set(ca_certs=ca_certs, certfile=certfile, keyfile=keyfile)

	# Bind the On Connect handler
	client.on_connect = on_connect

	# Connect to MQTT Broker
	client.connect(mqtt_broker, port)

	# Start the MQTT Client Loop to handle the callbacks
	client.loop_start()

	# Bind to the timeline
	global binding
	retval = binding.timeline_bind(timeline_uuid, app_name, 1000, 1000)
	if retval != ReturnTypes.QOT_RETURN_TYPE_OK:
		print ('Unable to bind to timeline, terminating ....')
		exit (1)

	# Loop and sleep till we get a ctrl + c (AsyncIO Loop)
	dispatch_nats(nats_broker, nats_topic)

	# Unbind from the timeline
	print("Unbinding from timeline")
	binding.timeline_unbind()
	client.loop_stop()
	client.disconnect()


if __name__ == '__main__':
	parser = argparse.ArgumentParser(description='Helloworld Python QoT App with MQTT actors')
	parser.add_argument('--timeline', '-t', default='my_test_timeline', type=str, help='name of timeline to bind to')
	parser.add_argument('--app', '-a', default='qot_app', type=str, help='name of app component')
	parser.add_argument('--broker', '-b', default='mqttserver-svc.default.svc.cluster.local', type=str, help='IP or URL of the MQTT broker')
	parser.add_argument('--port', '-p', default='1883', type=int, help='MQTT port')
	parser.add_argument('--cacerts', default=None, type=str, help='CA certificate path')
	parser.add_argument('--certfile', default=None, type=str, help='Certificate file path')
	parser.add_argument('--keyfile', default=None, type=str, help='Keyfile path')
	parser.add_argument('--natstopic', default='datastream-qot_transform_traffic', type=str, help='Keyfile path')
	parser.add_argument('--natsbroker', default='nats://nats.default.svc.cluster.local:4222', type=str, help='Keyfile path')

	args = parser.parse_args()
	timeline_uuid = args.timeline
	main_func(args.timeline, args.app, args.broker, args.port, args.cacerts, args.certfile, args.keyfile, args.natsbroker, args.natstopic)