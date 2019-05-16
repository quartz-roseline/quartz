/*
  chronyd/chronyc - Programs for keeping computer clocks accurate.

 **********************************************************************
 * Copyright (C) Richard P. Curnow  1997-2003
 * Copyright (C) Miroslav Lichvar  2011, 2014-2015
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

  The routines in this file present a common local (system) clock
  interface to the rest of the software.

  They interface with the system specific driver files in sys_*.c
  */

#include "config.h"

#include "sysincl.h"

#include "conf.h"
#include "local.h"
#include "localp.h"
#include "memory.h"
#include "smooth.h"
#include "util.h"
#include "logging.h"

/* Added for the QoT Stack */
#include <pthread.h>
#include "../global_timeline.h"

#ifdef NTP_QOT_STACK
// Clock Statistics Data Point -> variable defined in chrony-3.2/local.c
extern qot_stat_t ntp_clocksync_data_point[MAX_TIMELINES];
#define LOC_DEBUG_LOG 1
#define LOC_DEBUG_FILE "/opt/qot-stack/doc/data/uncertainty.csv"
FILE* loc_outfile_fd;
int loc_outfile_flag = 0;
#endif

/* ================================================== */

/* Variable to store the current frequency, in ppm */
static double current_freq_ppm;

/* Maximum allowed frequency, in ppm */
static double max_freq_ppm;

/* Temperature compensation, in ppm */
static double temp_comp_ppm;

/* ================================================== */
/* Store the system dependent drivers */

static lcl_ReadFrequencyDriver drv_read_freq;
static lcl_SetFrequencyDriver drv_set_freq;
static lcl_AccrueOffsetDriver drv_accrue_offset;
static lcl_ApplyStepOffsetDriver drv_apply_step_offset;
static lcl_OffsetCorrectionDriver drv_offset_convert;
static lcl_SetLeapDriver drv_set_leap;
static lcl_SetSyncStatusDriver drv_set_sync_status;

/* ================================================== */

/* Types and variables associated with handling the parameter change
   list */

typedef struct _ChangeListEntry {
  struct _ChangeListEntry *next;
  struct _ChangeListEntry *prev;
  LCL_ParameterChangeHandler handler;
  void *anything;
} ChangeListEntry;

static ChangeListEntry change_list;

/* ================================================== */

/* Types and variables associated with handling the parameter change
   list */

typedef struct _DispersionNotifyListEntry {
  struct _DispersionNotifyListEntry *next;
  struct _DispersionNotifyListEntry *prev;
  LCL_DispersionNotifyHandler handler;
  void *anything;
} DispersionNotifyListEntry;

static DispersionNotifyListEntry dispersion_notify_list;

/* ================================================== */

static int precision_log;
static double precision_quantum;

static double max_clock_error;

/* ================================================== */

/* Define the number of increments of the system clock that we want
   to see to be fairly sure that we've got something approaching
   the minimum increment.  Even on a crummy implementation that can't
   interpolate between 10ms ticks, we should get this done in
   under 1s of busy waiting. */
#define NITERS 100

#define NSEC_PER_SEC 1000000000

static void
calculate_sys_precision(void)
{
  struct timespec ts, old_ts;
  int iters, diff, best;

  LCL_ReadRawTime(&old_ts);

  /* Assume we must be better than a second */
  best = NSEC_PER_SEC;
  iters = 0;

  do {
    LCL_ReadRawTime(&ts);

    diff = NSEC_PER_SEC * (ts.tv_sec - old_ts.tv_sec) + (ts.tv_nsec - old_ts.tv_nsec);

    old_ts = ts;
    if (diff > 0) {
      if (diff < best)
        best = diff;
      iters++;
    }
  } while (iters < NITERS);

  assert(best > 0);

  precision_quantum = 1.0e-9 * best;

  /* Get rounded log2 value of the measured precision */
  precision_log = 0;
  while (best < 707106781) {
    precision_log--;
    best *= 2;
  }

  assert(precision_log >= -30);

  DEBUG_LOG("Clock precision %.9f (%d)", precision_quantum, precision_log);
}

