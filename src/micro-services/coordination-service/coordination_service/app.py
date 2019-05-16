# @file app.py
# @brief QoT Coordination Service Flask App
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

import logging.config

import os
import optparse

from flask import Flask, Blueprint
from coordination_service import settings
from coordination_service.api.coordinator.endpoints.timelines import ns as timelines_namespace
from coordination_service.api.coordinator.endpoints.sync_servers import ns as servers_namespace
from coordination_service.api.restplus import api
from coordination_service.database import db, reset_database

# Import Zookeeper Client Wrapper
from coordination_service.zookeeper import zk_coordinator

# Import the cluster node publisher init/destroy functions
from coordination_service.api.coordinator.business import init_cluster_node_publisher, destroy_cluster_node_publisher

app = Flask(__name__)
logging_conf_path = os.path.normpath(os.path.join(os.path.dirname(__file__), '../logging.conf'))
logging.config.fileConfig(logging_conf_path)
log = logging.getLogger(__name__)


def configure_app(flask_app):
	# flask_app.config['SERVER_NAME'] = settings.FLASK_SERVER_NAME # Commented out to prevent errors on using any other hostname
	flask_app.config['SQLALCHEMY_DATABASE_URI'] = settings.SQLALCHEMY_DATABASE_URI
	flask_app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = settings.SQLALCHEMY_TRACK_MODIFICATIONS
	flask_app.config['SWAGGER_UI_DOC_EXPANSION'] = settings.RESTPLUS_SWAGGER_UI_DOC_EXPANSION
	flask_app.config['RESTPLUS_VALIDATE'] = settings.RESTPLUS_VALIDATE
	flask_app.config['RESTPLUS_MASK_SWAGGER'] = settings.RESTPLUS_MASK_SWAGGER
	flask_app.config['ERROR_404_HELP'] = settings.RESTPLUS_ERROR_404_HELP


def initialize_app(flask_app):
	configure_app(flask_app)

	blueprint = Blueprint('api', __name__, url_prefix='/api')
	api.init_app(blueprint)
	api.add_namespace(timelines_namespace)
	api.add_namespace(servers_namespace)
	flask_app.register_blueprint(blueprint)

	db.init_app(flask_app)

	# Reset the DB
	with app.test_request_context():
		from coordination_service.database.models import Timeline, Node, Server  # noqa
		db.drop_all()
		db.create_all()

	# Initialize the local cluster publisher
	init_cluster_node_publisher(options.pub_host)

def main():
	# Initialize the Flask APP
	initialize_app(app)

	# Initialize the Zookeeper Connection
	zk_coordinator.zookeeper_connect(options.zk_hosts, options.pub_host)
	zk_coordinator.zookeeper_bootstrap(options.coordinator_group)
	zk_coordinator.run_for_master()

	# Start the Flask server
	log.info('>>>>> Starting development server at http://{}/api/ <<<<<'.format(app.config['SERVER_NAME']))
	app.run(host='0.0.0.0', port=8502, debug=settings.FLASK_DEBUG)


if __name__ == "__main__":
	# Parse the command-line arguments
	optParser = optparse.OptionParser()
	optParser.add_option("-z", "--zk_hosts",
				  action="store", 
				  default="127.0.0.1:2181",
				  help="Comma separated list of host:port zookeeper server pairs",)
	optParser.add_option("-p", "--pub_host",
				  action="store", 
				  default="127.0.0.1:4222",
				  help="host:port pubsub server (NATS)",)
	optParser.add_option("-c", "--coordinator_group",
				  action="store", 
				  default="test",
				  help="Coordinator Group unique name",)
	options, args = optParser.parse_args()
	main()
