# @file models.py
# @brief QoT Coordination Service SQLAlchemy DB Models
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
# Based on: http://flask-sqlalchemy.pocoo.org/2.1/quickstart/#simple-relationships

from datetime import datetime

from coordination_service.database import db

class Timeline(db.Model):
    '''Timeline SQLALchemy ORM Class'''
    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(80), unique=True, nullable=False)
    num_nodes = db.Column(db.Integer, nullable=False)              # Number of nodes
    accuracy = db.Column(db.Integer, nullable=False)               # Desired QoT Accuracy in ns
    resolution = db.Column(db.Integer, nullable=False)             # Desired QoT Resolution in ns 
    meta_data = db.Column(db.String(80), nullable=False)           # Timeline Meta Data

    def __init__(self, name):
        self.name = name
        self.num_nodes = 0              
        self.accuracy = 1000000000   # Set to 1 second by default
        self.resolution = 100        # Set to 100 nanosecond by default
        self.meta_data = "NULL"      # Timeline MetaData -> Set to a "NULL" string

    def __repr__(self):
        return '<Timeline %r>' % self.name


class Node(db.Model):
    '''Node SQLALchemy ORM Class'''
    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(50))                                # Node UUID
    ip = db.Column(db.String(50))                                  # IP address corresponding to the node
    accuracy = db.Column(db.Integer, nullable=False)               # Desired QoT Accuracy in ns
    resolution = db.Column(db.Integer, nullable=False)             # Desired QoT Resolution in ns 
    timeline_name = db.Column(db.String(80), db.ForeignKey('timeline.name'), nullable=False)
    timeline = db.relationship('Timeline', backref=db.backref('nodes', lazy='dynamic'))

    def __init__(self, name, timeline, accuracy, resolution, ip):
        self.name = name
        self.timeline = timeline
        self.accuracy = accuracy
        self.resolution = resolution
        self.ip = ip

    def __repr__(self):
        return '<Node %r>' % self.name

class Server(db.Model):
    '''NTP Server SQLALchemy ORM Class'''
    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(80), unique=True, nullable=False)
    stratum = db.Column(db.Integer, nullable=False)              # Stratum of the NTP server
    server_type = db.Column(db.String(80), nullable=False)       # Access Type of the server (global or local)

    def __init__(self, name, stratum, server_type):
        self.name = name
        self.stratum = stratum  
        self.server_type = server_type           
        
    def __repr__(self):
        return '<Server %r>' % self.name

class TimelineServer(db.Model):
    '''NTP Server SQLALchemy ORM Class'''
    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(80), unique=True, nullable=False)
    stratum = db.Column(db.Integer, nullable=False)              # Stratum of the NTP server
    server_type = db.Column(db.String(80), nullable=False)       # Access Type of the server (global or local)
    tl_name = db.Column(db.String(80), nullable=False)           # Name of the timeline to which the server corresponds

    def __init__(self, name, stratum, server_type, tl_name):
        self.name = name
        self.stratum = stratum  
        self.server_type = server_type 
        self.tl_name = tl_name          
        
    def __repr__(self):
        return '<Server name: %r, timeline %r, stratum %r, type %r>' % (self.name, self.tl_name, self.stratum, self.server_type)