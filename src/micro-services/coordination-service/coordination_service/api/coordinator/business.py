# @file business.py
# @brief QoT Coordination Service Core Logic (DB and Zookeeper Interface)
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

from coordination_service.database import db
from coordination_service.database.models import Node, Timeline, Server, TimelineServer
from coordination_service.zookeeper import zk_coordinator

# Import the NATS Publisher Wrapper Class
from coordination_service.zookeeper.nats_pub import NATSPublisher

import json

publisher = NATSPublisher()  # NATS Publisher

def init_cluster_node_publisher(pub_host="localhost:4222"):
    ''' Initialize the publisher for cluster node joining/leaving info '''
    publisher.publisher_connect(pub_host)

def destroy_cluster_node_publisher():
    ''' Destroy the publisher for cluster node joining/leaving info '''
    publisher.terminate_publisher()

def publish_cluster_node_publisher(timeline_name, event_string):
    topic = "coordination.timelines." + str(timeline_name) + ".local"
    timeline_nodes = Node.query.filter(Node.timeline_name == timeline_name).all()
    data = {}
    # data["event"] = event_string # If added the parsing code on the timeline service end needs to be modified
    for node in timeline_nodes:
        data[node.name] = {}
        data[node.name]["accuracy"] = node.accuracy
        data[node.name]["resolution"] = node.resolution
    
    publisher.publish_data(topic, json.dumps(data).encode('utf-8'))

def create_node(data, timeline_name, ip):
    ''' Create a node in the DB'''
    name = data.get('name')
    accuracy = data.get('accuracy')
    resolution = data.get('resolution')
    timeline = Timeline.query.filter(Timeline.name == timeline_name).one()
    node_count = Node.query.filter((Node.name == name) & (Node.timeline_name == timeline_name)).count()

    if node_count == 0: # Indicates that node was not found in the db
        # Increment the number of nodes by 1
        timeline.num_nodes += 1

        # Create a Node DB data structure
        node = Node(name, timeline, accuracy, resolution, ip)

        # Add and Commit to DB
        db.session.add(node)
        db.session.add(timeline)
        db.session.commit()

        # Publish the info (all the nodes) to other nodes ("timeline service")
        publish_cluster_node_publisher(timeline_name, "node_joined")

        update_timeline(timeline_name, accuracy, resolution)

def updateqot_node(data, timeline_name, node_name):
    ''' Update the node QoT info in the DB'''
    name = node_name
    accuracy = data.get('accuracy')
    resolution = data.get('resolution')
    node = Node.query.filter((Node.name == node_name) & (Node.timeline_name == timeline_name)).one()

    node.resolution = resolution
    node.accuracy = accuracy

    # Update the timeline info
    update_timeline(timeline_name, accuracy, resolution)
    db.session.add(node)
    db.session.commit()

    # Publish the info (all the nodes) to other nodes ("timeline service")
    publish_cluster_node_publisher(timeline_name, "node_update")

def delete_node(timeline_name, node_name):
    ''' Delete a node in the DB'''
    node = Node.query.filter((Node.name == node_name) & (Node.timeline_name == timeline_name)).one()
    timeline = Timeline.query.filter(Timeline.name == timeline_name).one()

    # Return if no nodes exist on the timeline
    if timeline.num_nodes <= 0:
        return

    # Decrement the number of nodes by 1
    timeline.num_nodes -= 1

    db.session.delete(node)

    if timeline.num_nodes == 0:
        zk_coordinator.delete_timeline(timeline_name)
        db.session.delete(timeline)
    else:
        # Update the QoT requirements
        accuracy, resolution = get_newtlqot(timeline_name)
        timeline.accuracy = accuracy
        timeline.resolution = resolution
        db.session.add(timeline)
    
    db.session.commit()

    # Publish the info (all the nodes) to other nodes ("timeline service")
    publish_cluster_node_publisher(timeline_name, "node_left")

def get_newtlqot(timeline_name):
    ''' Compute the updated QoT for the timeline'''
    nodes = Node.query.filter(Node.timeline_name == timeline_name).all()

    desired_acc = 1000000000
    desired_res = 1000000000

    # Get the best QoT requirements among all the leftover nodes
    for node in nodes:
        if node.accuracy < desired_acc:
            desired_acc = node.accuracy
        if node.resolution < desired_res:
            desired_res = node.resolution

    # Update the timeline info for the Zookeeper node
    info = {}
    info['accuracy'] = desired_acc   
    info['resolution'] = desired_res     
    zk_coordinator.update_timeline(timeline_name, json.dumps(info).encode('utf8'))

    return desired_acc, desired_res

