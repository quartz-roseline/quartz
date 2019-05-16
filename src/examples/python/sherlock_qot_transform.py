# @file helloworld.py
# @brief Sherlock QoT Python Transform
# @author Anon D'Anon
#
# Copyright (c) Anon, 2018.
# Copyright (c) Anon Inc., 2018.
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

# System calls
import sys 

# Argument Parsing 
import argparse

# Signal Handlers
import signal

# Module to Read Core Time
import time

# For Type Checking
import inspect

# For Spawning Threads and using Mutex Locks
from threading import Thread, Lock

# JSON Serialization/De-serialization
import json

# Import AsyncIO
import asyncio

# Import Socket Library
import socket

# Import the Math Library
import math

# Import Array Library
import array

# Import OS functionality
import os

# Share Memory
import mmap

# Import C-Style Structs
import struct

# NATS Python 3 Client
from nats.aio.client import Client as NATS
from nats.aio.errors import ErrConnectionClosed, ErrTimeout, ErrNoServers

# Enum Type
from enum import Enum

# Hard coded maximum ring buffer size
MAX_RING_BUFFER_SIZE = 10

# Timeline Socket Path
TL_SOCKET_PATH = "/tmp/qot_timeline"

# Global Variable used to indicate if the binding has been initialized
initialized = False

class ReturnTypes(Enum):
	""" class that implements return types as codes """
	QOT_RETURN_TYPE_OK = 0					  # Return Type OK
	QOT_RETURN_TYPE_ERR = 1					  # Return Type generic error
	QOT_RETURN_TYPE_CONN_ERR = 2			  # Connection error to timeline service

	def __int__(self):
		return self.value

class TimelineMessageTypes(Enum):
	""" qot timeline service message codes """
	TIMELINE_CREATE         = 0               # Create a timeline                        
	TIMELINE_DESTROY        = 1               # Destroy a timeline                       
	TIMELINE_UPDATE         = 2               # Update timeline binding parameters       
	TIMELINE_BIND           = 3               # Bind to a timeline                       
	TIMELINE_UNBIND         = 4               # Unbind from a timeline                   
	TIMELINE_QUALITY        = 5               # Get the QoT Spec for this timeline       
	TIMELINE_INFO           = 6               # Get the timeline info                    
	TIMELINE_SHM_CLOCK      = 7               # Get the timeline clock rd-only shm fd    
	TIMELINE_SHM_CLKSYNC    = 8               # Get the timeline clock shm fd            
	TIMELINE_UNDEFINED      = 9               # Undefined function   

	def __int__(self):
		return self.value

class TimelineTypes(Enum):
	""" Timeline Types enumerated class """
	QOT_TIMELINE_LOCAL  = 0				      # Local Timeline -> Internal or External Reference
	QOT_TIMELINE_GLOBAL = 1                   # Global Timeline -> Tied to UTC

	def __int__(self):
		return self.value

class RingBuffer:
	""" class that implements a not-yet-full buffer """
	def __init__(self,size_max):
		self.max = size_max
		self.data = []
		self.mutex = Lock()

	class __Full:
		""" class that implements a full buffer """
		def append(self, x):
			""" Append an element overwriting the oldest one. """
			self.mutex.acquire()
			try:
				self.data[self.cur] = x
				self.cur = (self.cur+1) % self.max
			finally:
				self.mutex.release()

		def get(self):
			""" return list of elements in correct order """
			self.mutex.acquire()
			try:
				ret_data = self.data[self.cur:]+self.data[:self.cur]
			finally:
				self.mutex.release()
			return ret_data

	def append(self,x):
		"""append an element at the end of the buffer"""
		print("Added data to ring buffer")
		self.mutex.acquire()
		try:
			self.data.append(x)
			if len(self.data) == self.max:
				self.cur = 0
				# Permanently change self's class from non-full to full
				self.__class__ = self.__Full
		finally:
			self.mutex.release()

	def get(self):
		""" Return a list of elements from the oldest to the newest. """
		self.mutex.acquire()
		try:
			ret_data = self.data
		finally:
			self.mutex.release()

		return ret_data

