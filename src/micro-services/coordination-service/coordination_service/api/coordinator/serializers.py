# @file serializers.py
# @brief QoT Coordination Service JSON Description for Serialization
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

from flask_restplus import fields
from coordination_service.api.restplus import api

node = api.model('Physical Node', {
    'id': fields.Integer(readOnly=True, description='The unique identifier of the node'),
    'name': fields.String(required=True, description='The unique name of the node'),
    'accuracy': fields.Integer(required=True, attribute='The accuracy required by the node'),
    'resolution': fields.Integer(required=True, attribute='The resolution required by the node'),
    'ip': fields.String(required=False, attribute='The IP address of the node'),
    'timeline_name': fields.String(attribute='timeline.name'),
})

timeline = api.model('QoT Timeline', {
    'id': fields.Integer(readOnly=True, description='The unique identifier of the timeline'),
    'name': fields.String(required=True, description='Timeline unique name'),
    'meta_data': fields.String(required=False, description='Timeline meta-data (domain for PTP)'),
})

timeline_with_nodes = api.inherit('Timeline with nodes', timeline, {
    'nodes': fields.List(fields.Nested(node))
})

qot = api.model('Quality of Time', {
    'accuracy': fields.Integer(required=True, description='Desired accuracy in nanoseconds'),
    'resolution': fields.Integer(required=True, description='Desired resolution in nanoseconds'),
})

timeline_num_nodes = api.model('Number of active nodes on the timeline', {
    'num_nodes': fields.Integer(required=True, description='Number of active nodes on the timeline'),
})

server = api.model('NTP Server Credentials', {
    'name': fields.String(required=True, description='The IP at which the NTP server is accesible'),
    'stratum': fields.Integer(required=True, attribute='The stratum of the NTP server'),
    'server_type': fields.String(required=True, description='Field indicating if the server is globally accesible (global or local)'),
})