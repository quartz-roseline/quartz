# @file qot_types.py
# @brief QoT Stack Python Data Types and Basic Operations
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

# Define keyword prefix for defining a global timeline
GLOBAL_TL_STRING = "gl_"

# Default Maximum Number of Timelines Supported 
MAX_TIMELINES = 5

# For ease of conversion 
ASEC_PER_NSEC = 1000000000
ASEC_PER_USEC = 1000000000000
ASEC_PER_MSEC = 1000000000000000
SEC_PER_SEC   = 1
mSEC_PER_SEC  = 1000
uSEC_PER_SEC  = 1000000
nSEC_PER_SEC  = 1000000000
pSEC_PER_SEC  = 1000000000000
fSEC_PER_SEC  = 1000000000000000
aSEC_PER_SEC  = 1000000000000000000

# Operations on lengths of time #

# An absolute length time 
class TimeLength:
	def __init__(self, sec, asec):
		self.sec = sec
		self.asec = asec

# A single point in time with respect to some reference
class TimePoint:
	def __init__(self, sec, asec):
		self.sec = sec
		self.asec = asec

# An interval of time
class TimeInterval:
	def __init__(self, below, above):
		self.below = below # fractional seconds
		self.above = above # fractional seconds

# An point of time with an interval of uncertainty
class UTimePoint:





