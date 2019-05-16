/*
 * @file qot_timeline_subscriber.hpp
 * @brief Timeline CoordinationService-Subscriber class header in the QoT stack
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

#ifndef QOT_TIMELINE_SUBSCRIBER_HPP
#define QOT_TIMELINE_SUBSCRIBER_HPP

// NATS client header
#include <nats/nats.h>

namespace qot_core
{
	// Timeline Subscriber class
	class TimelineSubscriber
	{
		// Constructor and Destructor
		public: TimelineSubscriber(std::string nats_host, std::string timeline_uuid, void *parent);
		public: ~TimelineSubscriber();

		/* Subscribe to a NATS topic (subject) */
		public: int natsSubscribe();

		/* Un-subscribe from a NATS topic (subject) */
		public: int natsUnSubscribe();

		/* Private NATS Connection Variables*/
		private: natsConnection      *conn;
	    private: natsSubscription    *sub;
	    private: natsStatus          s;
	    private: volatile bool       done;
		private: std::string nats_host;

	    /* Private State Variables */
		private: std::string timeline_uuid; 
		private: void* parent_class;

	};
}



#endif