/*
 * @file CircBuffer.cpp
 * @brief Circular Buffer Implementation for peer clock offsets
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
#include <iostream>
#include "CircBuffer.hpp"

using namespace qot;
	
// Constructor 
CircBuffer::CircBuffer(int size)
 :insert_point(0), current_size(0), latest_slope(0.0), latest_intercept(0), prev_insert_point(-1), clk_params(NULL)
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
CircBuffer::~CircBuffer()
{
	// Nothing to do now ..
}

/* Add element to the circular buffer */
int CircBuffer::AddElement(peer_clk_params_t params)
{
	int64_t intercept;
	double slope;

	// Detect aninvalid sample
	if (params.timestamp == 0)
		return 0;

	// Detect a duplicate sample
	if (prev_insert_point >= 0)
	{
		if (params.timestamp == buffer[prev_insert_point].timestamp)
			return 0;
	}

	buffer_lock.lock();
	buffer[insert_point] = params;
	if (current_size < buf_size)
		current_size++;

	// Calculate the slope and offset
	if (current_size > 1)
	{
		slope = double(buffer[insert_point].offset_ns - buffer[prev_insert_point].offset_ns)/double(buffer[insert_point].timestamp - buffer[prev_insert_point].timestamp);
		intercept = buffer[insert_point].offset_ns - int64_t(slope*buffer[insert_point].timestamp);
		std::cout << buffer[insert_point].offset_ns << " " << buffer[insert_point].timestamp << "\n";
	    // Add the parameters to the local timeline data structure
	    if (clk_params != NULL)
	    {
	    	clk_params->last = 0;                            
	    	clk_params->mult = int64_t((-slope)*1000000000LL);   
	    	clk_params->nsec = -intercept; 
	    	clk_params->slope = -slope;
	    	std::cout << "Overlay Parameters updated mult = " << clk_params->mult << ", nsec = " << clk_params->nsec << "\n";          
	  	}
	}
	
	// Update previous insert point & insert point
	prev_insert_point = insert_point;
	insert_point = (insert_point + 1) % buf_size;
	buffer_lock.unlock();

	// Update the class intercept and slope variables
	latest_intercept = intercept;
	latest_slope = slope;

	return 0;
}

/* Find the appropriate offset (using linear interpolation for "new" timestamps) */
int64_t CircBuffer::GetOffset(uint64_t timestamp)
{
	int64_t offset = int64_t(latest_slope*timestamp) + latest_intercept;
	return offset;
}

/* Find the latest drift */
double CircBuffer::GetLatestDrift()
{
	double slope;
	buffer_lock.lock();
	slope = latest_slope;
	buffer_lock.unlock();
	return slope;
}

/* Find the appropriate offset (using linear interpolation for "new" timestamps) */
int CircBuffer::GetOffsettedTime(struct timespec *now)
{
	int64_t now_ns = int64_t(now->tv_sec*1000000000LL) + now->tv_nsec;
	now_ns -= int64_t(latest_slope*now_ns) + latest_intercept;
	now->tv_sec = now_ns/1000000000ULL;
	now->tv_nsec = now_ns % 1000000000LL;
	return 0;
}

/* Set the pointer to the variable which holds the estimated clock parameters */
int CircBuffer::SetClkParamVar(tl_translation_t *set_clk_params)
{
	clk_params = set_clk_params;
	return 0;
}

/* Find the appropriate offset (using linear interpolation) */
int CircBuffer::FindOffset(uint64_t timestamp, int64_t &offset)
{
	int i, loc, next_loc, prev_loc;
	int retval = -1;
	double slope;
	int64_t intercept;

	buffer_lock.lock();

	// Check if vector has size less than 2
	if (current_size < 2)
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

		// Location is now head of the buffer, check if timestamp is in the future
		// Compared to the latest timestamp
		if (i == 0 && timestamp > buffer[loc].timestamp)
		{
			// Case 1: Linearly interpolate using the last two values into the future
			// Find prev location to interpolate between the two locations
			if (insert_point == 0)
			{
				prev_loc = current_size - 1 - i - 1;
			}
			else
			{
				prev_loc = insert_point - 1 - i - 1;
				if (prev_loc < 0)
					prev_loc = current_size + prev_loc;
			}

			// Calculate the offset
			slope = double(buffer[loc].offset_ns - buffer[prev_loc].offset_ns)/double(buffer[loc].timestamp - buffer[prev_loc].timestamp);
			intercept = buffer[loc].offset_ns - int64_t(slope*buffer[loc].timestamp);
			offset = int64_t(slope*timestamp) + intercept;

			retval = 0;
			break;
		}
		else if (timestamp > buffer[loc].timestamp)
		{
			// Case 2: Interpolate between loc and next loc
			// Calculate the offset
			slope = double(buffer[next_loc].offset_ns - buffer[loc].offset_ns)/double(buffer[next_loc].timestamp - buffer[loc].timestamp);
			intercept = buffer[loc].offset_ns - int64_t(slope*buffer[loc].timestamp);
			offset = int64_t(slope*timestamp) + intercept;
			retval = 0;
			break;
		}
		else
		{
			next_loc = loc;
		}
	}

	buffer_lock.unlock();
	return retval;
}

/* Find the appropriate clock parameters */
int CircBuffer::FindParams(uint64_t timestamp, peer_clk_params_t &params)
{
	int i, loc;
	int retval = -1;

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

		if (timestamp > buffer[loc].timestamp)
		{
			params = buffer[loc];
			retval = 0;
			break;
		}
	}

	buffer_lock.unlock();
	return retval;
}


	



