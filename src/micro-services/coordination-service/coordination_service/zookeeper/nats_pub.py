# @file nats_pub.py
# @brief QoT Coordination Service NATS Data Publisher
# @author Anon D'Anon
#  
# Copyright (c) Anon, 2018. All rights reserved.
# Copyright (c) Anon Inc., 2018. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without modification, 
# are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright notice, 
#     this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright notice, 
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Import the Python JSON Library
import json

# Import AsyncIO
import asyncio

# NATS Python 3 Client
from nats.aio.client import Client as NATS
from nats.aio.errors import ErrConnectionClosed, ErrTimeout, ErrNoServers

# For Spawning Threads and using Mutex Locks
from threading import Thread, Lock

# NATS Publisher Class
class NATSPublisher:
	def __init__(self):
		self._nats_thread_running = True 						# Flag to terminate thread
		self._nats_hosts = []        						    # NATS Server IP
		self._nats_servers = []

		# Initialize AsyncIO Loop
		self._loop = asyncio.new_event_loop()

	# @asyncio.coroutine
	def _nats_run(self):
		''' Run NATS '''
		self._nc = NATS()

		# Connect to the NATS Server
		try:
			yield from self._nc.connect(servers=self._nats_servers, io_loop=self._loop)
		except ErrNoServers as e:
			print(e)
			return

		print("qotCoordinator: Connected to NATS on " + str(self._nats_servers))
		yield from self._nc.flush()

	def _dispatch_nats(self):
		''' Thread Handler to dispatch NATS '''
		# Run AsynIO Loop
		self._loop.run_until_complete(self._nats_run())
		self._loop.run_forever()
		
	def publisher_connect(self, host="localhost:4222"):
		'''Connect to NATS'''
		self._nats_hosts.append(host)        						
		for host in self._nats_hosts:
			self._nats_servers.append(str("nats://") + str(host))

		# Create the NATS Thread
		asyncio.set_event_loop(self._loop)
		self._nats_thread = Thread(target=self._dispatch_nats)
		self._nats_thread.start()

	def publish_data(self, topic, data):
		''' Publish Data '''
		print("qotCoordinator: Publishing data " + str(data) + " on " + str(topic))
		if not self._nc.is_connected:
			print("qotCoordinator: Not connected to NATS!")
			return
		
		# Publish the data
		asyncio.run_coroutine_threadsafe(self._nc.publish(topic, data), loop=self._loop)

	def terminate_publisher(self):
		''' Close the NATS Publisher Thread '''
		self._nats_thread_running = False
		self._loop.close()
		self._nats_thread.join()
	