def create_timeline(data):
    ''' Create a timeline '''
    timeline_name = data.get('name')
    timeline_id = data.get('id')

    # Check if the timeline already exists
    timeline_count = Timeline.query.filter(Timeline.name == timeline_name).count()
    if timeline_count == 0:
        timeline = Timeline(timeline_name)
        if timeline_id:   # If set to non-zero value then override
            timeline.id = timeline_id

        # Add to Zookeeper -> Change the test into a JSON with the timeline "info"
        info = {}
        info['accuracy'] = 1000000000   # 1 billion ns (default values)
        info['resolution'] = 100        # 100 ns       (default values)
        zk_coordinator.create_timeline(timeline_name, json.dumps(info).encode('utf8'))

        db.session.add(timeline)
        db.session.commit()


def update_timeline(timeline_name, acc_ns, res_ns):
    ''' Update the timeline QoT '''
    timeline = Timeline.query.filter(Timeline.name == timeline_name).one()

    # Check if update is required
    if acc_ns != 0 and timeline.accuracy > acc_ns:
        timeline.accuracy = acc_ns

    if res_ns != 0 and timeline.resolution > res_ns:
        timeline.resolution = res_ns  

    # Update the timeline info for the Zookeeper node
    info = {}
    info['accuracy'] = timeline.accuracy   
    info['resolution'] = timeline.resolution     
    zk_coordinator.update_timeline(timeline_name, json.dumps(info).encode('utf8'))

    # Update the node
    db.session.add(timeline)
    db.session.commit()


def delete_timeline(timeline_name):
    ''' Delete the timeline '''
    timeline = Timeline.query.filter(Timeline.name == timeline_name).one()

    nodes = Node.query.filter(Node.timeline_name == timeline_name).count()

    if nodes == 0:
        # Delete timeline from Zookeeper 
        zk_coordinator.delete_timeline(timeline_name)

        db.session.delete(timeline)
        db.session.commit()  

def update_timeline_metadata(data, timeline_name):
    ''' Update the timeline metadata '''
    # Check if the timeline exists
    timeline = Timeline.query.filter(Timeline.name == timeline_name).one()
    if timeline is not None:
        timeline.meta_data = data.get('meta_data')

        # Update the node
        db.session.commit()

def register_server(data):
    ''' Register an NTP server '''
    server_name = data.get('name')
    server_stratum = data.get('stratum')
    server_type = data.get('server_type')

    # Check if the server already exists
    server_count = Server.query.filter(Server.name == server_name).count()
    if server_count == 0:
        # Create a server DB instance
        server = Server(server_name, server_stratum, server_type)

        # Add to Zookeeper if global server
        if server_type == "global":
            info = {}
            info['name'] = server_name  # 1 billion ns (default values)
            info['stratum'] = server_stratum        # 100 ns       (default values)
            zk_coordinator.register_server(server_name, json.dumps(info).encode('utf8'))

        # Commit the instance to the DB
        db.session.add(server)
        db.session.commit()   

def delete_server(server_name):
    ''' Delete an NTP server '''
    server = Server.query.filter(Server.name == server_name).one()

    # Server exists in the DB
    if server:
        # Delete server from Zookeeper 
        zk_coordinator.delete_server(server_name)

        # Delete the instance from the DB
        db.session.delete(server)
        db.session.commit()  

def get_remote_timeline_server(timeline_name):
    '''Get the remote servers for a timeline from the zookeeper coordinator'''
    servers = zk_coordinator.get_servers(timeline_name)
    ret_server = {}
    # Get the first server -> Can be updated later to get all servers
    for server_name in servers:
        ret_server["name"] = servers[server_name]["name"]
        ret_server["stratum"] = servers[server_name]["stratum"]
        ret_server["server_type"] = "global"
        break

    return ret_server

def register_timeline_server(data, timeline_name):
    ''' Register an NTP server on a timeline'''
    server_name = data.get('name')
    server_stratum = data.get('stratum')
    server_type = data.get('server_type')

    # Check if the server already exists
    server_count = TimelineServer.query.filter((TimelineServer.name == server_name) & (TimelineServer.tl_name == timeline_name)).count()
    if server_count == 0:
        # Create a server DB instance
        server = TimelineServer(server_name, server_stratum, server_type, timeline_name)

        # Add to Zookeeper if global server
        if server_type == "global":
            info = {}
            info['name'] = server_name  # 1 billion ns (default values)
            info['stratum'] = server_stratum        # 100 ns       (default values)
            zk_coordinator.register_timeline_server(server_name, timeline_name, json.dumps(info).encode('utf8'))

        # Commit the instance to the DB
        db.session.add(server)
        db.session.commit()   
        print('Server added to DB %r' % server)

def delete_timeline_server(server_name, timeline_name):
    ''' Delete an NTP server from a timeline'''
    server = TimelineServer.query.filter((TimelineServer.name == server_name) & (TimelineServer.tl_name == timeline_name)).one()

    # Server exists in the DB
    if server:
        # Delete server from Zookeeper 
        zk_coordinator.delete_timeline_server(server_name, timeline_name)

        # Delete the instance from the DB
        db.session.delete(server)
        db.session.commit()     