/*
 * @file qot_timeline_rest.hpp
 * @brief Header for Timeline REST API Interface to the Coordination Service
 * @author Anon D'Anon
 *
 * Copyright (c) Anon, 2018.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef QOT_TIMELINE_REST_HPP
#define QOT_TIMELINE_REST_HPP

#include <string>
#include <vector>

#include <cpprest/http_client.h>

#include "qot_tl_types.hpp"
 
using namespace web;
using namespace web::http;
using namespace web::http::client;

namespace qot_core
{
	// Timeline class
	class TimelineRestInterface
	{
		// Constructor and Destructor
		public: TimelineRestInterface(std::string host);
		public: ~TimelineRestInterface();

		/* Public Functions */
		public: std::vector<std::string> get_timelines();
		public: int post_timeline(std::string timeline_uuid);
		public: int delete_timeline(std::string timeline_uuid);
		public: std::vector<qot_node_phy_t> get_timeline_nodes(std::string timeline_uuid);
		public: std::string get_timeline_metadata(std::string timeline_uuid);
		public: int put_timeline_metadata(std::string timeline_uuid, std::string meta_data);
		public: int get_timeline_num_nodes(std::string timeline_uuid);
		public: int get_timeline_coord_id(std::string timeline_uuid);
		public: int post_node(std::string timeline_uuid, std::string node_uuid, unsigned long long accuracy_ns, unsigned long long resolution_ns);
		public: int delete_node(std::string timeline_uuid, std::string node_uuid);
		public: int get_node(std::string timeline_uuid, std::string node_uuid, unsigned long long &accuracy_ns, unsigned long long &resolution_ns);
		public: std::string get_node_ip(std::string timeline_uuid, std::string node_uuid);
		public: int put_node(std::string timeline_uuid, std::string node_uuid, unsigned long long accuracy_ns, unsigned long long resolution_ns);
		public: int get_timeline_qot(std::string timeline_uuid, unsigned long long &accuracy_ns, unsigned long long &resolution_ns);
		public: std::vector<qot_server_t> get_timeline_servers(std::string timeline_uuid);	
		public: int post_timeline_server(std::string timeline_uuid, qot_server_t &server);
		public: int get_timeline_server_info(std::string timeline_uuid, qot_server_t &server);
		public: int delete_timeline_server(std::string &timeline_uuid, std::string &server_name);

		/* Private Variables */
		private: http_client client;		// C++ Rest SDK Client Instance
		private: std::string host_url;	// Host at which to make the request

	};
}



#endif