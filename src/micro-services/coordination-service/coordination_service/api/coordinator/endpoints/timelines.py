# @file timelines.py
# @brief QoT Coordination Service REST API for Timelines
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
from coordination_service.api.coordinator.business import create_timeline, delete_timeline, update_timeline_metadata, create_node, updateqot_node, delete_node, register_timeline_server, delete_timeline_server, get_remote_timeline_server
from coordination_service.api.coordinator.serializers import timeline, timeline_with_nodes, qot, timeline_num_nodes, node, server
from coordination_service.api.restplus import api
from coordination_service.database.models import Timeline, Node, TimelineServer

log = logging.getLogger(__name__)

ns = api.namespace('service/timelines', description='Operations on QoT Timelines')

@ns.route('/')
class TimelineCollection(Resource):

    @api.marshal_list_with(timeline)
    def get(self):
        """
        Returns list of active timelines.
        """
        timelines = Timeline.query.all()
        return timelines

    @api.response(201, 'Timeline successfully created.')
    @api.expect(timeline)
    def post(self):
        """
        Creates a new timeline.
        """
        data = request.json
        create_timeline(data)
        return None, 201


@ns.route('/<string:tl_name>')
@api.response(404, 'Timeline not found.')
class TimelineItem(Resource):

    @api.marshal_with(timeline_with_nodes)
    def get(self, tl_name):
        """
        Returns a timeline with a list of nodes.
        """
        return Timeline.query.filter(Timeline.name == tl_name).one()

    @api.expect(timeline)
    @api.response(204, 'Timeline metadata successfully updated.')
    def put(self, tl_name):
        """
        Update the Timeline metadata
        """
        data = request.json
        update_timeline_metadata(data, tl_name)
        return None, 204


    @api.response(204, 'Timeline successfully deleted.')
    def delete(self, tl_name):
        """
        Deletes a timeline.
        """
        delete_timeline(tl_name)
        return None, 204

@ns.route('/<string:tl_name>/qot')
@api.response(404, 'Timeline not found.')
class TimelineQoT(Resource):

    @api.marshal_with(qot)
    def get(self, tl_name):
        """
        Returns the (Best) Desired QoT of the timeline
        """
        return Timeline.query.filter(Timeline.name == tl_name).one()

@ns.route('/<string:tl_name>/nodes')
@api.response(404, 'Timeline not found.')
class TimelineNodes(Resource):

    @api.marshal_with(timeline_num_nodes)
    def get(self, tl_name):
        """
        Returns the Number of nodes on the timeline
        """
        return Timeline.query.filter(Timeline.name == tl_name).one()

    @api.response(201, 'Node successfully created.')
    @api.expect(node)
    def post(self, tl_name):
        """
        Add a new node to the timeline
        """
        data = request.json
        # Get the IP of the node -> TBD: May cause issues if coord service is behind a proxy
        ip = request.environ.get('HTTP_X_REAL_IP', request.remotr_addr)
        print("TimelineNodes: POST: Node IP is %s", str(ip))
        create_node(data, tl_name, ip)
        return None, 201

@ns.route('/<string:tl_name>/nodes/<string:node_name>')
@api.response(404, 'Timeline or node not found.')
class TimelineNode(Resource):

    @api.marshal_with(node)
    def get(self, tl_name, node_name):
        """
        Returns the Node information
        """
        return Node.query.filter((Node.name == node_name) & (Node.timeline_name == tl_name)).one()

    @api.expect(qot)
    @api.response(204, 'Node QoT successfully updated.')
    def put(self, tl_name, node_name):
        """
        Update the QoT Requirements of the node
        """
        data = request.json
        updateqot_node(data, tl_name, node_name)
        return None, 204

    @api.response(204, 'Node successfully deleted')
    def delete(self, tl_name, node_name):
        """
        Deletes a node
        """
        delete_node(tl_name, node_name)
        return None, 204

@ns.route('/<string:tl_name>/servers')
class TimelineServerCollection(Resource):

    @api.marshal_list_with(server)
    def get(self, tl_name):
        """
        Returns list of active servers.
        """
        servers = TimelineServer.query.filter(TimelineServer.tl_name == tl_name).all()
        # Check if servers is empty -> if yes try to see if remote servers exist
        if not servers:
            server = get_remote_timeline_server(tl_name)
            # Append to the list if remote server is non-empty
            if bool(server) != False: 
                servers.append(server)

        return servers

    @api.response(201, 'Timeline Server successfully registered.')
    @api.expect(server)
    def post(self, tl_name):
        """
        Register a new server.
        """
        data = request.json
        register_timeline_server(data, tl_name)
        return None, 201


@ns.route('/<string:tl_name>/servers/<string:server_name>')
@api.response(404, 'Timeline Server not found.')
class TimelineServerItem(Resource):

    @api.marshal_with(server)
    def get(self, tl_name, server_name):
        """
        Returns a timeline with a list of nodes.
        """
        return TimelineServer.query.filter((TimelineServer.name == server_name) & (TimelineServer.tl_name == tl_name)).one()


    @api.response(204, 'Timeline Server successfully deleted.')
    def delete(self, tl_name, server_name):
        """
        Deletes a timeline.
        """
        delete_timeline_server(server_name, tl_name)
        return None, 204

