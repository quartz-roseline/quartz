/*
  chronyd/chronyc - Programs for keeping computer clocks accurate.

 **********************************************************************
 * Copyright (C) Miroslav Lichvar  2016-2017
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 **********************************************************************

  =======================================================================

  Tracking of hardware clocks (e.g. RTC, PHC)
  */

#include "config.h"

#include "sysincl.h"

#include "array.h"
#include "hwclock.h"
#include "local.h"
#include "logging.h"
#include "memory.h"
#include "regress.h"
#include "util.h"

/* Added for the QoT Stack */
#include <pthread.h>
#include "../global_timeline.h"

#ifdef NTP_QOT_STACK
// Clock Statistics Data Point -> variable defined in chrony-3.2/local.c
extern qot_stat_t ntp_clocksync_data_point[MAX_TIMELINES];
#define QOT_DEBUG_LOG 1
#define QOT_DEBUG_FILE "/opt/qot-stack/doc/data/phcclkrtmap.csv"
FILE* outfile_fd;
int outfile_flag = 0;
#endif

/* Maximum number of samples per clock */
#define MAX_SAMPLES 16

/* Maximum acceptable frequency offset of the clock */
#define MAX_FREQ_OFFSET (2.0 / 3.0)

struct HCL_Instance_Record {
  /* HW and local reference timestamp */
  struct timespec hw_ref;
  struct timespec local_ref;

  /* Samples stored as intervals (uncorrected for frequency error)
     relative to local_ref and hw_ref */
  double x_data[MAX_SAMPLES];
  double y_data[MAX_SAMPLES];

  /* Number of samples */
  int n_samples;

  /* Maximum error of the last sample */
  double last_err;

  /* Minimum interval between samples */
  double min_separation;

  /* Flag indicating the offset and frequency values are valid */
  int valid_coefs;

  /* Estimated offset and frequency of HW clock relative to local clock */
  double offset;
  double frequency;
};

/* ================================================== */

static void
handle_slew(struct timespec *raw, struct timespec *cooked, double dfreq,
            double doffset, LCL_ChangeType change_type, void *anything)
{
  HCL_Instance clock;
  double delta;

  clock = anything;

  if (clock->n_samples)
    UTI_AdjustTimespec(&clock->local_ref, cooked, &clock->local_ref, &delta, dfreq, doffset);
  if (clock->valid_coefs)
    clock->frequency /= 1.0 - dfreq;
}

/* ================================================== */

HCL_Instance
HCL_CreateInstance(double min_separation)
{
  HCL_Instance clock;

  clock = MallocNew(struct HCL_Instance_Record);
  clock->x_data[MAX_SAMPLES - 1] = 0.0;
  clock->y_data[MAX_SAMPLES - 1] = 0.0;
  clock->n_samples = 0;
  clock->valid_coefs = 0;
  clock->min_separation = min_separation;

  LCL_AddParameterChangeHandler(handle_slew, clock);

  #ifdef NTP_QOT_STACK
  if (QOT_DEBUG_LOG)
  {
    outfile_fd = fopen(QOT_DEBUG_FILE, "w");
    if (outfile_fd < 0) {
      fprintf(stderr, "opening %s: %s\n", QOT_DEBUG_FILE, strerror(errno));
    }
    else
    {
      printf("Opened HWclock debug file %s\n", QOT_DEBUG_FILE);
      outfile_flag = 1;
    }
  }
  #endif

  return clock;
}

/* ================================================== */

void HCL_DestroyInstance(HCL_Instance clock)
{
  LCL_RemoveParameterChangeHandler(handle_slew, clock);
  Free(clock);
  #ifdef NTP_QOT_STACK
  if (outfile_flag)
  {
    fclose(outfile_fd);
  }
  #endif
}

/* ================================================== */

int
HCL_NeedsNewSample(HCL_Instance clock, struct timespec *now)
{
  if (!clock->n_samples ||
      fabs(UTI_DiffTimespecsToDouble(now, &clock->local_ref)) >= clock->min_separation)
    return 1;

  return 0;
}

/* ================================================== */

#ifdef NTP_QOT_STACK
/* Uncertainty lock and condition variable */
pthread_mutex_t loc_uncertainty_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t loc_uncertainty_condvar = PTHREAD_COND_INITIALIZER;

void HCL_SetUncertainty(int64_t freq_ppb, int64_t offset)
{
  fake_local_timelineid = 1; // Defaults to 1

  pthread_mutex_lock(&loc_uncertainty_lock);
  // Add Statistic for the QoT Uncertainty Service to process
  ntp_clocksync_data_point[fake_local_timelineid].offset = offset;//(int64_t)ceil(offset*1.0e9);
  ntp_clocksync_data_point[fake_local_timelineid].drift = freq_ppb;
  ntp_clocksync_data_point[fake_local_timelineid].data_id++;

  // Signal the NTP18 local uncertainty thread that a new data poin has been added
  pthread_cond_signal(&loc_uncertainty_condvar);

  pthread_mutex_unlock(&loc_uncertainty_lock);
}

// Translate from clock realtime to PHC using a set of parameters
int64_t clockrt_to_phc(tl_translation_t* clk_params, int64_t timestamp)
{
  int64_t val = timestamp;
  val -= clk_params->last;
  val  = clk_params->nsec + val + ((clk_params->mult*val)/1000000000L);
  return val;
}
#endif

