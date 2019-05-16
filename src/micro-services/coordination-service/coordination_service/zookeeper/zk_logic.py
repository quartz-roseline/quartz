# @file zk_logic.py
# @brief QoT Coordination Service Zookeeper Client Logic
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

# Import components of the Kazoo Python Zookeeper Client Library
from kazoo.client import KazooClient
from kazoo.client import KazooState
from kazoo.recipe.election import Election

# Import the NATS Publisher Wrapper Class
from coordination_service.zookeeper.nats_pub import NATSPublisher

# Import the Python Logging Library
import logging

# Import the Python JSON Library
import json

# Initialize Logging
logging.basicConfig()

# Zookeeper Coordinator Class
class ZKCoordinator:
	def __init__(self):
		self.zk = None                    # Client Instance
		self.hosts = '127.0.0.1:2181'     # Server List
		self.connected = False            # Connection state
		self.zk_state = KazooState.LOST   # Zookeeper State
		self.coordinator_name = None      # Coordinator UUID
		self.publisher = NATSPublisher()  # NATS Publisher
		self.remote_servers = {}		  # Per Timeline Remote Servers

	def zookeeper_listener(self, state):
		'''Listens for changes to the Zookeeper session connection state'''
		self.zk_state = state
		if state == KazooState.LOST:
			# Register somewhere that the session was lost
			self.connected = False 
			print("qotCoordinator: Zookeeper entered LOST state")
		elif state == KazooState.SUSPENDED:
			# Handle being disconnected from Zookeeper
			self.connected = False
			print("qotCoordinator: Zookeeper entered SUSPENDED state")
		else:
			# Handle being connected/reconnected to Zookeeper
			self.connected = True
			print("qotCoordinator: Zookeeper entered CONNECTED state")

	def zookeeper_connect(self, zk_hosts, pub_host="localhost:4222"):
		'''Connect to the Zookeeper Servers and start the session'''
		try:
			self.zk = KazooClient(hosts=zk_hosts)
			self.zk.start()
			self.zk.add_listener(self.zookeeper_listener)
			print("qotCoordinator: Successfully connected to Zookeeper: " + str(zk_hosts))
			self.hosts = zk_hosts

			# Connect to the NATS Publisher
			self.publisher.publisher_connect(pub_host)
			return 0
		except Exception as e: 
			print(e)
			return -1

	def zookeeper_stop(self):
		'''Stop the Zookeeper session'''    
		try:
			if self.zk != None:
				self.zk.stop()
				print("qotCoordinator: Successfully stopped Zookeeper session")
				return 0
			else:
				return -1
			self.publisher.terminate_publisher()
		except Exception as e: 
			print(e)
			return -1

	def zookeeper_bootstrap(self, coordinator_uuid):
		'''Initialize the coordinator basic node requirements'''
		# Ensure that /timelines & /coordinators path exists, create if necessary
		self.zk.ensure_path("/timelines")
		self.zk.ensure_path("/coordinators")

		# Ensure that /servers path exists, create if necessary
		self.zk.ensure_path("/servers")

		# Create the node under the coordination group
		coord_path = "/coordinators/" + str(coordinator_uuid)
		# try:
		#   self.zk.create(coord_path, ephemeral=True)
		# except Exception as e: 
		#   print(e)

		self.coordinator_name = coordinator_uuid

	def elected_master(self):
		'''Callback function if Leader Election Suceeded'''
		# TBD -> Elaborate on this function
		print("qotCoordinator: Elected Master")

	def run_for_master(self):
		'''Leader election among multiple coordinators
		   Note: Blocks till the node is elected master'''
		# TBD => Make this function better ! -> replace my-identifies with a real identifier
		election = self.zk.Election("/coordinators/" + str(self.coordinator_name), "my-identifier")

		# blocks until the election is won, then calls elected_master()
		election.run(self.elected_master)

	def create_timeline(self, timeline_uuid, info):
		'''Registers that a node in the coordination group is on this timeline'''
		# Ensure the parent timeline node exists
		self.zk.ensure_path("/timelines/" + str(timeline_uuid))
		self.zk.ensure_path("/timelines/" + str(timeline_uuid) + "/nodes")
		self.zk.ensure_path("/timelines/" + str(timeline_uuid) + "/servers")

		# TBD: Register the node -> Replace "yes" with the JSON in info
		self.zk.create("/timelines/" + str(timeline_uuid) + "/nodes/" + self.coordinator_name, info, ephemeral=True) 

		# Create an empty dictionary to store the remote servers
		self.remote_servers[timeline_uuid] = {}

		# Create a watch for the timeline children
		@self.zk.ChildrenWatch("/timelines/" + str(timeline_uuid) + "/nodes")
		def watch_tlchildren(children):
			print("gotCoordinator: Timeline " + timeline_uuid + " has children " + str(children))
			children_info = {}
			for child in children:
				try:
					data, stat = self.zk.get("/timelines/" + str(timeline_uuid) + "/nodes/" + str(child))
					children_info[str(child)] = json.loads(data.decode('utf-8'))
				except Exception as e: 
					print(e)

			# If non-empty publish children info to NATS
			if bool(children_info) != False:
				topic = "coordination.timelines." + str(timeline_uuid) + ".global"
				self.publisher.publish_data(topic, json.dumps(children_info).encode('utf-8'))

		# Create a watch for the timeline servers
		@self.zk.ChildrenWatch("/timelines/" + str(timeline_uuid) + "/servers")
		def watch_tlservers(children):
			print("gotCoordinator: Timeline " + timeline_uuid + " has servers " + str(children))
			children_info = {}
			for child in children:
				try:
					data, stat = self.zk.get("/timelines/" + str(timeline_uuid) + "/servers/" + str(child))
					children_info[str(child)] = json.loads(data.decode('utf-8'))
				except Exception as e: 
					print(e)

			# If non-empty publish servers to NATS & Store a copy locally
			if bool(children_info) != False:
				# Store Server Info
				self.remote_servers[timeline_uuid] = children_info
				# Publish to NATS
				topic = "coordination.timelines." + str(timeline_uuid) + ".servers"
				self.publisher.publish_data(topic, json.dumps(children_info).encode('utf-8'))
				
		return 0

	def delete_timeline(self, timeline_uuid):
		'''Removes the coordination group node from this timeline'''
		# Delete the node
		self.zk.delete("/timelines/" + str(timeline_uuid) + "/nodes/" + self.coordinator_name)
		
		# Check if the parent timeline has any children
		children = self.zk.get_children("/timelines/" + str(timeline_uuid) + "/nodes")
		if len(children) == 0:
			self.zk.delete("/timelines/" + str(timeline_uuid) + "/nodes")					# Delete "nodes" node
			self.zk.delete("/timelines/" + str(timeline_uuid) + "/servers", recursive=True) # Delete all servers
			self.zk.delete("/timelines/" + str(timeline_uuid))								# Delete the timeline
		return 0

	def update_timeline(self, timeline_uuid, info):
		'''Updates the coordination group node info on this timeline'''
		try:
			self.zk.set("/timelines/" + str(timeline_uuid) + "/nodes/" + self.coordinator_name, info)
			return 0
		except Exception as e: 
			print(e)
			return -1

	def get_servers(self, timeline_uuid):
		'''Get the remote servers for a timeline'''
		return self.remote_servers[timeline_uuid]


	def register_timeline_server(self, server_name, timeline_uuid, info):
		'''Registers the availability of a timeline NTP server'''
		# Ensure the parent timeline node exists
		self.zk.ensure_path("/timelines/" + str(timeline_uuid) + "/servers")

		# TBD: Register the node -> Replace "yes" with the JSON in info
		self.zk.create("/timelines/" + str(timeline_uuid) + "/servers/" + str(server_name), info, ephemeral=True) 
		
		return 0

	def delete_timeline_server(self, server_name, timeline_uuid):
		'''Removes the timeline NTP server'''
		# Delete the node
		self.zk.delete("/timelines/" + str(timeline_uuid) + "/servers/" + str(server_name))
		return 0

	def register_server(self, server_name, info):
		'''Registers the availability of a global NTP server'''
		# Ensure the parent timeline node exists
		self.zk.ensure_path("/servers")
		# TBD: Register the node -> Replace "yes" with the JSON in info
		self.zk.create("/servers/" + str(server_name), info, ephemeral=True) 
		
		return 0

	def delete_server(self, server_name):
		'''Removes the server'''
		# Delete the node
		self.zk.delete("/servers/" + str(server_name))
		return 0

