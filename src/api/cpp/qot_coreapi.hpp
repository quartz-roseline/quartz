/*
 * @file qot_coreapi.hpp
 * @brief The Core C++ application programming interface to the QoT stack
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

#ifndef QOT_STACK_CORE_API_CPP_QOT_H
#define QOT_STACK_CORE_API_CPP_QOT_H

#include <string>

/* Include basic types, time math and ioctl interface */
extern "C"
{
	#include "../../qot_types.h"
}

// If the Userspace QoT Timeline Service is used
#ifdef QOT_TIMELINE_SERVICE
#include "../../micro-services/timeline-service/qot_timeline_service.hpp"

#ifdef NATS_SERVICE
// NATS client header
#include <nats/nats.h>

// Include circular buffer implementation
#include "clkparams_circbuffer.hpp"

#endif 

#endif

namespace qot_coreapi
{
	/* Internal Timeline & Binding Information */
	typedef struct timeline {
	    qot_timeline_t info;                  /* Basic timeline information               */
	    qot_binding_t binding;                /* Basic binding info                       */
	    int fd;                               /* File descriptor to /dev/timelineX ioctl  */
	    int qotusr_fd;                        /* File descriptor to /dev/qotusr ioctl     */
	} timeline_t;

	/* Timer Callback */
	typedef void (*qot_timer_callback_t)(int sig, siginfo_t *si, void *ucontext);

	// Binding Class
	class TimelineBinding
	{
		// Constructor & Destructor
		public: TimelineBinding();
		public: TimelineBinding(int timeout_seconds); // Timeout in seconds till when to try connecting to the timeline service/kernel module
		public: ~TimelineBinding();

		/**
		 * @brief Bind to a timeline with a given resolution and accuracy
		 * @param uuid Name of the timeline
		 * @param name Name of this binding
		 * @param res Maximum tolerable unit of time
		 * @param acc Maximum tolerable deviation from true time
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_bind(const std::string uuid, const std::string name, timelength_t res, timeinterval_t acc);

		/**
		 * @brief Unbind from a timeline
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_unbind();

		/**
		 * @brief Get the accuracy requirement associated with this binding
		 * @param acc Maximum tolerable deviation from true time
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_get_accuracy(timeinterval_t& acc);

		/**
		 * @brief Get the resolution requirement associated with this binding
		 * @param res Maximum tolerable unit of time
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_get_resolution(timelength_t& res);

		/**
		 * @brief Query the name of this application
		 * @param name Pointer to the where the name will be written
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_get_name(std::string& app_name);

		/**
		 * @brief Query the timeline's UUID
		 * @param uuid Name of the timeline
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_get_uuid(std::string& timeline_uuid);


		/**
		 * @brief Set the accuracy requirement associated with this binding
		 * @param acc Maximum tolerable deviation from true time
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_set_accuracy(timeinterval_t& acc);

		/**
		 * @brief Set the resolution requirement associated with this binding
		 * @param res Maximum tolerable unit of time
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_set_resolution(timelength_t& res);

		/**
		 * @brief Query the time according to the core
		 * @param core_now Estimated time
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_getcoretime(utimepoint_t& core_now);

		/**
		 * @brief Query the time according to the timeline
		 * @param est Estimated time
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_gettime(utimepoint_t& est);

		/**
		 * @brief Set the periodic scheduling parameters requirement associated with this binding
		 * @param start_offset First wakeup time
		 * @param period wakeup period
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_set_schedparams(timelength_t& period, timepoint_t& start_offset); 


		/**
		 * @brief Block wait until a specified uncertain point
		 * @param utp The time point at which to resume. This will be modified by the
		 *            function to reflect the predicted time of resume
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_waituntil(utimepoint_t& utp);

		/**
		 * @brief Block wait until next period
		 * @param utp Returns the actual uncertain wakeup time
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_waituntil_nextperiod(utimepoint_t& utp);

		/**
		 * @brief Block for a specified length of uncertain time
		 * @param utl The period for blocking. This will be modified by the
		 *             function to reflect the estimated time of blocking
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_sleep(utimelength_t& utl);

		/**
		 * @brief Non-blocking call to create a timer
		 * @param timer A pointer to a timer object
		 * @param callback The function that will be called
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_timer_create(qot_timer_t& timer, qot_timer_callback_t callback);

		/**
		 * @brief Non-blocking call to cancel a timer
		 * @param timer A pointer to a timer object
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_timer_cancel(qot_timer_t& timer);

		/**
		 * @brief Converts core time to remote timeline time
		 * @param est timepoint to be converted
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_core2rem(timepoint_t& est); 

		/**
		 * @brief Converts remote timeline time to core time
		 * @param est timepoint to be converted
		 * @return A status code indicating success (0) or other
		 **/
		public: qot_return_t timeline_rem2core(timepoint_t& est); 

		// Private Function
		private: qot_return_t timeline_check_fd();

		#ifdef QOT_TIMELINE_SERVICE
		/* Send a message to the socket */
		private: qot_return_t send_message(qot_timeline_msg_t &msg);

		/* Convert from core time to timeline time */
		private: qot_return_t qot_loc2rem(utimepoint_t &est, int period, int instant_flag);

		/* Convert from timeline time to core time */
		private: qot_return_t qot_rem2loc(utimepoint_t &est, int period);

		/* Private implementation function to compute the current timeline time */
		private: qot_return_t timeline_getvtime(utimepoint_t &est);

		/* Private implementation function to compute the timestamp uncertainty */
		private: qot_return_t timeline_computeqot(utimepoint_t &est);

		#ifdef NATS_SERVICE
		/* Subscribe to a NATS topic (subject) */
		private: int natsSubscribe(std::string &topic);

		/* Un-subscribe from a NATS topic (subject) */
		private: int natsUnSubscribe();
		
		#endif

		#endif

		// Private Class Members
		private: timeline_t timeline;
		private: int status_flag;

		// UNIX-domain socket stuff
		#ifdef QOT_TIMELINE_SERVICE
		private: int sock;
		private: tl_translation_t *tl_clk_params;		// Main Clock Params
		private: tl_translation_t *tl_ov_clk_params;	// Overlay Clock Params

		// NATS Connection Stuff
		#ifdef NATS_SERVICE
		private: natsConnection      *conn;
	    private: natsSubscription    *sub;
	    private: natsStatus          s;
	    private: volatile bool       done;
		private: int nats_status_flag;    // Indicates connection went through

	    // Circular Buffer
		private: CircularBuffer *param_buffer;

		#endif

		#endif
	};
}

#endif

