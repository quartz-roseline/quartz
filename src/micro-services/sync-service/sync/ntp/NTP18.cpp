/**
 * @file NTP18.cpp
 * @brief Provides ntp instance based on Chrony to the sync interface
 * @author Anon D'Anon
 * 
 * Copyright (c) Anon, 2018. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice, 
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND f
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
 * Reference: Based on the chrony implementation of NTP
 * 1. chrony: https://chrony.tuxfamily.org/
 */

#include "NTP18.hpp"

#include "global_timeline.h"

extern "C"
{
  #include "chrony-3.2/reference.h"
  #include "chrony-3.2/sources.h"
  #include "chrony-3.2/client_chronyc.h"
  #include "chrony-3.2/ntp_sources.h"
}

#include "../../qot_sync_service.hpp"

#include <map>
#include <mutex>

/* ================================================== */

/* Set when the initialisation chain has been completed.  Prevents finalisation
 * chain being run if a fatal error happened early. */

static int initialised = 0;

static int exit_status = 0;

static int reload = 0;

static REF_Mode ref_mode = REF_ModeNormal;

/* Global NTP Configuration file */
std::string conf_file = DEFAULT_CONF_FILE;

/* Clock Parameters for the local timeline and global timeline clocks */
tl_translation_t* global_clk_params = NULL;
tl_translation_t* local_clk_params = NULL;

/* Data Structure indicating Server Status*/
typedef struct qot_server_data {
  int64_t accuracy;           /* Required QoT */
  bool flag;                  /* Flag indicating server is set */
  std::string server_ip;      /* Server IP Address */
  std::string timeline_uuid;  /* Timeline UUID */
  int good_data_counter;      /* Consecutive Good Data counter */
} qot_sdata_t;

/* Global Timeline QoT Mapping */
std::map<int, qot_sdata_t> timeline_qotmap;
std::mutex qotmap_lock;

/* ================================================== */

static void
do_platform_checks(void)
{
  /* Require at least 32-bit integers, two's complement representation and
     the usual implementation of conversion of unsigned integers */
  assert(sizeof (int) >= 4);
  assert(-1 == ~0);
  assert((int32_t)4294967295U == (int32_t)-1);
}

/* ================================================== */

static void
delete_pidfile(void)
{
  const char *pidfile = CNF_GetPidFile();

  if (!pidfile[0])
    return;

  /* Don't care if this fails, there's not a lot we can do */
  unlink(pidfile);
}

/* ================================================== */

void
MAI_CleanupAndExit(void)
{
  if (!initialised) exit(exit_status);
  
  if (CNF_GetDumpDir()[0] != '\0') {
    SRC_DumpSources();
  }

  /* Don't update clock when removing sources */
  REF_SetMode(REF_ModeIgnore);

  SMT_Finalise();
  TMC_Finalise();
  MNL_Finalise();
  CLG_Finalise();
  NSD_Finalise();
  NSR_Finalise();
  SST_Finalise();
  NCR_Finalise();
  NIO_Finalise();
  CAM_Finalise();
  KEY_Finalise();
  RCL_Finalise();
  SRC_Finalise();
  REF_Finalise();
  RTC_Finalise();
  SYS_Finalise();
  SCH_Finalise();
  LCL_Finalise();
  PRV_Finalise();

  delete_pidfile();
  
  CNF_Finalise();
  HSH_Finalise();
  LOG_Finalise();

  #ifndef NTP_QOT_STACK
  exit(exit_status);
  #endif
}

/* ================================================== */

static int need_to_exit_prog = 0;

static void
signal_cleanup(int x)
{
  if (!initialised) exit(0);
  SCH_QuitProgram();
  need_to_exit_prog = 1;

  // To kill the sync service on a ctrl + C
  #ifdef NTP_QOT_STACK 
  #ifdef QOT_TIMELINE_SERVICE
  sync_service_running = 0;
  #endif
  #endif
}

/* ================================================== */

static void
quit_timeout(void *arg)
{
  /* Return with non-zero status if the clock is not synchronised */
  exit_status = REF_GetOurStratum() >= NTP_MAX_STRATUM;
  SCH_QuitProgram();
}

/* ================================================== */

