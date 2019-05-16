/*
 * @file qot.h
 * @brief Global Variables to Share Local Timeline Clock (Header)
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

#ifndef NTP_GLOB_TLCLOCK_DATA_QOT_H
#define NTP_GLOB_TLCLOCK_DATA_QOT_H

#include "../../../../qot_types.h"

/* Flag to protect QoT Stack Code */
#define PTP_QUARTZ 1

/* Definition for clock_adjtime which does not exist in headers */
#ifndef ADJ_SETOFFSET
#define ADJ_SETOFFSET 0x0100
#endif

/* Helper Macros */
#ifndef CLOCKFD
#define CLOCKFD 3
#define FD_TO_CLOCKID(fd)	((~(clockid_t) (fd) << 3) | CLOCKFD)
#define CLOCKID_TO_FD(clk)	((unsigned int) ~((clk) >> 3))
#endif

#ifdef QOT_TIMELINE_SERVICE
// Global Timeline Clock Descriptors 
int local_timelineid;       // Local Timeline ID -> Defaults to 1 (Note: This may not be the actual timeline ID)
extern tl_translation_t* local_clk_params;	// Local Timeline translation Params

// Variable to kill the sync service main thread
extern int sync_service_running;

// Variable to check if node is the master for this timeline -> defined in linuxptp-1.8/clock.c
extern int timeline_master_flag[MAX_TIMELINES];

#endif

#endif

