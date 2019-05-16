# @file mqtt_dummy_sensor.py
# @brief Dummy MQTT Sensor
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

# Argument Parser
import argparse

# MQTT Client
import paho.mqtt.client as mqtt

# Disable SSL hostname matching
import ssl
ssl.match_hostname = lambda cert, hostname: True

# Module to Read System Time
import time

# Default topic 
topic = "qot/timeline/my_test_timeline/actor"

def main_func(broker: str, port: int, ca_certs: str, certfile: str, keyfile: str):
	global topic
	client = mqtt.Client()

	# Configure TLS Certificate
	if ca_certs != None and certfile != None and keyfile != None:
		client.tls_set(ca_certs=ca_certs, certfile=certfile, keyfile=keyfile)

	# Connect to MQTT Broker
	client.connect(broker, port)

	# Periodically Publish the sensor payload -> here it is a floating point CLOCK_REALTIME timestamp
	counter = 0
	while 1:
		time_val = str(time.clock_gettime(time.CLOCK_REALTIME))
		counter = counter + 1
		print('Dummy Sensor Publishing Message %d with timestamp %s' % (counter, time_val))
		client.publish(topic, time_val)
		# Sleep for 10 seconds
		time.sleep(10)

if __name__ == '__main__':
	parser = argparse.ArgumentParser(description='Dummy MQTT Actor')
	parser.add_argument('--timeline', '-t', default='my_test_timeline', type=str, help='name of timeline to bind to')
	parser.add_argument('--broker', '-b', default='test.mosquitto.org', type=str, help='IP or URL of the MQTT broker')
	parser.add_argument('--port', '-p', default='1883', type=int, help='MQTT port')
	parser.add_argument('--cacerts', default=None, type=str, help='CA certificate path')
	parser.add_argument('--certfile', default=None, type=str, help='Certificate file path')
	parser.add_argument('--keyfile', default=None, type=str, help='Keyfile path')
	args = parser.parse_args()

	topic = "qot/timeline/" + args.timeline + "/sensor"

	main_func(args.broker, args.port, args.cacerts, args.certfile, args.keyfile)