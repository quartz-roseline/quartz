/*
 * @file CircBuffer.hpp
 * @brief Circular Buffer Header for peer clock offsets
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

#ifndef QOT_CIRC_BUFF_QOT_H
#define QOT_CIRC_BUFF_QOT_H

#include <mutex>
#include <vector>
#include <atomic>

extern "C"
{
	#include <time.h>
	#include "../../../../qot_types.h"
}

#define CIRBUFF_DEFSIZE 30

/* Data structure to store the peer clock parameters */
typedef struct peer_clk_params {
	uint64_t timestamp;
	int64_t offset_ns;
} peer_clk_params_t;

namespace qot
{
	class CircBuffer 
	{
		// Constructor & Destructor
		public: CircBuffer(int size);
		public: ~CircBuffer();

		/* Add element to the circular buffer */
		public:	int AddElement(peer_clk_params_t params);

		/* Find the appropriate clock parameters */
		public: int FindParams(uint64_t timestamp, peer_clk_params_t &params);

		/* Find the appropriate offset (using linear interpolation) */
		public: int FindOffset(uint64_t timestamp, int64_t &offset);

		/* Find the appropriate offset (using linear interpolation for "new" timestamps) */
		public: int64_t GetOffset(uint64_t timestamp);

		/* Find the latest drift */
		public: double GetLatestDrift();

		/* Find the appropriate offset (using linear interpolation for "new" timestamps) */
		public: int GetOffsettedTime(struct timespec *now);

		/* Set the pointer to the variable which holds the estimated clock parameters */
		public: int SetClkParamVar(tl_translation_t *set_clk_params);

		/* Circular buffer */
		private: std::vector<peer_clk_params_t> buffer;
		private: int insert_point;
		private: int prev_insert_point;
		private: int current_size;
		private: int buf_size;
		private: std::mutex buffer_lock;

		/* Params to translate from local HW to local timeline master */
		private: tl_translation_t *clk_params; 

		/* Latest Offset calculation parameters */
		private: std::atomic<int64_t> latest_intercept;
		private: std::atomic<double> latest_slope;
	};
}

#endif

