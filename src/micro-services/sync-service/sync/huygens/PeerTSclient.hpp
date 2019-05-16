/**
 * @file PeerTSclient.hpp
 * @brief  Peer to Peer Timestamping Echo Client to figure out "network-effect" discrepancies
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
#ifndef QOT_PEER_TIMESTAMPING_CLIENT_HPP
#define QOT_PEER_TIMESTAMPING_CLIENT_HPP

extern "C" 
{
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h> 
    #include <sys/signal.h>
    #include <pthread.h>
}

#include <boost/thread.hpp> 
#include <boost/log/trivial.hpp>

#ifdef NATS_SERVICE
// NATS client header
#include <nats/nats.h>
#endif 

/* Coded Probes Struct */
struct probe_timestamps {
	int64_t rx[2];			// Local Receive Timestamp T4
	int64_t rx_remote[2];   // Remote Receive Timestamp T2
	int64_t tx[2];          // Local Transmission Timestamp T1
	int64_t tx_remote[2];   // Remote Transmission Timestamp T3
	int validity_flag;
};

namespace qot
{
	class PeerTSclient
	{
		/* Constructor and Destructor 
		Params: hostname   The DNS name or IP of the server to which to connect to
                portno     The port on which the server listens
                iface      Name of the interface on which to listen to
                pub_server Server to which to publish data
                ts_flag Flag to get hardware timestamps 0 -> SW Kernel Timestamps 1 -> HW Timestamps in System Time 2-> HW Timestamps*/
		public: PeerTSclient(const std::string &hostname, int portno, const std::string &iface, const std::string &pub_server, int ts_flag);
		public: ~PeerTSclient();

		// Control functions
		public: int Reset();
		public: int Start(const std::string &node_name, uint64_t period_ns);
		public: int Stop();

		// Function to check error status
		public: bool GetErrorStatus();

		/* Desc: Function to start a client which sends packets to a server */
		private: int ts_client_loop();

		/* Desc: Function to start a processing loop to process packets */
		private: int proc_client_loop();

		// Private class variables
		private: int portno;			                  // Communication Port
		private: std::string iface;		                  // Communication Interface
		private: std::string node_uuid;	                  // Name of the client (IP or hostname)
		private: std::string hostname;	                  // Hostname of the server (IP or hostname)
		private: int ts_flag; 			                  // Timestamping flag
		private: boost::thread client_thread;             // Timestamping ping thread
		private: boost::thread processor_thread;	      // Timestamp processing thread
		private: bool running;			                  // Flag indication service is running
		private: int sockfd;                              // Socket fd
		private: struct hostent *server;	              // Server data structure
	    private: uint64_t tx_period_ns;                   // Transmission Period 
		private: struct probe_timestamps *ts_buffer;	  // Buffer to hold timestamp
		private: struct probe_timestamps *proc_ts_buffer; // Swap buffer to hold past timestamps
		private: uint64_t ts_duration_ns;				  // Duration over which to process timestamps
		private: uint64_t ts_buf_len;					  // Buffer length
		private: pthread_mutex_t data_lock;				  // Mutex to protect buffers
		private: pthread_cond_t data_condvar;             // Condition Variable to indicate new batch
		private: bool error_flag;						  // Error flag to restart the sync
		private: bool ptp_msgflag;						  // Flag indicating messages are PTP-like

		#ifdef NATS_SERVICE
		// Connect to the NATS Server
		private: int natsConnect(const char* nats_url);
		// Return if the nats connection succeeded or not
		private: int getNatsStatus();

		// NATS messaging variables
		private: natsConnection      *conn;
	    private: natsMsg             *msg;
	    private: natsStatus          s;
		#endif

		// Publishing Server
	    private: std::string nats_server;
	};
}

#endif