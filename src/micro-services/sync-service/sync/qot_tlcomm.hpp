/*
 * @file qot_tlcomm.hpp
 * @brief Header of the Interface to communicate with the Timeline Service  
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

#ifndef QOT_TL_COMM_HPP
#define QOT_TL_COMM_HPP

#include <mutex>

// Include the QoT Data Types
extern "C"
{
	#include "../../../qot_types.h"
}

// GEt the custom types required by the timeline service
#include "../../timeline-service/qot_tl_types.hpp"

#include "../../timeline-service/qot_timeline_service.hpp"

namespace qot
{
	// Timeline-service communicator class
	class TLCommunicator
	{
		// Constructor and Destructor
		public: TLCommunicator();
		public: ~TLCommunicator();

		// Request pointer to timeline clock shared memory from the timeline service
		public: tl_translation_t* request_clk_memory(int timeline_id);

		// Request pointer to timeline overlay clock shared memory from the timeline service
		public: tl_translation_t* request_ov_clk_memory(int timeline_id);

		// Get the Timeline NTP Server
		public: int get_timeline_server(int timeline_id, qot_server_t &server);

		// Set the Timeline NTP Server
		public: int set_timeline_server(int timeline_id, qot_server_t &server);

		// Send a message to the timeline service
		private: int send_message(qot_timeline_msg_t &msg);

		/* Mutex used to protect the data structure */
		private: std::mutex comm_mutex;

		// Socket fd used to communicate
		private: int sock;
		private: int status_flag;

	};
}

#endif