void
HCL_AccumulateSample(HCL_Instance clock, struct timespec *hw_ts,
                     struct timespec *local_ts, double err)
{
  double hw_delta, local_delta, local_freq, raw_freq;
  int i, n_runs, best_start;

  local_freq = 1.0 - LCL_ReadAbsoluteFrequency() / 1.0e6;

  /* Shift old samples */
  if (clock->n_samples) {
    if (clock->n_samples >= MAX_SAMPLES)
      clock->n_samples--;

    hw_delta = UTI_DiffTimespecsToDouble(hw_ts, &clock->hw_ref);
    local_delta = UTI_DiffTimespecsToDouble(local_ts, &clock->local_ref) / local_freq;

    if (hw_delta <= 0.0 || local_delta < clock->min_separation / 2.0) {
      clock->n_samples = 0;
      DEBUG_LOG("HW clock reset interval=%f", local_delta);
    }

    for (i = MAX_SAMPLES - clock->n_samples; i < MAX_SAMPLES; i++) {
      clock->y_data[i - 1] = clock->y_data[i] - hw_delta;
      clock->x_data[i - 1] = clock->x_data[i] - local_delta;
    }
  }

  clock->n_samples++;
  clock->hw_ref = *hw_ts;
  clock->local_ref = *local_ts;
  clock->last_err = err;

  /* Get new coefficients */
  clock->valid_coefs =
    RGR_FindBestRobustRegression(clock->x_data + MAX_SAMPLES - clock->n_samples,
                                 clock->y_data + MAX_SAMPLES - clock->n_samples,
                                 clock->n_samples, 1.0e-10, &clock->offset, &raw_freq,
                                 &n_runs, &best_start);

  if (!clock->valid_coefs) {
    DEBUG_LOG("HW clock needs more samples");
    return;
  }

  clock->frequency = raw_freq / local_freq;

  /* Drop unneeded samples */
  clock->n_samples -= best_start;

  #ifdef NTP_QOT_STACK
  struct timespec nsec;
  // tl_translation_t old_params;
  // int64_t proj_offset;
  // Add the parameters to the local timeline data structure
  if (local_clk_params != NULL)
  {
    // old_params = *local_clk_params;
    local_clk_params->last = clock->local_ref.tv_sec*1000000000ULL + clock->local_ref.tv_nsec;                            
    local_clk_params->mult = (clock->frequency - 1.0)*1000000000;   
    UTI_AddDoubleToTimespec(&clock->hw_ref, clock->frequency*clock->offset, &nsec);                         
    local_clk_params->nsec = nsec.tv_sec*1000000000ULL + nsec.tv_nsec; 
    // proj_offset = clockrt_to_phc(local_clk_params, local_clk_params->last) - clockrt_to_phc(&old_params, local_clk_params->last);
    HCL_SetUncertainty((int64_t)((clock->frequency - 1.0)*1000000000), (int64_t)ceil(clock->offset*1.0e9));
    // HCL_SetUncertainty((int64_t)((clock->frequency - 1.0)*1000000000), proj_offset);
    printf("New local->HW clock parameters added last = %lld mult = %lld nsec = %lld\n", local_clk_params->last, local_clk_params->mult, local_clk_params->nsec); 
  }
  printf("HW clock samples=%d offset=%e freq=%e raw_freq=%e err=%e ref_diff=%e\n",
            clock->n_samples, clock->offset, clock->frequency - 1.0, raw_freq - 1.0, err,
            UTI_DiffTimespecsToDouble(&clock->hw_ref, &clock->local_ref));
  if (outfile_flag && QOT_DEBUG_LOG)
  {
    fprintf(outfile_fd, "%ld,%09ld,%ld,%09ld,%f,%lld\n", clock->local_ref.tv_sec, clock->local_ref.tv_nsec,clock->hw_ref.tv_sec, clock->hw_ref.tv_nsec, clock->frequency, (int64_t)ceil(clock->offset*1.0e9)); 
  }
  #endif



  /* If the fit doesn't cross the error interval of the last sample,
     or the frequency is not sane, drop all samples and start again */
  if (fabs(clock->offset) > err ||
      fabs(clock->frequency - 1.0) > MAX_FREQ_OFFSET) {
    DEBUG_LOG("HW clock reset");
    clock->n_samples = 0;
    clock->valid_coefs = 0;
  }

  

  DEBUG_LOG("HW clock samples=%d offset=%e freq=%e raw_freq=%e err=%e ref_diff=%e",
            clock->n_samples, clock->offset, clock->frequency - 1.0, raw_freq - 1.0, err,
            UTI_DiffTimespecsToDouble(&clock->hw_ref, &clock->local_ref));
}

/* ================================================== */

int
HCL_CookTime(HCL_Instance clock, struct timespec *raw, struct timespec *cooked, double *err)
{
  double offset, elapsed;

  if (!clock->valid_coefs)
    return 0;

  elapsed = UTI_DiffTimespecsToDouble(raw, &clock->hw_ref);
  offset = elapsed / clock->frequency - clock->offset;
  UTI_AddDoubleToTimespec(&clock->local_ref, offset, cooked);

  /* Fow now, just return the error of the last sample */
  if (err)
    *err = clock->last_err;

  return 1;
}
