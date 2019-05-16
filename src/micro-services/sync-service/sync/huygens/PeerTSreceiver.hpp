/**
 * @file PeerTSreceiver.hpp
 * @brief  Peer to Peer receiver to get offsets from the compute server
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
#ifndef QOT_PEER_RECEIVER_HPP
#define QOT_PEER_RECEIVER_HPP

#include <boost/thread.hpp> 
#include <boost/log/trivial.hpp>
#include <fstream>

extern "C"
{
	#include <time.h>
	#include "../../../../qot_types.h"
}

#ifdef NATS_SERVICE
// NATS client header
#include <nats/nats.h>
#endif 

#include "CircBuffer.hpp"
#include "../SyncUncertainty.hpp"

// Define a structure to encapsulate multiple pointers
struct data_ptrs {
    qot::SyncUncertainty *sync_uncertainty;
    qot::CircBuffer *param_buffer;
    tl_translation_t *clk_params;
};

namespace qot
{
	class PeerTSreceiver
	{
		/* Constructor and Destructor 
		Params: node_name       The DNS name or IP of the "receiver" node 
                pub_server      Server to which to publish data 
                iface_name      Name of the interface on which to discipline the clock
                discipline_flag True indicates the clock should be disciplined */
		public: PeerTSreceiver(const std::string &node_name, const std::string &pub_server, const std::string &iface_name, bool discipline_flag);
		public: ~PeerTSreceiver();

		// Control functions
		public: int Start(uint64_t proc_period_ns);
		public: int Stop();

		// PHC Control functions
		private: int phc_initialize(); 

		/* Set the pointer to the variable which holds the estimated clock parameters */
		public: int SetClkParamVar(tl_translation_t *set_clk_params);

		// Private class variables
		private: std::string node_uuid;	                  // Name of the server (IP or hostname)
	    private: uint64_t proc_period_ns;                 // Processing Period 
		private: std::string iface;						  // Name of the interface
		private: bool disc_flag;						  // Flag indicating if the PHC should be disciplined

		#ifdef NATS_SERVICE
		// Connect to the NATS Server
		private: natsStatus natsConnect(const char* nats_url);
		// Return if the nats connection succeeded or not
		private: int getNatsStatus();
		// Subscribe to a NATS topic (subject)
		private: int natsSubscribe(std::string &topic);
		// Un-subscribe from a NATS topic (subject) 
		private: int natsUnSubscribe();
	
		// NATS messaging variables
		private: natsConnection      *conn;
	    private: natsSubscription    *sub;
	    private: natsStatus          s;
	    private: volatile bool       done;
		#endif

		// Sync Uncertainty Calculator 
		private: SyncUncertainty *sync_uncertainty;

		// Circular Buffer
		private: CircBuffer *param_buffer;

		// Encapsulating data structure containing multiple pointers
		private: struct data_ptrs data;

		/* Params to translate from local HW to local timeline master */
		private: tl_translation_t *clk_params;

		// Clock ID of the PHC
		private: clockid_t clkid;

		// Publishing Server
	    private: std::string nats_server;
	};
}

#endif