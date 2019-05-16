/*
 * @file qot_timeline_clock.hpp
 * @brief Timeline Clock class header in the QoT stack
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

#ifndef QOT_TIMELINE_CLOCK_HPP
#define QOT_TIMELINE_CLOCK_HPP

// Include the QoT Data Types
extern "C"
{
	#include "../../qot_types.h"
}

// C++ Std Library
#include <string>

namespace qot_core
{
	// Timeline Clock class
	class TimelineClock
	{
		// Constructor and Destructor -> main_clk_flag indicates the clock is the primary clock
		public: TimelineClock(qot_timeline_t &timeline, bool main_clk_flag);
		public: ~TimelineClock();
		
		/* Update the accuracy and resolution of the clock */
		public: qot_return_t set_quality(timequality_t qot);
		
		/* Get the accuracy and resolution of the clock */
		public: timequality_t get_desired_quality();

		/* Get the translation parameters of the clock */
		public: tl_translation_t get_translation_params();

		/* Get the Shared Memory file descriptor */
		public: int get_shm_fd();

		/* Get the Read-only shared memory file descriptor */
		public: int get_rdonly_shm_fd();

		// Query the status flag to know the construction status
		public: int query_status_flag();

		// Timeline Info
		private: qot_timeline_t timeline_info;		// Timeline Info
		private: timequality_t quality;				// Timeline Accuracy and Resolution 
		private: tl_translation_t *clock_params;    // Timeline Clock Parameters
		private: int status_flag;                   // Constructor status flag 

		// POSIX Shared Memory Information
		private: std::string tl_shm_name;           // Name of the shared memory region
		private: int tl_shm_fd;						// file descriptor to shared memory
		private: int tl_shm_fd_rdonly;				// read-only file descriptor to shared memory 

		
	};
}



#endif