static void
ntp_source_resolving_end(void)
{
  NSR_SetSourceResolvingEndHandler(NULL);

  if (reload) {
    /* Note, we want reload to come well after the initialisation from
       the real time clock - this gives us a fighting chance that the
       system-clock scale for the reloaded samples still has a
       semblence of validity about it. */
    SRC_ReloadSources();
  }

  SRC_RemoveDumpFiles();
  RTC_StartMeasurements();
  RCL_StartRefclocks();
  NSR_StartSources();
  NSR_AutoStartSources();

  /* Special modes can end only when sources update their reachability.
     Give up immediatelly if there are no active sources. */
  if (ref_mode != REF_ModeNormal && !SRC_ActiveSources()) {
    REF_SetUnsynchronised();
  }
}

/* ================================================== */

static void
post_init_ntp_hook(void *anything)
{
  if (ref_mode == REF_ModeInitStepSlew) {
    /* Remove the initstepslew sources and set normal mode */
    NSR_RemoveAllSources();
    ref_mode = REF_ModeNormal;
    REF_SetMode(ref_mode);
  }

  /* Close the pipe to the foreground process so it can exit */
  LOG_CloseParentFd();

  CNF_AddSources();
  CNF_AddBroadcasts();

  NSR_SetSourceResolvingEndHandler(ntp_source_resolving_end);
  NSR_ResolveSources();
}

/* ================================================== */

static void
reference_mode_end(int result)
{
  switch (ref_mode) {
    case REF_ModeNormal:
    case REF_ModeUpdateOnce:
    case REF_ModePrintOnce:
      exit_status = !result;
      SCH_QuitProgram();
      break;
    case REF_ModeInitStepSlew:
      /* Switch to the normal mode, the delay is used to prevent polling
         interval shorter than the burst interval if some configured servers
         were used also for initstepslew */
      SCH_AddTimeoutByDelay(2.0, post_init_ntp_hook, NULL);
      break;
    default:
      assert(0);
  }
}

/* ================================================== */

static void
post_init_rtc_hook(void *anything)
{
  if (CNF_GetInitSources() > 0) {
    CNF_AddInitSources();
    NSR_StartSources();
    assert(REF_GetMode() != REF_ModeNormal);
    /* Wait for mode end notification */
  } else {
    (post_init_ntp_hook)(NULL);
  }
}

/* ================================================== */

static void
check_pidfile(void)
{
  const char *pidfile = CNF_GetPidFile();
  FILE *in;
  int pid, count;
  
  in = fopen(pidfile, "r");
  if (!in)
    return;

  count = fscanf(in, "%d", &pid);
  fclose(in);
  
  if (count != 1)
    return;

  if (getsid(pid) < 0)
    return;

  LOG_FATAL("Another chronyd may already be running (pid=%d), check %s",
            pid, pidfile);
}

/* ================================================== */

static void
write_pidfile(void)
{
  const char *pidfile = CNF_GetPidFile();
  FILE *out;

  if (!pidfile[0])
    return;

  out = fopen(pidfile, "w");
  if (!out) {
    LOG_FATAL("Could not open %s : %s", pidfile, strerror(errno));
  } else {
    fprintf(out, "%d\n", (int)getpid());
    fclose(out);
  }
}

/* ================================================== */

static void
go_daemon(void)
{
  int pid, fd, pipefd[2];

  /* Create pipe which will the daemon use to notify the grandparent
     when it's initialised or send an error message */
  if (pipe(pipefd)) {
    LOG_FATAL("pipe() failed : %s", strerror(errno));
  }

  /* Does this preserve existing signal handlers? */
  pid = fork();

  if (pid < 0) {
    LOG_FATAL("fork() failed : %s", strerror(errno));
  } else if (pid > 0) {
    /* In the 'grandparent' */
    char message[1024];
    int r;

    close(pipefd[1]);
    r = read(pipefd[0], message, sizeof (message));
    if (r) {
      if (r > 0) {
        /* Print the error message from the child */
        message[sizeof (message) - 1] = '\0';
        fprintf(stderr, "%s\n", message);
      }
      exit(1);
    } else
      exit(0);
  } else {
    close(pipefd[0]);

    setsid();

    /* Do 2nd fork, as-per recommended practice for launching daemons. */
    pid = fork();

    if (pid < 0) {
      LOG_FATAL("fork() failed : %s", strerror(errno));
    } else if (pid > 0) {
      exit(0); /* In the 'parent' */
    } else {
      /* In the child we want to leave running as the daemon */

      /* Change current directory to / */
      if (chdir("/") < 0) {
        LOG_FATAL("chdir() failed : %s", strerror(errno));
      }

      /* Don't keep stdin/out/err from before. But don't close
         the parent pipe yet. */
      for (fd=0; fd<1024; fd++) {
        if (fd != pipefd[1])
          close(fd);
      }

      LOG_SetParentFd(pipefd[1]);
    }
  }
}