/* ================================================== */

void
LCL_Initialise(void)
{
  change_list.next = change_list.prev = &change_list;

  dispersion_notify_list.next = dispersion_notify_list.prev = &dispersion_notify_list;

  /* Null out the system drivers, so that we die
     if they never get defined before use */
  
  drv_read_freq = NULL;
  drv_set_freq = NULL;
  drv_accrue_offset = NULL;
  drv_offset_convert = NULL;

  /* This ought to be set from the system driver layer */
  current_freq_ppm = 0.0;
  temp_comp_ppm = 0.0;

  calculate_sys_precision();

  /* This is the maximum allowed frequency offset in ppm, the time must
     never stop or run backwards */
  max_freq_ppm = CNF_GetMaxDrift();
  max_freq_ppm = CLAMP(0.0, max_freq_ppm, 500000.0);

  max_clock_error = CNF_GetMaxClockError() * 1e-6;
}

#ifdef NTP_QOT_STACK
/* ================================================== */
/* Added for the QoT Stack */

void
LCL_Initialise_GlobalTimeline(int timelineid, int *timelinesfd)
{
  global_timelineid  = timelineid;

  #ifndef QOT_TIMELINE_SERVICE
  global_timelinefd  = timelinesfd[0]; 
  global_tmlclkid    = FD_TO_CLOCKID(global_timelinefd);

  struct timespec now, tl_now;

  clock_gettime(CLOCK_REALTIME, &now);
  clock_gettime(global_tmlclkid, &tl_now);

  printf("Initial Clock Status .....\n");
  printf("CLOCK_REALTIME %lld.%9llu\n", now.tv_sec, now.tv_nsec);
  printf("TIMELINE_TIME  %lld.%9llu\n", tl_now.tv_sec, tl_now.tv_nsec);
  #endif

  #ifdef NTP_QOT_STACK
  if (LOC_DEBUG_LOG)
  {
    loc_outfile_fd = fopen(LOC_DEBUG_FILE, "w");
    if (loc_outfile_fd < 0) {
      fprintf(stderr, "opening %s: %s\n", LOC_DEBUG_FILE, strerror(errno));
    }
    else
    {
      printf("Opened LCLclk debug file %s\n", LOC_DEBUG_FILE);
      loc_outfile_flag = 1;
    }
  }
  #endif

  change_list.next = change_list.prev = &change_list;

  dispersion_notify_list.next = dispersion_notify_list.prev = &dispersion_notify_list;

  /* Null out the system drivers, so that we die
     if they never get defined before use */
  
  drv_read_freq = NULL;
  drv_set_freq = NULL;
  drv_accrue_offset = NULL;
  drv_offset_convert = NULL;

  /* This ought to be set from the system driver layer */
  current_freq_ppm = 0.0;
  temp_comp_ppm = 0.0;

  calculate_sys_precision();

  /* This is the maximum allowed frequency offset in ppm, the time must
     never stop or run backwards */
  max_freq_ppm = CNF_GetMaxDrift();
  max_freq_ppm = CLAMP(0.0, max_freq_ppm, 500000.0);

  max_clock_error = CNF_GetMaxClockError() * 1e-6;
}

/* ================================================== */
#endif

void
LCL_Finalise(void)
{
  while (change_list.next != &change_list)
    LCL_RemoveParameterChangeHandler(change_list.next->handler,
                                     change_list.next->anything);

  while (dispersion_notify_list.next != &dispersion_notify_list)
    LCL_RemoveDispersionNotifyHandler(dispersion_notify_list.next->handler,
                                      dispersion_notify_list.next->anything);

  #ifdef NTP_QOT_STACK
  if (loc_outfile_flag)
  {
    fclose(loc_outfile_fd);
  }
  #endif
}

