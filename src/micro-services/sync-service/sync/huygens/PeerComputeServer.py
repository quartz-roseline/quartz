# @file PeerComputeServer.py
# @brief Server which computes the time translation for each of the peer nodes
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

# Module to Read Core Time
import time

# For Type Checking
import inspect

# For Spawning Threads and using Mutex Locks & Conditional Variables
from threading import Thread, Lock, Condition

# JSON Serialization/De-serialization
import json

# Import AsyncIO
import asyncio

# Import the Math Library
import math

# Import Array Library
import array

# Import numpy
import numpy as np

# Import OS functionality
import os

# Import Pause Library
import pause

# NATS Python 3 Client
from nats.aio.client import Client as NATS
from nats.aio.errors import ErrConnectionClosed, ErrTimeout, ErrNoServers

# Import NetworkX graph library
import networkx as nx

# Import CMDline Argument Parsing functions
import argparse

# Import signal handling capabilitys
import signal

# Global Variable used to terminate program on SIGINT
running = 1

# Global variable to enable debug prints
debug = 0

# SIGINT signal handler (Ctrl+C)
def signal_handler(signal, frame):
	print('Program Exiting')
	global running
	running = 0

class PeerSyncCompute:
	"""PeerSyncCompute class processes offset/drift pairs between various pairs of nodes
	   using the network effect to compute final time estimates"""

	def __init__(self: object, edge_list: list, node_list: list, master: str, period=2, nats_servers=["nats://localhost:4222"]):
		# Initialize default values
		self._period = period    	    # Period in seconds over which data is processed
		self._nats_servers = nats_servers   # NATS servers to subscribe from
		self._edge_list = edge_list         # List of Edges participating in the peer sync (undirected)
		self._node_list = node_list         # List of nodes participating in the peer sync
		self._num_edges = len(edge_list)    # Number of Edges
		self._num_nodes = len(node_list)    # Number of Nodes
		self._master = master               # Synchronization master

		# Setup loop matrix using Number of Independent Loops
		self._num_loops = self._num_edges - self._num_nodes + 1 		    # Number of linearly independent loops
		self._loop_matrix = np.zeros((self._num_loops, self._num_edges*2))	# Loop Matrix -> First num_edges cols are in same order as edge list, remaining are in the opp direction as edge_list
		self._recv_beta = np.zeros((2*self._num_edges,1))					# List of offsets computed per edge
		self._recv_alpha = np.zeros((2*self._num_edges,1))					# List of drifts computed per edge
		self._recv_start = np.zeros((2*self._num_edges,1))					# List of start times per edge
		self._recv_flags_dict = {}											# Flag indicating if data is received in that cycle
		self._preliminary_offsets = np.zeros((self._num_edges*2,1))			# Preliminary per edge estimated offsets
		self._preliminary_time = np.zeros((self._num_nodes,1))			    # Preliminary time at the nodes
		self._final_offsets = np.zeros((self._num_edges*2,1))				# Final per-edge computed offsets
		self._final_time = np.zeros((self._num_nodes,1))				    # Final time at the nodes
		
		# Create a map of nodes from names to numbers
		self._node_map = {}
		node_counter = 0
		for node in self._node_list:
			self._node_map[node] = node_counter
			node_counter += 1
		print(self._node_map)

		# Edge map -> maps node indice pair to the index of the edge 
		self._edge_map = {}
		edge_counter = 0
		for edge in edge_list:
			if self._node_map[edge[0]] not in self._edge_map:
				self._edge_map[self._node_map[edge[0]]] = {}
			self._edge_map[self._node_map[edge[0]]][self._node_map[edge[1]]] = edge_counter;
			edge_counter += 1

		for edge in edge_list:
			if self._node_map[edge[1]] not in self._edge_map:			
				self._edge_map[self._node_map[edge[1]]] = {}
			self._edge_map[self._node_map[edge[1]]][self._node_map[edge[0]]] = edge_counter;
			edge_counter += 1
		print("PeerSyncCompute is setup: " + "Master is " + str(self._master) + ", Period is " + str(self._period) + " seconds")

		# Initialize the condition variable
		self._condition = Condition()

	####### Private Functions #######

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

		@asyncio.coroutine
		def message_handler(msg):
			subject = msg.subject
			reply = msg.reply
			data = msg.data.decode()
			if debug == 1:
				print("Received a message on '{subject} {reply}': {data}".format(
				  subject=subject, reply=reply, data=data))

			# Parse the received JSON
			params = json.loads(data)

			# Add the latest parameters to the respective arrays
			server = params["server"]
			client = params["client"]
			self._condition.acquire()
			index1 = self._edge_map[self._node_map[client]][self._node_map[server]]
			self._recv_start[index1] = params["start_time"]
			self._recv_beta[index1] = params["offset"] - params["drift"]*self._recv_start[index1]
			self._recv_alpha[index1] =  params["drift"]
			index2 = self._edge_map[self._node_map[server]][self._node_map[client]]
			self._recv_start[index2] = params["start_time"] + params["offset"]
			self._recv_beta[index2] = -self._recv_beta[index1]/(1+params["drift"])
			self._recv_alpha[index2] = -params["drift"]/(1+params["drift"])

			if client == self._master:
				# Calculate the time at the master -> May need to change this
				self._preliminary_time[self._node_map[client]] = params["start_time"] + (self._period/2)*1000000000 # Midpoint of the interval

			# Update the flag dictionary
			self._recv_flags_dict[str(params["server"])+str(params["client"])] = 1

			if len(self._recv_flags_dict.keys()) == self._num_edges:
				self._condition.notify()

			self._condition.release()
			
		# Simple publisher and async subscriber via coroutine.
		params_subject = "qot.peer.params"
		sid = yield from self._nc.subscribe(params_subject, cb=message_handler)

		# Loop until the process unbinds from timeline
		self._offset_data = {}
		offset_topic = "qot.peer.offsets"
		while self._nats_thread_running:
			yield from asyncio.sleep(2, loop=self._loop)
			# Serialize and publish offsets
			if (bool(self._offset_data)):
				yield from self._nc.publish(offset_topic, json.dumps(self._offset_data).encode('utf-8'))

		# Disconnect from NATS Server
		yield from self._nc.close()

	def _dispatch_nats(self):
		''' Thread Handler to dispatch NATS '''
		# Run AsynIO Loop
		self._loop = asyncio.new_event_loop()
		asyncio.set_event_loop(self._loop)
		self._loop.run_until_complete(self._nats_run())
		# self._loop.run_forever()

	def _calculate_preliminary_offsets(self):
		''' Function to calculate the preliminary clock offsets '''
		# Set the preliminary time
		set_counter = 1 # The preliminary time for the master has already been set
		while set_counter < self._num_nodes:
			for edge in self._edge_list:
				index = self._edge_map[self._node_map[edge[0]]][self._node_map[edge[1]]]
				# Set the preliminary time if the "client" nodes time is computed
				if self._preliminary_time[self._node_map[edge[0]]] != 0 and self._preliminary_time[self._node_map[edge[1]]] == 0:
					self._preliminary_time[self._node_map[edge[1]]] = self._preliminary_time[self._node_map[edge[0]]]*(1 + self._recv_alpha[index]) + self._recv_beta[index] 
					set_counter += 1

		#print("Preliminary time is:")
		#print(self._preliminary_time)

		# Calculate the preliminary offsets
		for edge in self._edge_list:
			index1 = self._edge_map[self._node_map[edge[0]]][self._node_map[edge[1]]]
			self._preliminary_offsets[index1] = self._recv_alpha[index1]*self._preliminary_time[self._node_map[edge[1]]] + self._recv_beta[index1] 
			index2 = self._edge_map[self._node_map[edge[1]]][self._node_map[edge[0]]]
			self._preliminary_offsets[index2] = self._recv_alpha[index2]*self._preliminary_time[self._node_map[edge[0]]] + self._recv_beta[index2]
		
		#print("Preliminary offsets are:")
		#print(self._preliminary_offsets)

		return 0 

	def _calculate_final_offsets(self):
		''' Function to calculate the per-edge final clock offsets '''
		self._final_offsets = np.dot(self._pre_computed_mat, self._preliminary_offsets)
		return self._final_offsets

	def _calculate_final_time(self):
		''' Function to calculate the per node final clock time'''
		# Set the final time for the master (same as preliminary time)
		self._final_time[self._node_map[self._master]] = self._preliminary_time[self._node_map[self._master]]
		set_counter = 1 # The preliminary time for the master has already been set
		while set_counter < self._num_nodes:
			for edge in self._edge_list:
				index = self._edge_map[self._node_map[edge[0]]][self._node_map[edge[1]]]
				# Set the preliminary time if the "client" nodes time is computed
				if self._final_time[self._node_map[edge[0]]] != 0 and self._final_time[self._node_map[edge[1]]] == 0:
					#print(str(edge[1]) + " " + str(edge[0]) + " " + str(self._final_offsets[index]))					
					self._final_time[self._node_map[edge[1]]] = self._final_time[self._node_map[edge[0]]] + self._final_offsets[index] 
					set_counter += 1

		for node in self._node_list:
			self._offset_data[node] = {}
			self._offset_data[node]["offset"] = (self._final_time[self._node_map[node]][0]-self._preliminary_time[self._node_map[self._master]][0])/1000000000
			self._offset_data[node]["final time"] = self._final_time[self._node_map[node]][0]/1000000000

		print("Offsets & final time are:")
		print(self._offset_data)

		# Reset initial time to 0
		self._preliminary_time = np.zeros((self._num_nodes,1))
		self._final_time = np.zeros((self._num_nodes,1))

		return 0

	def _compute_loop_matrix(self):
		''' Function to precompute the loop matrix'''
		# Treat the graph as an undirected graph
		sync_graph = nx.Graph()
		for edge in self._edge_list:
			sync_graph.add_edge(self._node_map[edge[0]], self._node_map[edge[1]])
		print("Edges in Graph are: ")
		print (sync_graph.edges)

		# Compute the independent loops
		indepedent_loops = nx.cycle_basis(sync_graph, 0)

		# Populate the loop matrix
		loop_counter = 0
		for loop in indepedent_loops:
			edge_counter = 0
			for edge in loop:
				if edge_counter == len(loop) - 1:
					edge0 = edge
					edge1 = loop[0]
					index = self._edge_map[edge1][edge0]
					self._loop_matrix[loop_counter][index] = -1
				else:
					edge0 = edge
					edge1 = loop[edge_counter + 1]
					index = self._edge_map[edge0][edge1]
					self._loop_matrix[loop_counter][index] = 1
				edge_counter += 1
			loop_counter += 1

		return self._loop_matrix

	def _precompute_matrices(self):
		''' Function to Precompute the required matrices '''
		self._loop_matrix = self._compute_loop_matrix()
		print ("Loop Matrix is:")
		print (self._loop_matrix)
		temp_mat = np.dot(self._loop_matrix,np.transpose(self._loop_matrix))
		try:
			temp_mat_inv = np.linalg.inv(temp_mat)
		except numpy.linalg.LinAlgError:
			print("Error: Matrix not invertible")
			return -1
		else:
			self._pre_computed_mat = np.eye(2*self._num_edges) - np.dot(np.dot(np.transpose(self._loop_matrix),temp_mat_inv), self._loop_matrix)
			return 0

	def _dispatch_offset_calc(self):
		''' Thread Handler to dispatch function which calculates the final offsets '''
		topic = "qot.peer.offsets"
		offset_data ={}
		while self._nats_thread_running:
			self._condition.acquire()
			# Wait on conditionn variable
			self._condition.wait()

			if self._nats_thread_running == False:
				self._condition.release()
				break

			#print("******Calculcating preliminary offsets******")
			self._calculate_preliminary_offsets()
			self._calculate_final_offsets()
			self._calculate_final_time()

			# Reinitialize the dictionary
			self._recv_flags_dict = {}
			self._condition.release()

			if debug == 1:
				print("******Final Offsets Calculcated******")
				print(self._final_offsets)

	####### Public API Calls #######
		
	def start(self: object):
		'''
		* @brief start the peer sync processor
		* @return A status code indicating success (0) or other'''

		print("PeerComputeServer: Starting ... ")
		# Precompute the Matrices required
		retval = self._precompute_matrices()
		if retval < 0:
			print ("Terminating as matrix computation threw an error")
			return -1;

		# Start a new thread for NATS which runs AsyncIO
		self._nats_thread_running = True # Flag to terminate thread
		self._nats_thread = Thread(target=self._dispatch_nats)
		self._nats_thread.start()

		# Start the processing thread
		self._proc_thread = Thread(target=self._dispatch_offset_calc)
		self._proc_thread.start()

		return 0

	def stop(self: object):
		'''
		* @brief stop the peer sync processor
		* @return A status code indicating success (0) or other'''
		print("PeerComputeServer: Exiting")
		# NATS & Processing Thread Running Flag is unset
		self._nats_thread_running = False 

		# NATS Thread Join
		self._nats_thread.join()

		print("PeerComputeServer: NATS Thread joined")

		# Signal the Processing thread to wake up
		self._condition.acquire()
		self._condition.notify()
		self._condition.release()

		# Processing Thread join
		self._proc_thread.join()

		print("PeerComputeServer: Processor Thread joined")

		# Close the AsyncIO Loop
		self._loop.close()

		print("PeerComputeServer: Exited cleanly")

		return 0
		