/* ================================================== */

static void
print_help(const char *progname)
{
      printf("Usage: %s [-4|-6] [-n|-d] [-q|-Q] [-r] [-R] [-s] [-t TIMEOUT] [-f FILE|COMMAND...]\n",
             progname);
}

/* ================================================== */

static void
print_version(void)
{
  printf("chronyd (chrony) version %s (%s)\n", CHRONY_VERSION, CHRONYD_FEATURES);
}

/* ================================================== */

static int
parse_int_arg(const char *arg)
{
  int i;

  if (sscanf(arg, "%d", &i) != 1)
    LOG_FATAL("Invalid argument %s", arg);
  return i;
}

using namespace qot;

NTP18::NTP18(boost::asio::io_service *io, // ASIO handle
	const std::string &iface,     // interface	
	struct uncertainty_params config // uncertainty calculation configuration		
	) : asio(io), baseiface(iface), sync_uncertainty(config), loc_sync_uncertainty(config), tl_clk_params(NULL), local_tl_clk_params(NULL), 
      nats_server("nats://nats.default.svc.cluster.local:4222")
{	
	global_clk_params = NULL;
  local_clk_params = NULL;
  this->Reset();
}

NTP18::~NTP18()
{
	this->Stop();
}

void NTP18::Reset()
{
	 status_flag = false;
}

void NTP18::Start(
	bool master,
	int log_sync_interval,
	uint32_t sync_session,
	int timelineid,
	int *timelinesfd,
  const std::string &tl_name,
  std::string &node_name,
	uint16_t timelines_size)
{
	// First stop any sync that is currently underway
	//this->Stop(); -> Need to figure out a way to restart without stopping
  timeline_uuid = tl_name;
  if (status_flag == false)
  {
  	// Start sync if it is not running
  	BOOST_LOG_TRIVIAL(info) << "Starting NTP synchronization";
  	kill = false;
    status_flag = true;

  	// Initialize Local Tracking Variable for Clock-Skew Statistics (Checks Staleness)
  	last_clocksync_data_point.offset  = 0;
  	last_clocksync_data_point.drift   = 0;
  	last_clocksync_data_point.data_id = 0;

  	// Initialize Global Variable for Clock-Skew Statistics 
  	ntp_clocksync_data_point[timelineid].offset  = 0;
  	ntp_clocksync_data_point[timelineid].drift   = 0;
  	ntp_clocksync_data_point[timelineid].data_id = 0;

  	// Spawn the sync and uncertainty threads
    sync_thread = boost::thread(boost::bind(&NTP18::SyncThread, this, timelineid, timelinesfd, timelines_size));
    uncertainty_thread = boost::thread(boost::bind(&NTP18::UncertaintyThread, this, timelineid, timelinesfd, timelines_size));
    loc_uncertainty_thread = boost::thread(boost::bind(&NTP18::LocalUncertaintyThread, this, timelineid, timelinesfd, timelines_size));
  }
  else
  {
    // Reprogram Sync -> Already running (TBD: FIgure out a way to re-program sync)
    BOOST_LOG_TRIVIAL(info) << "Updating NTP synchronization parameters";
  }

}

void NTP18::Stop()
{
	// If sync is not running return
  if (status_flag == false)
    return;

  BOOST_LOG_TRIVIAL(info) << "Stopping NTP synchronization ";
  SCH_QuitProgram();
	kill = true;

  // Wake the uncertainty thread from its condition variable
  pthread_mutex_lock(&uncertainty_lock);
  pthread_cond_signal(&uncertainty_condvar);
  pthread_mutex_unlock(&uncertainty_lock);

  // Wake the local timeline uncertainty thread from its condition variable
  pthread_mutex_lock(&loc_uncertainty_lock);
  pthread_cond_signal(&loc_uncertainty_condvar);
  pthread_mutex_unlock(&loc_uncertainty_lock);

  // Wait for the uncertainty and sync threads to join
  uncertainty_thread.join();
  loc_uncertainty_thread.join();
	sync_thread.join();

  #ifdef QOT_TIMELINE_SERVICE
  // Unmap the shared memory
  munmap(tl_clk_params, sizeof(tl_translation_t));
  #endif
}

int NTP18::ExtControl(void** pointer, ExtCtrlOptions type) 
{
  int timelineid = -1;
  int retval = 0;
  tl_translation_t* temp_clk_params = NULL;
  qot_server_t **server;
  qot_sync_msg_t **msg;
  qot_sdata_t sflag;
  std::map<int, qot_sdata_t>::iterator it;

  // Check if pointer or type is invalid
  if(!pointer || type < 0)
  {
    return -1;
  }

  // Chose functionality based on type
  switch (type)
  {
      case REQ_LOCAL_TL_CLOCK_MAIN: // pointer gives a "local" timeline id to get the local timeline main clock
          // Request Clock Memory
          timelineid = **((int**) pointer);
          local_tl_clk_params = comm.request_clk_memory(timelineid);

          if (local_tl_clk_params != NULL)
          {
            local_clk_params = local_tl_clk_params;
            BOOST_LOG_TRIVIAL(info) << "Got the Local Timeline Clock Memory Region";
          }
          else
          {
            BOOST_LOG_TRIVIAL(info) << "ERROR: Did not get the Local Timeline Clock Memory Region";
          }
          break;

      case REQ_LOCAL_TL_CLOCK_OV: // pointer gives a "local" timeline id to get the overlay local timeline main clock
          // Request Overlay Clock Memory
          timelineid = **((int**) pointer);
          temp_clk_params = comm.request_ov_clk_memory(timelineid);

          if (temp_clk_params != NULL)
          {
            // pointer = (void*)temp_clk_params;
            *pointer = (void*)temp_clk_params;
            BOOST_LOG_TRIVIAL(info) << "Got the Overlay Local Timeline Clock Memory Region";
          }
          break;

      case SET_PUBSUB_SERVER: // pointer points to a const char* with the nats server
          // Set the NATS server to be used
          nats_server = std::string(*((const char**)pointer));
          BOOST_LOG_TRIVIAL(info) << "Got the NATS server URL " << nats_server;
          break;

      case MODIFY_SYNC_PARAMS: // pointer points to a char* with the command
          // Externally Modify the NTP sync using the chronyc client library
          retval = client_call(*((char**)pointer));
          break;

      case GET_TIMELINE_SERVER: // pointer points to a qot_server_t* with the command
          // Get the timeline NTP server
          server = ((qot_server_t**) pointer);
          retval = comm.get_timeline_server((*server)->timeline_id, **server);
          pointer = (void**)server; 
          break;

      case SET_TIMELINE_SERVER: // pointer points to a qot_server_t* with the command
          // Set the timeline NTP server
          server = ((qot_server_t**) pointer);
          retval = comm.set_timeline_server((*server)->timeline_id, **server);
          break;

      case ADD_TL_SYNC_DATA: // pointer to qot_sync_msg_t
          // Add the timeline to the QoT map
          msg = ((qot_sync_msg_t**) pointer);
          qotmap_lock.lock();
          sflag.accuracy = (*msg)->demand.accuracy.above.sec*1000000000 + (*msg)->demand.accuracy.above.asec/1000000000; /* Required QoT */
          sflag.flag = 0;             /* Flag indicating server is set */
          sflag.timeline_uuid = std::string((*msg)->info.name);  /* Timeline UUID */
          sflag.good_data_counter = 0;
          timeline_qotmap[((*msg)->info.index)] = sflag;
          qotmap_lock.unlock();
          BOOST_LOG_TRIVIAL(info) << "NTP18: Added Timeline " << std::string((*msg)->info.name) << " with Acuracy " << timeline_qotmap[((*msg)->info.index)].accuracy << " ns to Map";
          break;

      case DEL_TL_SYNC_DATA: // pointer to qot_sync_msg_t
          // Remove the timeline from the QoT map
          msg = ((qot_sync_msg_t**) pointer);
          qotmap_lock.lock();
          it = timeline_qotmap.find(((*msg)->info.index));
          if (it != timeline_qotmap.end())
              timeline_qotmap.erase(it); 
          else
              std::cout << "NTP18: Error Timeline not found in QoT map\n";
          qotmap_lock.unlock();
          BOOST_LOG_TRIVIAL(info) << "NTP18: Removed Timeline " << std::string((*msg)->info.name) << " from Map";
          break;

      case SET_INIT_SYNC_CFG: // pointer points to a char* with the filename
          // Externally Modify the NTP initial configuration 
          conf_file = std::string(*((char**)pointer));
          break;

      default: // code to be executed if type doesn't match any cases
          return ENOTSUP;
  }
  return retval;
}


int NTP18::SyncThread(int timelineid, int *timelinesfd, uint16_t timelines_size)
{
	  BOOST_LOG_TRIVIAL(info) << "Sync thread started for timeline " << timelineid;

    #ifdef QOT_TIMELINE_SERVICE
    // Map the timeline clock into the memory space
    tl_clk_params = comm.request_clk_memory(timelineid);

    if (tl_clk_params != NULL)
      global_clk_params = tl_clk_params;
    else // Unable to map clock
      return -1;
    #endif

	  // const char *conf_file = DEFAULT_CONF_FILE;
    const char *progname = "ntp";
    char *user = NULL;
    const char *log_file = "log.txt";
    struct passwd *pw;
    int opt, debug = 1, nofork = 1, address_family = IPADDR_INET4;//IPADDR_UNSPEC;
    int do_init_rtc = 0, restarted = 0, client_only = 0, timeout = 0;
    int scfilter_level = 0, lock_memory = 0, sched_priority = 0;
    int clock_control = 1, system_log = 0;
    int config_args = 0;

  	do_platform_checks();

  	LOG_Initialise();

  	optind = 1;

    if (getuid() && !client_only)
      LOG_FATAL("Not superuser");

    if (log_file) {
      LOG_OpenFileLog(log_file);
    } else if (system_log) {
      LOG_OpenSystemLog();
    }
  
    LOG_SetDebugLevel(debug);
  
    LOG(LOGS_INFO, "chronyd version %s starting (%s)", CHRONY_VERSION, CHRONYD_FEATURES);

    DNS_SetAddressFamily(address_family);

    CNF_Initialise(restarted, client_only);

    CNF_ReadFile(conf_file.c_str());

    /* Check whether another chronyd may already be running */
    check_pidfile();

    /* Write our pidfile to prevent other chronyds running */
    write_pidfile();

    PRV_Initialise();
    /* Next line added to initialize the timeline global clock 
       Replaces LCL_Initialise() */
    #ifdef NTP_QOT_STACK
    LCL_Initialise_GlobalTimeline(timelineid, timelinesfd);
    #else
    LCL_Initialise();
    #endif
    SCH_Initialise();
    SYS_Initialise(clock_control);
    RTC_Initialise(do_init_rtc);
    SRC_Initialise();
    RCL_Initialise();
    KEY_Initialise();

    /* Open privileged ports before dropping root */
    CAM_Initialise(address_family);
    NIO_Initialise(address_family);
    NCR_Initialise();
    CNF_SetupAccessRestrictions();

    /* Command-line switch must have priority */
    if (!sched_priority) {
      sched_priority = CNF_GetSchedPriority();
    }
    if (sched_priority) {
      SYS_SetScheduler(sched_priority);
    }

    if (lock_memory || CNF_GetLockMemory()) {
      SYS_LockMemory();
    }

    if (!user) {
      user = CNF_GetUser();
    }

    if ((pw = getpwnam(user)) == NULL)
      LOG_FATAL("Could not get %s uid/gid", user);

    /* Create all directories before dropping root */
    CNF_CreateDirs(pw->pw_uid, pw->pw_gid);

    /* Drop root privileges if the specified user has a non-zero UID */
    if (!geteuid() && (pw->pw_uid || pw->pw_gid))
      SYS_DropRoot(pw->pw_uid, pw->pw_gid);

    REF_Initialise();
    SST_Initialise();
    NSR_Initialise();
    NSD_Initialise();
    CLG_Initialise();
    MNL_Initialise();
    TMC_Initialise();
    SMT_Initialise();

    /* From now on, it is safe to do finalisation on exit */
    initialised = 1;

    UTI_SetQuitSignalsHandler(signal_cleanup);

    CAM_OpenUnixSocket();

    if (scfilter_level)
      SYS_EnableSystemCallFilter(scfilter_level);

    if (ref_mode == REF_ModeNormal && CNF_GetInitSources() > 0) {
      ref_mode = REF_ModeInitStepSlew;
    }

    REF_SetModeEndHandler(reference_mode_end);
    REF_SetMode(ref_mode);

    if (timeout > 0)
      SCH_AddTimeoutByDelay(timeout, quit_timeout, NULL);

    if (do_init_rtc) {
      RTC_TimeInit(post_init_rtc_hook, NULL);
    } else {
      post_init_rtc_hook(NULL);
    }

    // Initialize the chronyc client with default settings
    init_client(NULL, -1);

    /* The program normally runs under control of the main loop in
       the scheduler. */
    SCH_MainLoop();

    LOG(LOGS_INFO, "chronyd exiting");

    // Exit the chronyc client
    exit_client();

    MAI_CleanupAndExit();

    BOOST_LOG_TRIVIAL(info) << "Sync thread stopping for timeline " << timelineid;

    return 0;
	
}