/* ================================================== */

/* Routine to read the system precision as a log to base 2 value. */
int
LCL_GetSysPrecisionAsLog(void)
{
  return precision_log;
}

/* ================================================== */
/* Routine to read the system precision in terms of the actual time step */

double
LCL_GetSysPrecisionAsQuantum(void)
{
  return precision_quantum;
}

/* ================================================== */

double
LCL_GetMaxClockError(void)
{
  return max_clock_error;
}

/* ================================================== */

void
LCL_AddParameterChangeHandler(LCL_ParameterChangeHandler handler, void *anything)
{
  ChangeListEntry *ptr, *new_entry;

  /* Check that the handler is not already registered */
  for (ptr = change_list.next; ptr != &change_list; ptr = ptr->next) {
    if (!(ptr->handler != handler || ptr->anything != anything)) {
      assert(0);
    }
  }

  new_entry = MallocNew(ChangeListEntry);

  new_entry->handler = handler;
  new_entry->anything = anything;

  /* Chain it into the list */
  new_entry->next = &change_list;
  new_entry->prev = change_list.prev;
  change_list.prev->next = new_entry;
  change_list.prev = new_entry;
}

/* ================================================== */

/* Remove a handler */
void LCL_RemoveParameterChangeHandler(LCL_ParameterChangeHandler handler, void *anything)
{

  ChangeListEntry *ptr;
  int ok;

  ptr = NULL;
  ok = 0;

  for (ptr = change_list.next; ptr != &change_list; ptr = ptr->next) {
    if (ptr->handler == handler && ptr->anything == anything) {
      ok = 1;
      break;
    }
  }

  assert(ok);

  /* Unlink entry from the list */
  ptr->next->prev = ptr->prev;
  ptr->prev->next = ptr->next;

  Free(ptr);
}

/* ================================================== */

int
LCL_IsFirstParameterChangeHandler(LCL_ParameterChangeHandler handler)
{
  return change_list.next->handler == handler;
}

/* ================================================== */

static void
invoke_parameter_change_handlers(struct timespec *raw, struct timespec *cooked,
                                 double dfreq, double doffset,
                                 LCL_ChangeType change_type)
{
  ChangeListEntry *ptr;

  for (ptr = change_list.next; ptr != &change_list; ptr = ptr->next) {
    (ptr->handler)(raw, cooked, dfreq, doffset, change_type, ptr->anything);
  }
}

/* ================================================== */

void
LCL_AddDispersionNotifyHandler(LCL_DispersionNotifyHandler handler, void *anything)
{
  DispersionNotifyListEntry *ptr, *new_entry;

  /* Check that the handler is not already registered */
  for (ptr = dispersion_notify_list.next; ptr != &dispersion_notify_list; ptr = ptr->next) {
    if (!(ptr->handler != handler || ptr->anything != anything)) {
      assert(0);
    }
  }

  new_entry = MallocNew(DispersionNotifyListEntry);

  new_entry->handler = handler;
  new_entry->anything = anything;

  /* Chain it into the list */
  new_entry->next = &dispersion_notify_list;
  new_entry->prev = dispersion_notify_list.prev;
  dispersion_notify_list.prev->next = new_entry;
  dispersion_notify_list.prev = new_entry;
}

/* ================================================== */

/* Remove a handler */
extern 
void LCL_RemoveDispersionNotifyHandler(LCL_DispersionNotifyHandler handler, void *anything)
{

  DispersionNotifyListEntry *ptr;
  int ok;

  ptr = NULL;
  ok = 0;

  for (ptr = dispersion_notify_list.next; ptr != &dispersion_notify_list; ptr = ptr->next) {
    if (ptr->handler == handler && ptr->anything == anything) {
      ok = 1;
      break;
    }
  }

  assert(ok);

  /* Unlink entry from the list */
  ptr->next->prev = ptr->prev;
  ptr->prev->next = ptr->next;

  Free(ptr);
}

