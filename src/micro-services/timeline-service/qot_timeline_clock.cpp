/*
 * @file qot_timeline_clock.cpp
 * @brief Timeline Clock Class functions in the QoT stack
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

// C++ Standard Library
#include <iostream>
#include <new>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>

// Headers for Shared Memory C Interprocess Communication
extern "C"
{
	#include <unistd.h>		// Std C
	#include <fcntl.h>		// File operations
	#include <sys/shm.h>	// Shared Memory
	#include <sys/mman.h>	// Memory Management
	#include <errno.h>		// Error
}

// Internal Timeline Class Header
#include "qot_timeline_clock.hpp"

// QoT Core Namespace
using namespace qot_core;


/* Private functions */

/* Public functions */

/* Constructor: Create a new timeline clock ß*/
TimelineClock::TimelineClock(qot_timeline_t &timeline, bool main_clk_flag)
  : timeline_info(timeline), tl_shm_name("null"), status_flag(0), clock_params(NULL)
{
    void* tl_shm_base;

    // Initialize the quality parameters to zero
    quality.resolution.sec = 0;
    quality.resolution.asec = 0;
    quality.accuracy.below = quality.resolution;
    quality.accuracy.above = quality.resolution;

    /* Set up the shared memory location for the timeline */

	// Setup the name of the shared memory location
	std::ostringstream tl_name;
	if (timeline.type == QOT_TIMELINE_LOCAL && main_clk_flag)
	{
		tl_name << "timeline_local";
	}
	else if (timeline.type == QOT_TIMELINE_LOCAL)
	{
		tl_name << "timeline" << timeline.index;
	}
	else	// Timeline is a global timeline
	{
		tl_name << "timeline_global";
	}

	tl_shm_name = tl_name.str();

	// Create a shared memory location
	tl_shm_fd = shm_open(tl_shm_name.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
  	if (tl_shm_fd == -1) {
    	std::cout << "qot_timeline_clock: Shared memory creation failed: " << strerror(errno) << "\n";
    	status_flag = 1;
    	return;
 	}

 	// Configure the size of the shared memory segment 
  	ftruncate(tl_shm_fd, sizeof(tl_translation_t));

  	// Map the shared memory region into the memory space
  	tl_shm_base = mmap(0, sizeof(tl_translation_t), PROT_READ | PROT_WRITE, MAP_SHARED, tl_shm_fd, 0);
	if (tl_shm_base == MAP_FAILED) {
	    std::cout << "qot_timeline_clock: Shared memory mmap failed: " << strerror(errno) << "\n";
	    
	    // Close and unlink the shared memory region
	    close(tl_shm_fd);
	    shm_unlink(tl_shm_name.c_str());

	    // Update the status flag
	    status_flag = 2;
	    return;
	}

	// Point the memory to the translation parameter pointer
	clock_params = (tl_translation_t*)tl_shm_base;

	// Initialize the clock parameters to zero
    clock_params->last = 0;		// Last core time instance at which synchronization happened
    clock_params->mult = 0;		// Frequency compensation multiplication factor in ppb
    clock_params->nsec = 0;		// Offset
    clock_params->u_nsec = 0;	// Right hand bound on offset uncertainty 
    clock_params->l_nsec = 0;	// Left hand bound on offset uncertainty
    clock_params->u_mult = 0;	// Right hand bound on ppb uncertainty
    clock_params->l_mult = 0;	// Left hand bound on ppb uncertainty

	// Create a shared memory location
	tl_shm_fd_rdonly = shm_open(tl_shm_name.c_str(), O_RDONLY, 0666);
  	if (tl_shm_fd_rdonly == -1) {
    	std::cout << "qot_timeline_clock: read-only shared memory open failed: " << strerror(errno) << "\n";
    	
    	// Close and unlink the shared memory region
    	munmap((void*)clock_params, sizeof(tl_translation_t));
	    close(tl_shm_fd);
	    shm_unlink(tl_shm_name.c_str());

	    // Update the status flag
    	status_flag = 3;
    	return;
 	}

 	// Unlink the Shared memory file so no other process can create file descriptors
 	shm_unlink(tl_shm_name.c_str());

}

/* Destructor: Remove a timeline clock*/
TimelineClock::~TimelineClock()
{
	if (status_flag != 0)
        return;

    // Destroy the shared memory object and region
    munmap((void*)clock_params, sizeof(tl_translation_t));
    close(tl_shm_fd);
    close(tl_shm_fd);
    clock_params = NULL;
}

/* Update the accuracy and resolution of the clock */
qot_return_t TimelineClock::set_quality(timequality_t qot)
{
	quality = qot;
	return QOT_RETURN_TYPE_OK;
}

/* Get the accuracy and resolution of the clock */
timequality_t TimelineClock::get_desired_quality()
{
	return quality;
}

/* Get the translation parameters of the clock */
tl_translation_t TimelineClock::get_translation_params()
{
	return *clock_params;
}

/* Get the Shared Memory file descriptor */
int TimelineClock::get_shm_fd()
{
	// Needs to be tested
	return tl_shm_fd;
}

/* Get the Read-only shared memory file descriptor */
int TimelineClock::get_rdonly_shm_fd()
{
	// Needs to be tested
	return tl_shm_fd_rdonly;
}

/* Query the status flag */
int TimelineClock::query_status_flag()
{
    return status_flag;
}