class TimelineBinding:
	"""TimelineBinding class lets apps bind to a timeline and perform various 
	   time-related operations on the timeline"""

	def __init__(self, mode="app"):
		# Set the Mode as "transform" or "app"
		self._mode = mode

		# Initialize Timeline Parameters
		self._timeline_uuid = None          # Timeline UUID (Unique name)
		self._timeline_type = "local"       # Timeline type (local or global)
		self._timeline_index = -1              # Timeline ID
		
		# Initialize Binding Parameters
		self._binding_name  = None          # Binding Name
		self._binding_id = -1               # Binding ID
		self._accuracy_ns = None            # Accuracy Specification in nanoseconds
		self._resolution_ns = None          # Resolution Specification in nanoseconds

		# Initialize Scheduling Parameters
		self._offset_ns = 0                 # Scheduling Offset
		self._period_ns = 0                 # Scheduling Period

		# Initialize AsyncIO Loop
		if mode == "transform":
			self._loop = asyncio.get_event_loop()
		# Else: mode defaults to "app"

	####### Private Functions #######

	# Decorator 1 to Perform Type Checking
	def checkargs(function):
		def _f(*arguments):
			for index, argument in enumerate(inspect.getfullargspec(function)[0]):
				if not isinstance(arguments[index], function.__annotations__[argument]):
					raise TypeError("{} is not of type {}".format(arguments[index], function.__annotations__[argument]))
			return function(*arguments)
		_f.__doc__ = function.__doc__
		return _f

	# Decorator 2 to Perform Type Checking
	def coerceargs(function):
		def _f(*arguments):
			new_arguments = []
			for index, argument in enumerate(inspect.getfullargspec(function)[0]):
				new_arguments.append(function.__annotations__[argument](arguments[index]))
			return function(*new_arguments)
		_f.__doc__ = function.__doc__
		return _f

	def _recv_fds(self, msglen, maxfds):
		fds = array.array("i")   # Array of ints
		msg, ancdata, flags, addr = self._sock.recvmsg(msglen, socket.CMSG_LEN(maxfds * fds.itemsize))
		for cmsg_level, cmsg_type, cmsg_data in ancdata:
			if (cmsg_level == socket.SOL_SOCKET and cmsg_type == socket.SCM_RIGHTS):
				# Append data, ignoring any truncated integers at the end.
				fds.fromstring(cmsg_data[:len(cmsg_data) - (len(cmsg_data) % fds.itemsize)])
		return msg, list(fds)

	def _send_timeline_msg(self):
		'''Send a request to the QoT Timeline Service over a UDP socket'''

		# Convert message to JSON
		message = json.dumps(self._tl_msg)

		# Send message to timeline service
		bytesSent = self._sock.send(message.encode())

		# Maximum message size to receive in one go
		MAX_BUF_LEN = 4096

		# Received Message
		msg_recv = ""
		recv_flag = False

		# Wait for a response from the timeline service
		if bytesSent > 0 and self._tl_msg["msgtype"] != int(TimelineMessageTypes.TIMELINE_SHM_CLOCK):
			amount_received = MAX_BUF_LEN
			while amount_received == MAX_BUF_LEN:
				data = self._sock.recv(MAX_BUF_LEN).decode()
				amount_received = len(data)
				msg_recv = msg_recv + data
				recv_flag = True

			# Possible error in receiving message
			if recv_flag == False:
				print ('Could not receive data from service')
				return ReturnTypes.QOT_RETURN_TYPE_CONN_ERR

			# Decode Message from JSON
			self._tl_msg = json.loads(msg_recv)
			return self._tl_msg["retval"]
		elif bytesSent == 0:
			print ('Failed to send message to service')
			self._tl_msg["retval"] = int(ReturnTypes.QOT_RETURN_TYPE_CONN_ERR)
			return self._tl_msg["retval"]
		else: # Message request to get clock shared memory
			self._tl_msg["retval"] = int(ReturnTypes.QOT_RETURN_TYPE_OK)
			return self._tl_msg["retval"]

		return ReturnTypes.QOT_RETURN_TYPE_ERR

	def _populate_timeline_msg_data(self):
		'''Populate the fields of the timeline message based on the instance parameters'''
		self._tl_msg = dict()

		# Timeline Information
		self._tl_msg["info"] =dict()
		self._tl_msg["info"]["index"] = self._timeline_index
		self._tl_msg["info"]["type"] = int(self._tl_type)
		self._tl_msg["info"]["name"] = self._timeline_uuid

		# Binding Information
		self._tl_msg["binding"] = dict()
		self._tl_msg["binding"]["name"] = self._binding_name
		self._tl_msg["binding"]["id"] = self._binding_id;

		# QoT Requirements
		self._tl_msg["demand"] = dict()
		self._tl_msg["demand"]["resolution"] = dict()
		self._tl_msg["demand"]["resolution"]["sec"] = int(math.floor(self._resolution_ns/1000000000))
		self._tl_msg["demand"]["resolution"]["asec"] = int((self._resolution_ns % 1000000000)*1000000000)
		self._tl_msg["demand"]["accuracy"] = dict()
		self._tl_msg["demand"]["accuracy"]["above"] = dict()
		self._tl_msg["demand"]["accuracy"]["below"] = dict()
		self._tl_msg["demand"]["accuracy"]["above"]["sec"] = int(math.floor(self._accuracy_ns/1000000000))
		self._tl_msg["demand"]["accuracy"]["above"]["asec"] = int((self._accuracy_ns % 1000000000)*1000000000)
		self._tl_msg["demand"]["accuracy"]["below"]["sec"] = int(math.floor(self._accuracy_ns/1000000000))
		self._tl_msg["demand"]["accuracy"]["below"]["asec"] = int((self._accuracy_ns % 1000000000)*1000000000)

	def _populate_timeline_msg_type(self, msg_type: TimelineMessageTypes):
		'''Populate the message type of the timeline message'''
		# Message Type
		self._tl_msg["msgtype"] = int(msg_type)

		 # Return Code
		self._tl_msg["retval"] = int(ReturnTypes.QOT_RETURN_TYPE_ERR)


	def _nats_subscribe(self):
		'''Run the NATS Subscriber '''
		self._nc = NATS()

		# Connect to the NATS Server
		yield from self._nc.connect(servers=["nats://nats.default.svc.cluster.local:4222"], io_loop=self._loop)

		@asyncio.coroutine
		def message_handler(msg):
			subject = msg.subject
			reply = msg.reply
			data = msg.data.decode()
			print("Received a message on '{subject} {reply}': {data}".format(
			  subject=subject, reply=reply, data=data))

			# Parse the received JSON
			params = json.loads(data)

			# Add the latest parameters to the ring buffer
			self._param_ring_buf.append(params)

		# Simple publisher and async subscriber via coroutine.
		tl_subject = "qot." + "timeline." + self._timeline_uuid + ".params"
		sid = yield from self._nc.subscribe(tl_subject, cb=message_handler)

		# Loop until the process unbinds from timeline
		while self._nats_thread_running:
			yield from asyncio.sleep(1, loop=self._loop)

		# Disconnect from NATS Server
		yield from self._nc.close()

	def _dispatch_nats(self):
		''' Thread Handler to dispatch NATS Subscriber'''
		# Create a Ring Buffer of Clock Parameters
		self._param_ring_buf = RingBuffer(MAX_RING_BUFFER_SIZE) 

		# Append initial value to ring buffer
		init_params = {"l_mult":0,"l_nsec":0,"last":0,"mult":0,"nsec":0,"u_mult":0,"u_nsec":0}
		self._param_ring_buf.append(init_params)

		# Run AsynIO Loop
		self._loop.run_until_complete(self._nats_subscribe())

	def _find_clkparams(self, coretime):
		'''Find the appropriate clock parameters based on the core time'''
		# Get the list of stored parameters
		param_list = self._param_ring_buf.get()

		# Parse the param list to find the appropriate clock parameters
		for list_params in reversed(param_list):
			# Compare with "last" after converting "last" to fractional seconds
			if coretime > list_params["last"]/1000000000:
				params = list_params
				break

		return params

	def _core2timeline(self, period_flag, coretime_ns, clk_params):
		'''Convert from core time to timeline time'''
		if period_flag:
			tl_time = coretime_ns + int(clk_params[1]*coretime_ns)/1000000000;
		else:
			tl_time = coretime_ns - clk_params[0];
			tl_time  = clk_params[2] + tl_time + (int(clk_params[1]*tl_time)/1000000000);
		
		return tl_time

	def _timeline2core(self, period_flag, tltime_ns, clk_params):
		'''Convert from timeline time to core time'''
		if period_flag:
			core_time = int(tltime_ns*1000000000)/(clk_params[1]+1000000000)
		else:
			diff = tltime_ns - clk_params[2]
			quot = int(diff*1000000000)/(clk_params[1]+1000000000)
			core_time = clk_params[0] + quot; 
		return core_time


	def _compute_qot(self, coretime_ns, clk_params):
		'''Compute the QoT estimates for the timestamp'''
		upper_bound = int(clk_params[5]*(coretime_ns - clk_params[0]))/1000000000 + clk_params[3]
		lower_bound = int(clk_params[6]*(coretime_ns - clk_params[0]))/1000000000 + clk_params[4]

		return upper_bound, lower_bound

	####### Public API Calls #######
		
	@checkargs
	def timeline_bind(self: object, timeline_uuid: str, app_name: str, res_ns: int, acc_ns: int):
		'''
		* @brief Bind to a timeline with a given resolution and accuracy
		* @param timeline_uuid Name of the timeline
		* @param app_name Name of this binding
		* @param res_ns Maximum tolerable unit of time in nanoseconds
		* @param acc_ns Maximum tolerable deviation from true time in nanoseconds
		* @return A status code indicating success (0) or other'''
		print("Binding to timeline %s" % timeline_uuid)
		self._timeline_uuid = timeline_uuid
		self._binding_name = app_name
		self._resolution_ns = res_ns
		self._accuracy_ns = acc_ns
		self._timeline_index = 0
		self._binding_id = -1

		# Timeline type
		if timeline_uuid.find("gl_") == 0:
			self._tl_type = TimelineTypes.QOT_TIMELINE_GLOBAL
		else:
			self._tl_type = TimelineTypes.QOT_TIMELINE_LOCAL

		# Return Value
		retval = ReturnTypes.QOT_RETURN_TYPE_OK

		if self._mode == "transform":
			# Start a new thread for NATS which runs AsyncIO
			self._nats_thread_running = True # Flag to terminate thread
			self._nats_thread = Thread(target=self._dispatch_nats)
			self._nats_thread.start()
		else:	# Assume App Mode
			# Create a UDS socket
			self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

			# Connect the socket to the port where the qot-timeline-service is listening
			try:
				self._sock.connect(TL_SOCKET_PATH)
			except socket.error as msg:
				print (msg)
				retval = ReturnTypes.QOT_RETURN_TYPE_CONN_ERR
				return retval

			# Initialize Timeline service message
			self._populate_timeline_msg_data()

			# Send TIMELINE_CREATE message to timeline service
			self._populate_timeline_msg_type(TimelineMessageTypes.TIMELINE_CREATE)
			if self._send_timeline_msg() != int(ReturnTypes.QOT_RETURN_TYPE_OK):
				print ('Failed to send timeline metadata to timeline service')
				retval = ReturnTypes.QOT_RETURN_TYPE_ERR
				return retval
			else:
				self._timeline_index = self._tl_msg["info"]["index"]
				print ('Timeline ID is %d' % self._timeline_index)

			# Send TIMELINE_BIND message to timeline service
			self._populate_timeline_msg_data()
			self._populate_timeline_msg_type(TimelineMessageTypes.TIMELINE_BIND)
			if self._send_timeline_msg() != int(ReturnTypes.QOT_RETURN_TYPE_OK):
				print ('Failed to send timeline metadata to timeline service')
				retval = ReturnTypes.QOT_RETURN_TYPE_ERR
				return retval
			else:
				self._binding_id = self._tl_msg["binding"]["id"]
				print ('Binding ID is %d' % self._binding_id)

			# Send TIMELINE_SHM_CLOCK message to timeline service
			self._populate_timeline_msg_data()
			self._populate_timeline_msg_type(TimelineMessageTypes.TIMELINE_SHM_CLOCK)
			if self._send_timeline_msg() != int(ReturnTypes.QOT_RETURN_TYPE_OK):
				print ('Failed to request timeline clock from timeline service')
				retval = ReturnTypes.QOT_RETURN_TYPE_ERR
				return retval
			else:
				# Get the timeline clock parameter file descriptor from the timeline service
				msg, shm_fd_list = self._recv_fds(20, 1)
				shm_fd = shm_fd_list[0]

				# Memory map the parameters into a byte array
				self._clk_params = mmap.mmap(shm_fd, 0, flags=mmap.MAP_SHARED, prot=mmap.PROT_READ)

		print("Bound to timeline %s" % timeline_uuid)
		return retval

	@checkargs
	def timeline_unbind(self: object):
		'''
		* @brief Unbind from a timeline
		* @return A status code indicating success (0) or other'''
		
		if self._mode == "transform":
			# NATS Thread Join
			self._nats_thread_running = False
			self._nats_thread.join()

			# Close the AsyncIO Loop
			self._loop.close()
		else:
			# Send TIMELINE_BIND message to timeline service
			self._populate_timeline_msg_type(TimelineMessageTypes.TIMELINE_UNBIND)
			if self._send_timeline_msg() != int(ReturnTypes.QOT_RETURN_TYPE_OK):
				print ('Failed to send unbing message to timeline service')
				retval = ReturnTypes.QOT_RETURN_TYPE_ERR
				return retval
			else:
				print ('Unbound from timeline service')

		return ReturnTypes.QOT_RETURN_TYPE_OK

	@checkargs
	def timeline_get_accuracy(self: object):
		'''
		* @brief Get the accuracy requirement associated with this binding
		* @return acc Maximum tolerable deviation from true time in nanoseconds'''
		return self._accuracy_ns

	@checkargs
	def timeline_get_resolution(self: object):
		'''
		* @brief Get the resolution requirement associated with this binding
		* @return res Maximum tolerable unit of time in nanoseconds'''
		return self._resolution_ns

	@checkargs
	def timeline_get_name(self: object):
		'''
		* @brief Query the name of this application
		* @return name Application name'''
		return self._binding_name

	@checkargs
	def timeline_get_uuid(self: object):
		'''
		* @brief Query the name of this timeline
		* @return name Timeline name'''
		return self._timeline_uuid

	@checkargs
	def timeline_set_accuracy(self: object, acc_ns: int):
		'''
		* @brief Set the accuracy requirement associated with this binding
		* @param acc Maximum tolerable deviation from true time in nanoseconds
		* @return A status code indicating success (0) or other'''
		self._accuracy_ns = acc_ns
		if self._mode != "transform":
			# Send TIMELINE_UPDATE message to timeline service
			self._populate_timeline_msg_data()
			self._populate_timeline_msg_type(TimelineMessageTypes.TIMELINE_UPDATE)
			if self._send_timeline_msg() != int(ReturnTypes.QOT_RETURN_TYPE_OK):
				print ('Failed to send set accuracy to timeline service')
				retval = ReturnTypes.QOT_RETURN_TYPE_ERR
				return retval
		return ReturnTypes.QOT_RETURN_TYPE_OK

	@checkargs
	def timeline_set_resolution(self: object, res_ns: int):
		'''
		* @brief Set the resolution requirement associated with this binding
		* @param res Maximum tolerable unit of time in nanoseconds
		* @return A status code indicating success (0) or other'''
		self._resolution_ns = res_ns
		if self._mode != "transform":
			# Send TIMELINE_UPDATE message to timeline service
			self._populate_timeline_msg_data()
			self._populate_timeline_msg_type(TimelineMessageTypes.TIMELINE_UPDATE)
			if self._send_timeline_msg() != int(ReturnTypes.QOT_RETURN_TYPE_OK):
				print ('Failed to send set accuracy to timeline service')
				retval = ReturnTypes.QOT_RETURN_TYPE_ERR
				return retval
		return ReturnTypes.QOT_RETURN_TYPE_OK

	@checkargs
	def timeline_get_coretime(self: object):
		'''
		* @brief Query the time according to the core
		* @return core_now Estimated time in fractional seconds
		Note: Core Clock is CLOCK_REALTIME'''
		return time.clock_gettime(time.CLOCK_REALTIME)

	@checkargs
	def timeline_gettime(self: object):
		'''
		* @brief Query the time according to the timeline
		* @return est Estimated timeline time in fractional seconds with uncertainty'''

		# Read the CLOCK_REALTIME core time
		core_now_ns = int(math.floor(time.clock_gettime(time.CLOCK_REALTIME)*1000000000))

		# Unpack the Memory-mapped Clock Parameters
		params = struct.unpack('@qqqqqqq', self._clk_params)

		tl_time = dict()
		# Convert from core time to timeline time
		tl_time["time_estimate"] = float(self._core2timeline(False, core_now_ns, params))/1000000000

		# Add the Uncertainty
		tl_time["interval_above"], tl_time["interval_below"] = self._compute_qot(core_now_ns, params)
		tl_time["interval_above"] = float(tl_time["interval_above"])/1000000000
		tl_time["interval_below"] = float(tl_time["interval_below"])/1000000000

		# Return Timestamp, Upper Bound, Lower Bound 
		return tl_time 

	@checkargs
	def timeline_set_schedparams(self: object, period_ns: int, offset_ns: int):
		'''
		* @brief Set the periodic scheduling parameters requirement associated with this binding
		* @param start_offset First wakeup time
		* @param period wakeup period
		* @return A status code indicating success (0) or other'''
		self._offset_ns = offset_ns
		self._period_ns = period_ns
		return 0

	@checkargs
	def timeline_waituntil(self: object, abs_time: float):
		'''
		* @brief Block wait until a specified uncertain point
		* @param abs_time the absolute fractional time to wake up at
		* @return Time at which the program resumes'''
		# Unpack the Memory-mapped Clock Parameters
		params = struct.unpack('@qqqqqqq', self._clk_params)

		# Translate timeline time duration to core time
		core_duration = float(self._timeline2core(False, int(abs_time*1000000000), params))/1000000000

		# Sleep for the duration
		time.sleep(core_duration)
		return self.timeline_gettime() # Needs to be fleshed out

	@checkargs
	def timeline_waituntil_nextperiod(self: object):
		'''
		* @brief Block and wait until next period
		* @return utp Returns the actual uncertain wakeup time'''
		return self.timeline_gettime() # Needs to be fleshed out

	@checkargs
	def timeline_sleep(self: object, rel_time: float):
		'''
		* @brief Block for a specified length of uncertain time
		* @param rel_time fractional seconds time to sleep for
		* @return A status code indicating success (0) or other'''
		# Unpack the Memory-mapped Clock Parameters
		params = struct.unpack('@qqqqqqq', self._clk_params)

		# Translate timeline time duration to core time
		core_duration = float(self._timeline2core(True, int(rel_time*1000000000), params))/1000000000

		# Sleep for the duration
		time.sleep(core_duration)
		return self.timeline_gettime() # Needs to be fleshed out

	@checkargs
	def timeline_core2rem(self: object, core_time: float):
		'''
		* @brief Converts core time to remote timeline time
		* @param core_time time to be converted in nanoseconds
		* @return A status code indicating success (0) or other'''
		tl_time = dict()

		# Find the appropriate clock parameters
		if self._mode == "transform":
			clk_params = self._find_clkparams(core_time)

			# Translate core time to timeline time
			tl_time["time_estimate"] = clk_params["nsec"] + (core_time*1000000000 - clk_params["last"]) + ((core_time*1000000000 - clk_params["last"])*(clk_params["mult"]))/1000000000
			tl_time["time_estimate"] = float(tl_time["time_estimate"])/1000000000

			# Add the uncertainty
			tl_time["interval_above"] = float((clk_params["u_mult"]*(core_time*1000000000 - clk_params["last"]))/1000000000 + clk_params["u_nsec"])/1000000000
			tl_time["interval_below"] = float((clk_params["l_mult"]*(core_time*1000000000 - clk_params["last"]))/1000000000 + clk_params["l_nsec"])/1000000000
		else:
			# Unpack the Memory-mapped Clock Parameters
			params = struct.unpack('@qqqqqqq', self._clk_params)
			
			# Translate core time to timeline time
			tl_time["time_estimate"] = float(self._core2timeline(False, int(core_time*1000000000), params))/1000000000

			# Add the uncertainty
			tl_time["interval_above"], tl_time["interval_below"] = self._compute_qot(int(core_time*1000000000), params)
			tl_time["interval_above"] = float(tl_time["interval_above"])/1000000000
			tl_time["interval_below"] = float(tl_time["interval_below"])/1000000000


		return tl_time
		
	@checkargs
	def timeline_rem2core(self: object, tl_time: float):
		'''
		* @brief Converts remote timeline time to core time
		* @param tl_time time to be converted in nanoseconds
		* @return core_time tl_time translated to core time'''
		# Unpack the Memory-mapped Clock Parameters
		params = struct.unpack('@qqqqqqq', self._clk_params)

		# Translate timeline time duration to core time
		core_time= float(self._timeline2core(False, int(rel_time*1000000000), params))/1000000000
		return core_time

# Global Binding Class
binding = TimelineBinding("transform")

def init_transform(timeline_uuid: str, app_name: str):
	print("Initializing transform ...")

	# Bind to the timeline
	global binding
	binding.timeline_bind(timeline_uuid, app_name, 1000, 1000)
	return

# Tranformation Main function invoked by Sherlock
def main(ctx,msg):

	global binding
	global initialized
	if initialized == False:
		# Initialize the Timeline Binding
		init_transform('my_test_timeline', "qot_transform")
		initialized = True

	# Get the provided core time
	coretime = float(msg)
	sherlock_time = float(ctx._Context__msg.timestamp)/1000000000
	print('---------------------------------------------------')
	print('Received Timestamp from Sensor   %f' % coretime)
	print('Received Timestamp from Sherlock %f' % sherlock_time)
	# Translate to Timeline Time
	tl_time = binding.timeline_core2rem(coretime)
	print('Translated Timeline time is      %f' % tl_time["time_estimate"])
	print('Upper Uncertainty bound is       %f' % tl_time["interval_above"])
	print('Lower Uncertainty bound is       %f' % tl_time["interval_below"])
	print('\n')
	
	# Send the data to the next stage
	ctx.send(json.dumps(tl_time).encode('utf-8'))
	return