/* ================================================== */

void
LCL_ReadRawTime(struct timespec *ts)
{
#if HAVE_CLOCK_GETTIME
  if (clock_gettime(CLOCK_REALTIME, ts) < 0)
    LOG_FATAL("clock_gettime() failed : %s", strerror(errno));
#else
  struct timeval tv;

  if (gettimeofday(&tv, NULL) < 0)
    LOG_FATAL("gettimeofday() failed : %s", strerror(errno));

  UTI_TimevalToTimespec(&tv, ts);
#endif
}

/* ================================================== */

void
LCL_ReadCookedTime(struct timespec *result, double *err)
{
  struct timespec raw;

  LCL_ReadRawTime(&raw);
  LCL_CookTime(&raw, result, err);
}

/* ================================================== */

void
LCL_CookTime(struct timespec *raw, struct timespec *cooked, double *err)
{
  double correction;

  LCL_GetOffsetCorrection(raw, &correction, err);
  UTI_AddDoubleToTimespec(raw, correction, cooked);
}

/* ================================================== */

void
LCL_GetOffsetCorrection(struct timespec *raw, double *correction, double *err)
{
  /* Call system specific driver to get correction */
  (*drv_offset_convert)(raw, correction, err);
}

/* ================================================== */
/* Return current frequency */

double
LCL_ReadAbsoluteFrequency(void)
{
  double freq;

  freq = current_freq_ppm; 

  /* Undo temperature compensation */
  if (temp_comp_ppm != 0.0) {
    freq = (freq + temp_comp_ppm) / (1.0 - 1.0e-6 * temp_comp_ppm);
  }

  return freq;
}

/* ================================================== */

static double
clamp_freq(double freq)
{
  if (freq <= max_freq_ppm && freq >= -max_freq_ppm)
    return freq;

  LOG(LOGS_WARN, "Frequency %.1f ppm exceeds allowed maximum", freq);

  return CLAMP(-max_freq_ppm, freq, max_freq_ppm);
}

/* ================================================== */

static int
check_offset(struct timespec *now, double offset)
{
  /* Check if the time will be still sane with accumulated offset */
  if (UTI_IsTimeOffsetSane(now, -offset))
      return 1;

  LOG(LOGS_WARN, "Adjustment of %.1f seconds is invalid", -offset);
  return 0;
}

/* ================================================== */

/* This involves both setting the absolute frequency with the
   system-specific driver, as well as calling all notify handlers */

void
LCL_SetAbsoluteFrequency(double afreq_ppm)
{
  struct timespec raw, cooked;
  double dfreq;
  
  afreq_ppm = clamp_freq(afreq_ppm);

  /* Apply temperature compensation */
  if (temp_comp_ppm != 0.0) {
    afreq_ppm = afreq_ppm * (1.0 - 1.0e-6 * temp_comp_ppm) - temp_comp_ppm;
  }

  /* Call the system-specific driver for setting the frequency */
  
  afreq_ppm = (*drv_set_freq)(afreq_ppm);

  dfreq = (afreq_ppm - current_freq_ppm) / (1.0e6 - current_freq_ppm);

  LCL_ReadRawTime(&raw);
  LCL_CookTime(&raw, &cooked, NULL);

  /* Dispatch to all handlers */
  invoke_parameter_change_handlers(&raw, &cooked, dfreq, 0.0, LCL_ChangeAdjust);

  current_freq_ppm = afreq_ppm;

}

/* ================================================== */