int NTP18::UncertaintyThread(int timelineid, int *timelinesfd, uint16_t timelines_size)
{
    // struct timespec wait_time;
    int64_t timedwait_period_second = 2;
    while (tl_clk_params == NULL)
    {
      sleep(1);
      if (kill || need_to_exit_prog)
      {
          return 0;
      }
    }
 
    BOOST_LOG_TRIVIAL(info) << "Sync Uncertainty thread started for timeline " << timelineid;

    int i = 0;
    int retval = 0;

    #ifdef QOT_TIMELINE_SERVICE
    #ifdef NATS_SERVICE
    // Connect to NATS Service
    sync_uncertainty.natsConnect(nats_server.c_str());
    #endif
    #endif

    /* Flag to enable direct QoT estimation for NTP using its peer dispersion calculations*/
    #ifndef QOT_PEER_DISP
    while (!need_to_exit_prog && !kill)
    {
      // Get the current time for timed wait (every second)
      // clock_gettime(CLOCK_REALTIME, &wait_time);
      // wait_time.tv_sec += 2;
      pthread_mutex_lock(&uncertainty_lock);

      // Check if a new data point has been added
      //pthread_cond_timedwait(&uncertainty_condvar, &uncertainty_lock, &wait_time);
      pthread_cond_wait(&uncertainty_condvar, &uncertainty_lock);

      if (kill || need_to_exit_prog)
      {
          pthread_mutex_unlock(&uncertainty_lock);
          break;
      }

      std::cout << "New uncertainty value found " << i++ <<  "\n";

      // Check if a new skew statistic data point has been added -> New statistic received -> Replace old value
      if(last_clocksync_data_point.data_id < ntp_clocksync_data_point[timelineid].data_id)
      {
         last_clocksync_data_point = ntp_clocksync_data_point[timelineid];
      }
      else
      {
         pthread_mutex_unlock(&uncertainty_lock);
         continue;
      }
      // else // Now the offset is uncorrected -> Because we are extrapolating from a previous point
      // {
      //    last_clocksync_data_point.drift = ntp_clocksync_data_point[timelineid].drift;
      //    // The uncompensated new offset is the drift*ns_time_passed/1Billion as drift is ppb, which is equivalent to drift*second_time_passed
      //    last_clocksync_data_point.offset = last_clocksync_data_point.offset + ntp_clocksync_data_point[timelineid].drift*timedwait_period_second;
      //    std::cout << "Drift = " << last_clocksync_data_point.drift << ", Offset = " << last_clocksync_data_point.offset << "\n";
      // }

      // Add Synchronization Uncertainty Sample
      #ifdef QOT_TIMELINE_SERVICE
      sync_uncertainty.CalculateBounds(last_clocksync_data_point.offset, ((double)last_clocksync_data_point.drift)/1000000000LL, -1, tl_clk_params, timeline_uuid);
      #else
      sync_uncertainty.CalculateBounds(last_clocksync_data_point.offset, ((double)last_clocksync_data_point.drift)/1000000000LL, timelinesfd[0], NULL, timeline_uuid);
      #endif

      pthread_mutex_unlock(&uncertainty_lock);
    }
    #else
      // Peer Est-based monitoring code -> to decide if servers should be changed/modified
      std::map<int, qot_sdata_t>::iterator it;
      int64_t accuracy, max_accuracy;
      char source_ip[100];
      qot_server_t server;
      IPAddr server_ip_addr;
      struct timespec now;
      int reselect_flag = 0;
      char input_data[100];

      while (!need_to_exit_prog && !kill)
      {
        sleep(QOT_STATUS_POLL);
        qotmap_lock.lock();
        clock_gettime(CLOCK_REALTIME, &now);
        for (it = timeline_qotmap.begin(); it != timeline_qotmap.end(); it++)
        {
            max_accuracy = global_clk_params->u_nsec; // This is the last set root dispersion
            accuracy = global_clk_params->u_nsec;
            accuracy = int64_t(REF_GetRootDispersion(&now)*1000000000);
            // Check if QoT is being met
            if (accuracy <= it->second.accuracy)
            {
                // We found one good sample -> Yayy !
                if (it->second.good_data_counter < 0)
                    it->second.good_data_counter = 0;

                std::cout << "NTP18: Timeline " << it->first << " QoT Accuracy " << it->second.accuracy << " being met\n";
                
                it->second.good_data_counter++;
                // Server not set & check if server is reliably good
                if (it->second.flag == 0 && it->second.good_data_counter >= QOT_SERVER_GOOD_ITERATIONS)
                {
                  // Get the best server
                  retval = SRC_GetBestSource(source_ip, &server.stratum ,100);

                  if (retval >=0)
                  {
                      // Set the timeline NTP server    
                      server.hostname = std::string(source_ip);
                      std::cout << "NTP18: Setting server " << server.hostname << " with stratum " << server.stratum << " for timeline " << it->first << "\n";
                      server.type = "global";
                      retval = comm.set_timeline_server(it->first, server);
                      if (retval == 0)
                      {
                        it->second.flag = 1;
                        it->second.server_ip = server.hostname;
                      }
                  }
                }
            }
            else // QoT is not being met
            {
                std::cout << "NTP18: Timeline " << it->first << " QoT Accuracy " << it->second.accuracy << " VIOLATION\n";
                
                // Decrement the good data counter
                it->second.good_data_counter--;

                /* Server has been set, try to better the poll if the root dispersion is usable */
                if (it->second.flag == 1 && it->second.accuracy > max_accuracy)
                {
                  retval = SRC_GetBestSourceIPAddr(&server_ip_addr);
                  if (retval >=0)
                  {
                      std::cout << "Adjusting Poll due to QoT Violation\n";
                      // Adjust the poll of the server
                      NSR_AdjustPoll(&server_ip_addr, accuracy, it->second.accuracy);
                      
                      // Try to fire up burst as a last-ditch attempt
                      sprintf(input_data, "burst 5/10");
                      retval = client_call(input_data);
                  }
                  else
                  {
                      // Source became invalid try to get a new source -> from the NTP Pool
                      std::cout << "Best Source became invalid, adding an NTP pool server\n";
                      sprintf(input_data, "add server 0.pool.ntp.org maxpoll 5");
                      retval = client_call(input_data);
                      // SRC_ReselectSource();

                  }
                }
                else if (it->second.flag == 0) // Server is not set
                {
                    // Try to see if a server has been set on the coordination service
                    retval = comm.get_timeline_server(it->first, server);
                    if (retval != 0)
                    {
                      // Wait for some iterations
                      if (it->second.good_data_counter < -QOT_SERVER_GOOD_ITERATIONS)
                      {
                        std::cout << "Best Source still not useful, adding an NTP pool server\n";
                        // The server needs to be changed -> Get a new one from the NTP pool servers
                        sprintf(input_data, "add server 0.pool.ntp.org maxpoll 5");
                        retval = client_call(input_data);
                        it->second.good_data_counter = 0;
                        std::cout << "Succesfully added an NTP pool server\n";
                        // Try to fire up burst as a last-ditch attempt
                        sprintf(input_data, "burst 5/10");
                        retval = client_call(input_data);
                        // SRC_ReselectSource();
                      }
                    }
                    else
                    {
                      std::cout << "NTP18: Got server " << server.hostname << " with stratum " << server.stratum << " for timeline " << it->first << "\n";
                      // Server Found -> Add it to the mix
                      sprintf(input_data, "add server %s maxpoll 5", server.hostname); 
                      retval = client_call(input_data);
                      // Try to fire up burst as a last-ditch attempt
                      sprintf(input_data, "burst 5/10");
                      retval = client_call(input_data);
                      it->second.good_data_counter = 0;
                      it->second.flag = 1; // Set the server flag
                    }
                }
                else // Server is set, may not be usable (best dispersion less than requirement)
                {
                    // We may need to try to change the server -> not reliable or not usable
                    if (it->second.good_data_counter < -QOT_SERVER_GOOD_ITERATIONS)
                    {            
                      std::cout << "Best Source not good enough, adding an NTP pool server\n";
                      char* input_data = "add server 0.pool.ntp.org maxpoll 5";
                      retval = client_call(input_data);
                      it->second.good_data_counter = 0;
                      // Try to fire up burst as a last-ditch attempt
                      sprintf(input_data, "burst 5/10");
                      retval = client_call(input_data);
                      // SRC_ReselectSource();
                    }
                }
            }          
        }
        qotmap_lock.unlock();
      }
    #endif
    BOOST_LOG_TRIVIAL(info) << "Sync Uncertainty thread stopping for timeline " << timelineid;

    return 0;
}

