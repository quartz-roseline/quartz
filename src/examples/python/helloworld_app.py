# @file helloworld_app.py
# @brief Helloworld QoT Python Applications
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

sys.path.append(os.path.abspath("/usr/local/lib"))
from qot_coreapi import TimelineBinding
from qot_coreapi import ReturnTypes

import signal

# Global Variable used to terminate program on SIGINT
running = 1

# SIGINT signal handler
def signal_handler(signal, frame):
	print('Program Exiting')
	global running
	running = 0

def main_func(timeline_uuid: str, app_name: str):

	# Register signal handler
	signal.signal(signal.SIGINT, signal_handler)

	print("Hello World !")

	# Bind to the timeline
	binding = TimelineBinding("app")
	retval = binding.timeline_bind(timeline_uuid, app_name, 1000, 1000)
	if retval != ReturnTypes.QOT_RETURN_TYPE_OK:
		print ('Unable to bind to timeline, terminating ....')
		exit (1)

	# Set the Period and Offset (1 second and 0 ns repectively)
	binding.timeline_set_schedparams(1000000000, 0)

	while running:
		# Read a Timeline Timestamp
		tl_time = binding.timeline_gettime()
		print('Timeline time is                %f' % tl_time["time_estimate"])
		print('Upper Bound is                  %f' % tl_time["interval_above"])
		print('Lower Bound is                  %f' % tl_time["interval_below"])
		# Wait until the next period
		binding.timeline_waituntil_nextperiod()

	# Unbind from the timeline
	print("Unbinding from timeline")
	binding.timeline_unbind()


if __name__ == '__main__':
	parser = argparse.ArgumentParser(description='Helloworld Python QoT App')
	parser.add_argument('--timeline', '-t', default='gl_my_test_timeline', type=str, help='name of timeline to bind to')
	parser.add_argument('--app', '-a', default='qot_app', type=str, help='name of app component')
	args = parser.parse_args()
	main_func(args.timeline, args.app)