void
LCL_AccumulateDeltaFrequency(double dfreq)
{
  struct timespec raw, cooked;
  double old_freq_ppm;

  old_freq_ppm = current_freq_ppm;

  /* Work out new absolute frequency.  Note that absolute frequencies
   are handled in units of ppm, whereas the 'dfreq' argument is in
   terms of the gradient of the (offset) v (local time) function. */

  current_freq_ppm += dfreq * (1.0e6 - current_freq_ppm);

  current_freq_ppm = clamp_freq(current_freq_ppm);

  /* Call the system-specific driver for setting the frequency */
  current_freq_ppm = (*drv_set_freq)(current_freq_ppm);
  dfreq = (current_freq_ppm - old_freq_ppm) / (1.0e6 - old_freq_ppm);

  LCL_ReadRawTime(&raw);
  LCL_CookTime(&raw, &cooked, NULL);

  /* Dispatch to all handlers */
  invoke_parameter_change_handlers(&raw, &cooked, dfreq, 0.0, LCL_ChangeAdjust);
}

/* ================================================== */
/* Global Variable for Sharing Computed Clock Statistic from Sync to Uncertainty Calculation */
qot_stat_t ntp_clocksync_data_point[MAX_TIMELINES];
qot_stat_t last_clocksync_data_point[MAX_TIMELINES];

void
LCL_AccumulateOffset(double offset, double corr_rate)
{
  struct timespec raw, cooked;

  /* In this case, the cooked time to be passed to the notify clients
     has to be the cooked time BEFORE the change was made */

  LCL_ReadRawTime(&raw);
  LCL_CookTime(&raw, &cooked, NULL);

  if (!check_offset(&cooked, offset))
      return;

  (*drv_accrue_offset)(offset, corr_rate);

  /* Dispatch to all handlers */
  invoke_parameter_change_handlers(&raw, &cooked, 0.0, offset, LCL_ChangeAdjust);
}

/* ================================================== */

int
LCL_ApplyStepOffset(double offset)
{
  struct timespec raw, cooked;

  /* In this case, the cooked time to be passed to the notify clients
     has to be the cooked time BEFORE the change was made */

  LCL_ReadRawTime(&raw);
  LCL_CookTime(&raw, &cooked, NULL);

  if (!check_offset(&raw, offset))
      return 0;

  if (!(*drv_apply_step_offset)(offset)) {
    LOG(LOGS_ERR, "Could not step system clock");
    return 0;
  }

  /* Reset smoothing on all clock steps */
  SMT_Reset(&cooked);

  /* Dispatch to all handlers */
  invoke_parameter_change_handlers(&raw, &cooked, 0.0, offset, LCL_ChangeStep);

  return 1;
}

/* ================================================== */

void
LCL_NotifyExternalTimeStep(struct timespec *raw, struct timespec *cooked,
    double offset, double dispersion)
{
  /* Dispatch to all handlers */
  invoke_parameter_change_handlers(raw, cooked, 0.0, offset, LCL_ChangeUnknownStep);

  lcl_InvokeDispersionNotifyHandlers(dispersion);
}

/* ================================================== */

void
LCL_NotifyLeap(int leap)
{
  struct timespec raw, cooked;

  LCL_ReadRawTime(&raw);
  LCL_CookTime(&raw, &cooked, NULL);

  /* Smooth the leap second out */
  SMT_Leap(&cooked, leap);

  /* Dispatch to all handlers as if the clock was stepped */
  invoke_parameter_change_handlers(&raw, &cooked, 0.0, -leap, LCL_ChangeStep);
}

/* ================================================== */