int NTP18::LocalUncertaintyThread(int timelineid, int *timelinesfd, uint16_t timelines_size)
{
    // struct timespec wait_time;
    int64_t timedwait_period_second = 2;

    fake_local_timelineid = 1;

    // Initialize Local Tracking Variable for Clock-Skew Statistics (Checks Staleness)
    last_clkrtphc_data_point.offset  = 0;
    last_clkrtphc_data_point.drift   = 0;
    last_clkrtphc_data_point.data_id = 0;

    // Initialize Global Variable for Clock-Skew Statistics 
    ntp_clocksync_data_point[fake_local_timelineid].offset  = 0;
    ntp_clocksync_data_point[fake_local_timelineid].drift   = 0;
    ntp_clocksync_data_point[fake_local_timelineid].data_id = 0;

    while (local_tl_clk_params == NULL)
    {
      sleep(1);
      if (kill || need_to_exit_prog)
      {
          return 0;
      }
    }
 
    BOOST_LOG_TRIVIAL(info) << "Local Timeline (CLK_RT->PHC) Sync Uncertainty thread started";

    int i = 0;

    #ifdef QOT_TIMELINE_SERVICE
    #ifdef NATS_SERVICE
    // Connect to NATS Service
    loc_sync_uncertainty.natsConnect(nats_server.c_str());
    #endif
    #endif

    while (!need_to_exit_prog && !kill)
    {
      // Get the current time for timed wait (every second)
      // clock_gettime(CLOCK_REALTIME, &wait_time);
      // wait_time.tv_sec += 2;
      pthread_mutex_lock(&loc_uncertainty_lock);

      // Check if a new data point has been added
      //pthread_cond_timedwait(&uncertainty_condvar, &uncertainty_lock, &wait_time);
      pthread_cond_wait(&loc_uncertainty_condvar, &loc_uncertainty_lock);

      if (kill || need_to_exit_prog)
      {
          pthread_mutex_unlock(&loc_uncertainty_lock);
          break;
      }

      std::cout << "New local timeline uncertainty value for CLKRT->PHC found " << i++ <<  "\n";

      // Check if a new skew statistic data point has been added -> New statistic received -> Replace old value
      if(last_clkrtphc_data_point.data_id < ntp_clocksync_data_point[fake_local_timelineid].data_id)
      {
         last_clkrtphc_data_point = ntp_clocksync_data_point[fake_local_timelineid];
      }
      else
      {
         pthread_mutex_unlock(&loc_uncertainty_lock);
         continue;
      }

      // Add Synchronization Uncertainty Sample
      #ifdef QOT_TIMELINE_SERVICE
      loc_sync_uncertainty.CalculateBounds(last_clkrtphc_data_point.offset, ((double)last_clkrtphc_data_point.drift)/1000000000LL, -1, local_tl_clk_params, timeline_uuid);
      #else
      // Needs to be fixed, timelinefd[0] can cause incorrect results
      loc_sync_uncertainty.CalculateBounds(last_clkrtphc_data_point.offset, ((double)last_clkrtphc_data_point.drift)/1000000000LL, timelinesfd[0], NULL, timeline_uuid);
      #endif

      pthread_mutex_unlock(&loc_uncertainty_lock);
    }
    BOOST_LOG_TRIVIAL(info) << "Local Timeline Sync Uncertainty thread stopping";

    return 0;
}
