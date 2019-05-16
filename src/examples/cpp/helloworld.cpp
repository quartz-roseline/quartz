/*
 * @file helloworld.c
 * @brief Simple C++ example binding and unbinding from a timeline
 * @author Anon D'Anon 
 * 
 * Copyright (c) Anon, 2018.
 *
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met
 * 	1. Redistributions of source code must retain the above copyright notice, 
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
 */


// C includes
extern "C" 
{
	#include <signal.h>
	#include <sys/types.h>
    #include <unistd.h>
}

#include <iostream>
#include <cmath>
#include <cstdlib>

// Include the QoT CPP Core API
#include "../../api/cpp/qot_coreapi.hpp"

// Basic onfiguration
#define TIMELINE_UUID    "gl_my_test_timeline"
#define APPLICATION_NAME "default"
#define OFFSET_MSEC      1000

#define DEBUG 1

using namespace qot_coreapi;

static int running = 1;

static void exit_handler(int s)
{
	printf("Exit requested \n");
  	running = 0;
}

// Main entry point of application
int main(int argc, char *argv[])
{
	// Variable declarations
	qot_return_t retval;

	timelength_t resolution;
	timeinterval_t accuracy;

	// Accuracy and resolution values
	resolution.sec = 0;
	resolution.asec = 10000000000; // 10 ns

	accuracy.below.sec = 0;
	accuracy.below.asec = 1000000000000000; // 1 ms
	accuracy.above.sec = 0;
	accuracy.above.asec = 1000000000000000; // 1 ms

	// Current Time
	utimepoint_t now;
	utimepoint_t core_now;
	utimepoint_t wake_now;
	timepoint_t  wake;
	timelength_t step_size;

	// Iteration counter
	int i = 0;

	// Grab the timeline
	const char *u = TIMELINE_UUID;
	if (argc > 1)
		u = argv[1];

	// Grab the application name
	const char *m = APPLICATION_NAME;
	if (argc > 2)
		m = argv[2];

	// Loop Interval
	int step_size_ms = OFFSET_MSEC;
    if (argc > 3)
        step_size_ms = atoi(argv[3]);

    // Accuracy Requirements (1 ns to 999 ms in nanoseconds)
    if (argc > 4)
    {
        accuracy.below.asec = (uint64_t) atoi(argv[4])*1000000000; 
        accuracy.above.asec = (uint64_t) atoi(argv[4])*1000000000; 
        if (DEBUG)
        	printf("Accuracy Set to %llu %llu\n", accuracy.below.sec, accuracy.below.asec);
    }

    // Initialize stepsize
	TL_FROM_mSEC(step_size, step_size_ms);

	if(DEBUG)
		printf("Helloworld starting.... process id %i\n", getpid());

	// Create the timeline class -> The constructor tries to indefinitely connect to the timeline service untill it connects
	TimelineBinding timeline;

	// Bind to a timeline
	if(DEBUG)
		printf("Binding to timeline %s ........\n", u);
	if(timeline.timeline_bind(std::string(u), std::string(m), resolution, accuracy))
	{
		printf("Failed to bind to timeline %s\n", u);
		return QOT_RETURN_TYPE_ERR;
	}
	if(DEBUG)
		printf("Bound to timeline %s ........\n", u);


	// Register signal handler to exit
	signal(SIGINT, exit_handler);

	// Get Core time
	timeline.timeline_getcoretime(core_now);

	// Get Timeline time
    if(timeline.timeline_gettime(now))
	{
		printf("Could not read timeline reference time\n");
	}
	else
	{
		wake_now = now;
		wake = wake_now.estimate;
		timepoint_add(&wake, &step_size);
        wake.asec = 0;
	}

	// Periodic Wakeup Loop
	while(running)
	{
		if(timeline.timeline_gettime(now))
		{
			printf("Could not read timeline reference time\n");
		}
		else if (DEBUG)
		{
			printf("[Iteration %d ]: core time =>\n", i++);
			printf("Scheduled wake up          %lld %llu\n", (long long)wake_now.estimate.sec, (unsigned long long)wake_now.estimate.asec);
			printf("Time Estimate @ wake up    %lld %llu\n", (long long)now.estimate.sec, (unsigned long long)now.estimate.asec);
			printf("Uncertainity below         %llu %llu\n", (unsigned long long)now.interval.below.sec, (unsigned long long)now.interval.below.asec);
			printf("Uncertainity above         %llu %llu\n", (unsigned long long)now.interval.above.sec, (unsigned long long)now.interval.above.asec);
			printf("WAITING FOR %d ms\n", step_size_ms);
		}
		timepoint_add(&wake, &step_size);
		wake_now.estimate = wake;
		timeline.timeline_waituntil(wake_now);
	}

	// Unbind from timeline
	if(timeline.timeline_unbind())
	{
		printf("Failed to unbind from timeline  %s\n", u);
		return QOT_RETURN_TYPE_ERR;
	}
	printf("Unbound from timeline %s\n", u);
	return 0;
}