/**
 * @file PeerTSclient.hpp
 * @brief  Peer to Peer Timestamping Echo Server to figure out "network-effect" discrepancies
 * @author Anon D'Anon
 * 
 * Copyright (c) Anon, 2018. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *      1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice, 
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */
#ifndef QOT_PEER_TIMESTAMPING_SERVER_HPP
#define QOT_PEER_TIMESTAMPING_SERVER_HPP

#include <boost/thread.hpp> 
#include <boost/log/trivial.hpp>
#include <set>
#include <map>

namespace qot
{
	class PeerTSserver
	{
		/* Constructor and Destructor 
		params:portno The port on which the server listens
	           iface  Name of the interface on which to listen to
	           offset Offset to apply to timestamps
	           ts_flag Flag to get hardware timestamps 0 -> SW Kernel Timestamps 1 -> HW Timestamps in System Time 2-> HW Timestamps*/
		public: PeerTSserver(int portno, const std::string &iface, int64_t offset, int ts_flag);
		public: PeerTSserver(int portno, const std::string &iface, int64_t offset, int ts_flag, std::set<std::string> &exclusion_set, std::map<std::string, std::string> &multicast_map);
		public: ~PeerTSserver();

		// Control functions
		public: int Reset();
		public: int Start(const std::string &node_name);
		public: int Stop();

		// Function to check error status
		public: bool GetErrorStatus();

		/* Desc: Function to start a server which recevies packets from other clients */
		private: int ts_server_loop();

		// Class variables
		private: std::string node_uuid;	                  // Name of the server (IP or hostname)
		private: int portno;
		private: std::string iface;
		private: int64_t offset;
		private: int ts_flag; // type of timestamping used
		private: boost::thread server_thread;
		private: bool running;
		private: int sockfd; /* socket */
		private: bool error_flag;
		private: int error_count;
		private: bool ptp_msgflag;	// Flag indicating messages are PTP-like

		// Class variables to hack for BBB-like platforms supporting only multicast PTP HW timestamping
		private: std::set<std::string> exclusion_set; // IP addresses to filter out packets from
		private: std::map<std::string, std::string> multicast_map; // Associate IP addresses with Multicast addresses

	};
}

#endif
