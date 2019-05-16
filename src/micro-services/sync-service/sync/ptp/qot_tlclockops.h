/**
 * @file qot_tlclockops.h
 * @brief Timeline Clock Operations Header
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
#ifndef QOT_TL_CLKOPS_PTP_H
#define QOT_TL_CLKOPS_PTP_H

#include "../../../../qot_types.h"

/* Set the PHC clock */
int qot_set_phc(clockid_t phc_clockid);

/* Convert from core time to timeline time */
qot_return_t qot_loc2rem(utimepoint_t *est, int period, tl_translation_t* clk_params);

/* Set the timeline time */
int qot_timeline_settime(const struct timespec *tp, tl_translation_t* clk_params);

/* Reat the timeline time */
int qot_timeline_gettime(struct timespec *tp, tl_translation_t* clk_params);

/* Adjust the timeline time */
int qot_timeline_adjtime(struct timex *tx, tl_translation_t* clk_params);

#endif
