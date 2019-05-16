# @file timelines.py
# @brief QoT Coordination Service REST API for NTP Sync Servers
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

import logging

from flask import request
from flask_restplus import Resource
from coordination_service.api.coordinator.business import register_server, delete_server
from coordination_service.api.coordinator.serializers import server
from coordination_service.api.restplus import api
from coordination_service.database.models import Server

log = logging.getLogger(__name__)

ns = api.namespace('service/servers', description='Operations to register/remove NTP servers')

@ns.route('/')
class ServerCollection(Resource):

    @api.marshal_list_with(server)
    def get(self):
        """
        Returns list of active servers.
        """
        servers = Server.query.all()
        return servers

    @api.response(201, 'Server successfully registered.')
    @api.expect(server)
    def post(self):
        """
        Register a new server.
        """
        data = request.json
        register_server(data)
        return None, 201


@ns.route('/<string:server_name>')
@api.response(404, 'Server not found.')
class ServerItem(Resource):

    @api.marshal_with(server)
    def get(self, server_name):
        """
        Returns a timeline with a list of nodes.
        """
        return Server.query.filter(Server.name == server_name).one()


    @api.response(204, 'Server successfully deleted.')
    def delete(self, server_name):
        """
        Deletes a timeline.
        """
        delete_server(server_name)
        return None, 204