# Code to test this library
if __name__ == '__main__':
	# Register signal handler
	signal.signal(signal.SIGINT, signal_handler)

	parser = argparse.ArgumentParser(description='PeerComputeServer to calculate offsets')
	parser.add_argument('--nats_server', '-n', default="nats://192.168.1.115:32369", type=str, help='url of nats server to connect to')
	parser.add_argument('--master_clock', '-m', default="192.168.1.115", type=str, help='hostname of the master clock')
	parser.add_argument('--period', '-p', default=2, type=float, help='the period over which data is processed')
	parser.add_argument('--config', '-c', default="/opt/qot-stack/doc/topology_example.json", type=str, help='topology configuration file')

	args = parser.parse_args()

	# Node List & Edge List
	node_list = []
	edge_list = []

	# Read the file and load the json
	with open(args.config) as f:
		config_data = json.load(f)
		node_list = config_data["nodes"]
		edge_list = config_data["edges"]

	# Setup the PeerSyncCompute Class
	# node_list = ["192.168.1.115", "192.168.1.116", "192.168.1.117", "192.168.1.118"]
	# edge_list = [["192.168.1.115","192.168.1.116"],["192.168.1.116","192.168.1.117"],["192.168.1.117","192.168.1.118"],["192.168.1.118","192.168.1.115"]]
	peer_sync = PeerSyncCompute(edge_list, node_list, args.master_clock, args.period, [args.nats_server])

	# Start Peer Processor
	peer_sync.start()

	# Stall for some time
	while running:
		time.sleep(1)

	# Stop Peer Processor
	peer_sync.stop()
	

