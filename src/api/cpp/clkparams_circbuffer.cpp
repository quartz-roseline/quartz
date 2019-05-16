/*
 * @file clkparams_circbuffer.cpp
 * @brief Circular Buffer Implementation for clock parameters
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

#include "clkparams_circbuffer.hpp"

using namespace qot_coreapi;
	
// Constructor 
CircularBuffer::CircularBuffer(int size)
 :insert_point(0), current_size(0)
{
	if (size > 0)
	{
		buffer.reserve(size);
		buf_size = size;
	}
	else
	{
		buffer.reserve(CIRBUFF_DEFSIZE);
		buf_size = CIRBUFF_DEFSIZE;
	}
}

// Destructor
CircularBuffer::~CircularBuffer()
{
	// Nothing to do now ..
}

/* Add element to the circular buffer */
int CircularBuffer::AddElement(tl_translation_t params)
{
	buffer_lock.lock();
	buffer[insert_point] = params;
	insert_point = (insert_point + 1) % buf_size;
	if (current_size < buf_size)
		current_size++;
	buffer_lock.unlock();
	return 0;
}

/* Find the appropriate clock parameters */
int CircularBuffer::FindParams(timepoint_t coretime, tl_translation_t &params)
{
	int i, loc;
	int retval = -1;
	int64_t core_ns = TP_TO_nSEC(coretime);

	buffer_lock.lock();

	// Check if vector is empty
	if (current_size == 0)
	{
		buffer_lock.unlock();
		return retval;
	}

	// Find nearest smaller "last timestamp" which corresponds to instance at which the values were computed
	for (i = 0; i < current_size; i++)
	{
		// Find location to search
		if (insert_point == 0)
		{
			loc = current_size - 1 - i;
		}
		else
		{
			loc = insert_point - 1 - i;
			if (loc < 0)
				loc = current_size + loc;
		}

		if (core_ns > buffer[loc].last)
		{
			params = buffer[loc];
			retval = 0;
			break;
		}
	}

	buffer_lock.unlock();
	return retval;
}


	



