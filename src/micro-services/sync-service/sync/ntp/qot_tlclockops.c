/**
 * @file qot_tlclockops.c
 * @brief Global Timeline Clock Operations
 * @author Anon D'Anon
 *
 * Copyright (c) Anon, 2018. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *      1. Redistributions of source code must retain the above copyright notice, 
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
 *
 *
 */

#ifdef QOT_TIMELINE_SERVICE

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/shm.h>    // Shared Memory
#include <sys/mman.h>   // Memory Management
#include <errno.h>      // Error

// Include Header
#include "qot_tlclockops.h"

// Include global timeline header
#include "global_timeline.h"

/* Convert from core time to timeline time */
qot_return_t qot_gl_timeline_loc2rem(utimepoint_t *est, int period)
{    
    int64_t val;

    if (!global_clk_params)
        return QOT_RETURN_TYPE_ERR;

    val = TP_TO_nSEC(est->estimate);

    // Check if this is correct -> makes the assumption that val is mostly greater than 1s (1 billion ns) (consider using floating point ops)
    if (period)
        val += (global_clk_params->mult*val)/1000000000L;
    else
    {
        val -= global_clk_params->last;
        val  = global_clk_params->nsec + val + ((global_clk_params->mult*val)/1000000000L);
    }
    TP_FROM_nSEC(est->estimate, val); 

    return QOT_RETURN_TYPE_OK;
}

/* Implementation function to compute the current timeline time */
static qot_return_t timeline_gettime(struct timespec *est)
{
    if (!est)
        return QOT_RETURN_TYPE_ERR;

    qot_return_t retval;
    utimepoint_t utp; 

    clock_gettime(CLOCK_REALTIME,est);
     
    timepoint_from_timespec(&utp.estimate, est);
    retval = qot_gl_timeline_loc2rem(&utp, 0);
    timespec_from_timepoint(est, &utp.estimate);
    return retval;
}

/* Local Timeline Posix Clock Discipline operations */
static int qot_timeline_clock_adjfreq(s32 ppb)
{
    utimepoint_t utp;
    s64 ns;

    // Get the core time
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME,&ts);
    timepoint_from_timespec(&utp.estimate, &ts);
    ns = TP_TO_nSEC(utp.estimate);

    // Write the new parameters to shared memory
    global_clk_params->nsec += (ns - global_clk_params->last)
        + (global_clk_params->mult * (ns - global_clk_params->last))/1000000000L; // ULL Changed to L -> Anon
    global_clk_params->last  = ns;
    global_clk_params->mult = (s64) ppb; // typecast added to s64
    return 0;
}

static int qot_timeline_clock_adjtime(s64 delta)
{
    utimepoint_t utp;
    s64 ns;
    
    // Get the core time
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME,&ts);
    timepoint_from_timespec(&utp.estimate, &ts);

    ns = TP_TO_nSEC(utp.estimate);
    global_clk_params->nsec += delta; 
    
    return 0;
}

static s32 qot_timeline_ppm_to_ppb(long ppm)
{
    s64 ppb = 1 + ppm;
    ppb *= 125;
    ppb >>= 13;
    return (s32) ppb;
}

static int qot_timeline_getres(struct timespec *tp)
{
    tp->tv_sec  = 0;
    tp->tv_nsec = 1;
    return 0;
}

int qot_gl_timeline_settime(const struct timespec *tp)
{
    utimepoint_t utp;
    s64 ns;
    // Get the core time
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME,&ts);
    timepoint_from_timespec(&utp.estimate, &ts);

    ns = TP_TO_nSEC(utp.estimate);
    global_clk_params->last = ns;
    global_clk_params->nsec = tp->tv_sec*1000000000LL + (s64)tp->tv_nsec;
    
    return 0;
}

int qot_gl_timeline_gettime(struct timespec *tp)
{
    utimepoint_t utp;
    s64 ns;
    s64 now;
   
    // Get the core time
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME,&ts);
    timepoint_from_timespec(&utp.estimate, &ts);

    ns = TP_TO_nSEC(utp.estimate);
    now = global_clk_params->nsec + (ns - global_clk_params->last)
          + (global_clk_params->mult * (ns - global_clk_params->last))/1000000000L; // Changed from ULL to L

    TP_FROM_nSEC(utp.estimate, now);
    timespec_from_timepoint(tp, &utp.estimate);
    return 0;
}

/* Global Variable */
long dialed_frequency = 0;

int qot_gl_timeline_adjtime(struct timex *tx)
{
    int err = -EOPNOTSUPP;
    if (tx->modes & ADJ_SETOFFSET) {
        struct timespec ts;
        s64 delta;
        ts.tv_sec = tx->time.tv_sec;
        ts.tv_nsec = tx->time.tv_usec;
        if (!(tx->modes & ADJ_NANO))
            ts.tv_nsec *= 1000;
        if ((unsigned long) ts.tv_nsec >= nSEC_PER_SEC)
            return -EINVAL;
        delta = ts.tv_sec*1000000000LL + (s64)ts.tv_nsec;
        err = qot_timeline_clock_adjtime(delta);
    } else if (tx->modes & ADJ_FREQUENCY) {
        s32 ppb = qot_timeline_ppm_to_ppb(tx->freq);
        //if (ppb > global_clk_params->max_adj || ppb < -global_clk_params->max_adj)
            //return -ERANGE;
        err = qot_timeline_clock_adjfreq(ppb);
        dialed_frequency = tx->freq;
    } else if (tx->modes == 0) {
        tx->freq = dialed_frequency;
        err = 0;
    }
    
    return err;
}

#endif