void
LCL_AccumulateFrequencyAndOffset(double dfreq, double doffset, double corr_rate)
{
  struct timespec raw, cooked;
  double old_freq_ppm;

  LCL_ReadRawTime(&raw);
  /* Due to modifying the offset, this has to be the cooked time prior
     to the change we are about to make */
  LCL_CookTime(&raw, &cooked, NULL);

  if (!check_offset(&cooked, doffset))
      return;

  old_freq_ppm = current_freq_ppm;

  /* Work out new absolute frequency.  Note that absolute frequencies
   are handled in units of ppm, whereas the 'dfreq' argument is in
   terms of the gradient of the (offset) v (local time) function. */
  current_freq_ppm += dfreq * (1.0e6 - current_freq_ppm);

  current_freq_ppm = clamp_freq(current_freq_ppm);

  DEBUG_LOG("old_freq=%.3fppm new_freq=%.3fppm offset=%.6fsec",
      old_freq_ppm, current_freq_ppm, doffset);

  /* Call the system-specific driver for setting the frequency */
  current_freq_ppm = (*drv_set_freq)(current_freq_ppm);
  dfreq = (current_freq_ppm - old_freq_ppm) / (1.0e6 - old_freq_ppm);

  (*drv_accrue_offset)(doffset, corr_rate);

  /* Dispatch to all handlers */
  invoke_parameter_change_handlers(&raw, &cooked, dfreq, doffset, LCL_ChangeAdjust);
}

/* ================================================== */

void
lcl_InvokeDispersionNotifyHandlers(double dispersion)
{
  DispersionNotifyListEntry *ptr;

  for (ptr = dispersion_notify_list.next; ptr != &dispersion_notify_list; ptr = ptr->next) {
    (ptr->handler)(dispersion, ptr->anything);
  }

}

/* ================================================== */

void
lcl_RegisterSystemDrivers(lcl_ReadFrequencyDriver read_freq,
                          lcl_SetFrequencyDriver set_freq,
                          lcl_AccrueOffsetDriver accrue_offset,
                          lcl_ApplyStepOffsetDriver apply_step_offset,
                          lcl_OffsetCorrectionDriver offset_convert,
                          lcl_SetLeapDriver set_leap,
                          lcl_SetSyncStatusDriver set_sync_status)
{
  drv_read_freq = read_freq;
  drv_set_freq = set_freq;
  drv_accrue_offset = accrue_offset;
  drv_apply_step_offset = apply_step_offset;
  drv_offset_convert = offset_convert;
  drv_set_leap = set_leap;
  drv_set_sync_status = set_sync_status;
  current_freq_ppm = (*drv_read_freq)();

  DEBUG_LOG("Local freq=%.3fppm", current_freq_ppm);
}

/* ================================================== */
/* Look at the current difference between the system time and the NTP
   time, and make a step to cancel it. */

int
LCL_MakeStep(void)
{
  struct timespec raw;
  double correction;

  LCL_ReadRawTime(&raw);
  LCL_GetOffsetCorrection(&raw, &correction, NULL);

  if (!check_offset(&raw, -correction))
      return 0;

  /* Cancel remaining slew and make the step */
  LCL_AccumulateOffset(correction, 0.0);
  if (!LCL_ApplyStepOffset(-correction))
    return 0;

  LOG(LOGS_WARN, "System clock was stepped by %.6f seconds", correction);

  return 1;
}

/* ================================================== */

int
LCL_CanSystemLeap(void)
{
  return drv_set_leap ? 1 : 0;
}

/* ================================================== */

void
LCL_SetSystemLeap(int leap, int tai_offset)
{
  if (drv_set_leap) {
    (drv_set_leap)(leap, tai_offset);
  }
}

/* ================================================== */

double
LCL_SetTempComp(double comp)
{
  double uncomp_freq_ppm;

  if (temp_comp_ppm == comp)
    return comp;

  /* Undo previous compensation */
  current_freq_ppm = (current_freq_ppm + temp_comp_ppm) /
    (1.0 - 1.0e-6 * temp_comp_ppm);

  uncomp_freq_ppm = current_freq_ppm;

  /* Apply new compensation */
  current_freq_ppm = current_freq_ppm * (1.0 - 1.0e-6 * comp) - comp;

  /* Call the system-specific driver for setting the frequency */
  current_freq_ppm = (*drv_set_freq)(current_freq_ppm);

  temp_comp_ppm = (uncomp_freq_ppm - current_freq_ppm) /
    (1.0e-6 * uncomp_freq_ppm + 1.0);

  return temp_comp_ppm;
}

