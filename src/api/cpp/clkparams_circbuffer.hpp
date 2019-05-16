/*
 * @file clkparams_circbuffer.hpp
 * @brief Circular Buffer Header for clock parameters
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

#ifndef QOT_STACK_CIRC_BUFF_QOT_H
#define QOT_STACK_CIRC_BUFF_QOT_H

#include <mutex>
#include <vector>

/* Include basic types, time math and ioctl interface */
extern "C"
{
	#include "../../qot_types.h"
}

#define CIRBUFF_DEFSIZE 10

namespace qot_coreapi
{
	class CircularBuffer 
	{
		// Constructor & Destructor
		public: CircularBuffer(int size);
		public: ~CircularBuffer();

		/* Add element to the circular buffer */
		public:	int AddElement(tl_translation_t params);

		/* Find the appropriate clock parameters */
		public: int FindParams(timepoint_t coretime, tl_translation_t &params);

		/* Circular buffer */
		private: std::vector<tl_translation_t> buffer;
		private: int insert_point;
		private: int current_size;
		private: int buf_size;
		private: std::mutex buffer_lock;
	};
}

#endif