/* ================================================== */

void
LCL_SetSyncStatus(int synchronised, double est_error, double max_error)
{
  if (drv_set_sync_status) {
    (drv_set_sync_status)(synchronised, est_error, max_error);
  }
}

/* ================================================== */

#ifdef NTP_QOT_STACK
/* Uncertainty lock and condition variable */
pthread_mutex_t uncertainty_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t uncertainty_condvar = PTHREAD_COND_INITIALIZER;

/* Set Uncertainty Variables directly using NTP Dispersion*/
void LCL_SetDispUncertaintyParams(struct timespec our_ref_time, double our_root_dispersion, double our_skew, double our_residual_freq)
{
  /* Enable only if peer-dispersion-based QoT estimation is on */
  #ifdef QOT_PEER_DISP
    #ifdef QOT_TIMELINE_SERVICE
    
      if (!global_clk_params)
        return;

      //#ifndef QUARTZ_V1 // New code added
      #ifdef SYNC_PRIVELEGED
        global_clk_params->last = our_ref_time.tv_sec*1000000000LL + (int64_t)our_ref_time.tv_nsec;
        global_clk_params->nsec = global_clk_params->last;
      #endif

      // Write the Parameters to the memory location
      global_clk_params->u_nsec = (int64_t)ceil(our_root_dispersion*1000000000LL);
      global_clk_params->l_nsec = global_clk_params->u_nsec;  // Take care of negative sign here only -> Kernel Space implementation does it in the kernel
      global_clk_params->u_mult = (int64_t)((our_skew + fabs(our_residual_freq) + LCL_GetMaxClockError())*1000000000LL);
      global_clk_params->l_mult = global_clk_params->u_mult;

    /* NTP uses the following formula to calculate root dispersion (uncertainty w.r.t stratum 1)
     our_root_dispersion + fabs(UTI_DiffTimespecsToDouble(ts, &our_ref_time))*(our_skew + fabs(our_residual_freq) + LCL_GetMaxClockError()); */

    #endif

  if (loc_outfile_flag && LOC_DEBUG_LOG)
  {
    fprintf(loc_outfile_fd, "%lld,%lld,%lld,%lld\n", global_clk_params->u_nsec, global_clk_params->u_mult, global_clk_params->last, global_clk_params->nsec); 
  }

  #endif
  return;
}

void LCL_SetUncertainty(double dfreq, double offset)
{
  /* Disable the parameter collection if the OoT estimation is using NTP Peer Dispersion */
  #ifndef QOT_PEER_DISP
    double freq_ppm;
    // freq_ppm = current_freq_ppm + dfreq * (1.0e6 - current_freq_ppm);
    freq_ppm = dfreq * (1.0e6 - current_freq_ppm);

    #ifdef QOT_TIMELINE_SERVICE
      //#ifndef QUARTZ_V1 // New code added
      #ifdef SYNC_PRIVELEGED
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME,&ts);
        if (!global_clk_params)
          return;
        global_clk_params->last = ts.tv_sec*1000000000LL + (int64_t)ts.tv_nsec;
        global_clk_params->nsec = global_clk_params->last;
      #endif
    #endif

    pthread_mutex_lock(&uncertainty_lock);
    // Add Statistic for the QoT Uncertainty Service to process
    ntp_clocksync_data_point[global_timelineid].offset = (int64_t)ceil(offset*1.0e9);
    ntp_clocksync_data_point[global_timelineid].drift = (int64_t)ceil(freq_ppm*1.0e3); // Convert PPM to PPB
    ntp_clocksync_data_point[global_timelineid].data_id++;

    // Signal the NTP18 uncertainty thread that a new data poin has been added
    pthread_cond_signal(&uncertainty_condvar);

    pthread_mutex_unlock(&uncertainty_lock);

  #endif
  return;
}
#endif