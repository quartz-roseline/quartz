/**
 * @file PTP18.cpp
 * @brief Provides ptp instance to the sync interface (based on linuxptp-1.8)
 * @author Anon D'Anon
 * 
 * Copyright (c) Regents of the Anon, 2018. All rights reserved.
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
 * The code in this file is an adaptation of ptp4l  
 *
 */

#include <fstream>
#include <map>

#include "PTP18.hpp"

#include "local_timeline.h"

#include "../../qot_sync_service.hpp"

using namespace qot;

#define DEBUG true
#define TEST  true
#define LOGGING_FLAG true
#define DECISION_MAKING_PERIOD 10

// Global Log File
std::ofstream ptp_logfile;

// Global Log file for rate change
std::ofstream rate_logfile;

// Data structure to hold the desired and delivered QoT
typedef struct accuracy_vector {
	uint64_t delivered_accuracy;      /* Delivered Accuracy   */
    uint64_t desired_accuracy;        /* Desired Accuracy  */
} accuracy_vector_t;

// Data Structure maintaining the per-timeline configurations (timeline uuid -> reciever map)
std::map<std::string, struct config*> config_map;

// Data structure maintaining timeline and node to QoT mappings
std::map<std::string, std::map<std::string, accuracy_vector_t>> timeline_qot_data;

int assume_two_step = 0;

PTP18::PTP18(boost::asio::io_service *io, // ASIO handle
		const std::string &iface,		  // interface
		struct uncertainty_params config  // uncertainty calculation configuration
	) : baseiface(iface), sync_uncertainty(config), cfg(NULL), tl_clk_params(NULL), qot_subscriber_flag(false),
      nats_server("nats://nats.default.svc.cluster.local:4222"), desired_accuracy(0)
{	
	this->Reset();	

	if (LOGGING_FLAG == 1)
    {
      std::string logfile_name = "/opt/qot-stack/doc/data/ptplog.csv";
      std::string ratefile_name = "/opt/qot-stack/doc/data/ptplog_rate.csv";
      ptp_logfile.open(logfile_name);
      rate_logfile.open(ratefile_name);
    }
}

PTP18::~PTP18()
{
	this->Stop();

	if (LOGGING_FLAG == 1)
	{
      ptp_logfile.close();
      rate_logfile.close();
	}

    qot_subscriber_flag = false;
}

void PTP18::Reset() 
{
	if (cfg)
		config_destroy(cfg);

	// Initialize Default Config
	cfg = config_create();

	// Copy the pointer to the map
	config_map[timeline_uuid] = cfg;

	// Reset QoT Subscriber flag
	qot_subscriber_flag = false;
}

void PTP18::Start(bool master, int log_sync_interval, uint32_t sync_session,
	int timelineid, int *timelinesfd, const std::string &tl_name, std::string &node_name, uint16_t timelines_size)
{
	int mod_log_sync_interval;
	// First stop any sync that is currently underway
	this->Stop();

	// Record timeline name
	timeline_uuid = tl_name;

	// Set the node name
	node_uuid = node_name;

	// Initialize Default Config
	cfg = config_create();

	// Copy the pointer to the map
	config_map[timeline_uuid] = cfg;

	// Reset QoT Subscriber flag
	qot_subscriber_flag = false;

	// If requested interval is less than 1 sec (log value < 0) then truncate to 0
	mod_log_sync_interval = log_sync_interval;
	if (log_sync_interval < 0)
		mod_log_sync_interval = 0;

	// Restart sync
	BOOST_LOG_TRIVIAL(info) << "Starting PTP synchronization as " << (master ? "master" : "slave") 
		<< " on domain " << sync_session << " with synchronization interval " << pow(mod_log_sync_interval,2) << " seconds";
	
	config_set_int(cfg, "logSyncInterval", mod_log_sync_interval);
	config_set_int(cfg, "domainNumber", sync_session); // Should (can also for multi-timeline?) be set to sync session

	if (master){
		config_set_int(cfg, "slaveOnly", 0);
	}

	// Force to be a slave -> prev lines commented by Anon (comment prev lines out to force slave mode)
	// if (config_set_int(cfg, "slaveOnly", 1)) {
	// 			goto out;

	kill = false;

	// Initialize Local Tracking Variable for Clock-Skew Statistics (Checks Staleness)
	last_clocksync_data_point.offset  = 0;
	last_clocksync_data_point.drift   = 0;
	last_clocksync_data_point.data_id = 0;

	// Initialize Global Variable for Clock-Skew Statistics 
	ptp_clocksync_data_point[timelineid].offset  = 0;
	ptp_clocksync_data_point[timelineid].drift   = 0;
	ptp_clocksync_data_point[timelineid].data_id = 0;

	thread = boost::thread(boost::bind(&PTP18::SyncThread, this, timelineid, timelinesfd, timelines_size));
}

void PTP18::Stop()
{
	BOOST_LOG_TRIVIAL(info) << "Stopping PTP synchronization ";
	kill = true;
	thread.join();
}

// Set the desired accuracy of the timeline
void PTP18::SetDesiredAccuracy(uint64_t accuracy)
{
	desired_accuracy = accuracy;
	sync_uncertainty.SetNodeAccuracy(desired_accuracy);
	return;
}

// Set the pub-sub server
void PTP18::SetPubSubServer(std::string server)
{
	nats_server = server;
}

int PTP18::ExtControl(void** pointer, ExtCtrlOptions type) 
{
  int timelineid = -1;
  int retval = 0;
  uint64_t accuracy;
  qot_sync_msg_t **msg;

  // Check if pointer or type is invalid
  if(!pointer || type < 0)
  {
    return -1;
  }

  // Chose functionality based on type
  switch (type)
  {
      case SET_PUBSUB_SERVER: // pointer points to a const char* with the nats server
          // Set the NATS server to be used
          SetPubSubServer(std::string(*((const char**)pointer)));
          BOOST_LOG_TRIVIAL(info) << "PTP18: Got the NATS server URL " << nats_server;
          break;

      case ADD_TL_SYNC_DATA: // pointer to qot_sync_msg_t
          // Add the timeline desired QoT
          msg = ((qot_sync_msg_t**) pointer);
          accuracy = (*msg)->demand.accuracy.above.sec*1000000000 + (*msg)->demand.accuracy.above.asec/1000000000; /* Required QoT */
          SetDesiredAccuracy(accuracy);
          BOOST_LOG_TRIVIAL(info) << "PTP18: Got the Desired Accuracy " << accuracy;
          break;

      default: // code to be executed if type doesn't match any cases
          return ENOTSUP;
  }
  return retval;
}

#ifdef QOT_TIMELINE_SERVICE
#ifdef NATS_SERVICE
// Callback function called by the sync uncertainty calculator if this node is the sync master to set the rate
void ptp_sync_tuner(tl_translation_t params, std::string timeline_uuid, std::string node_name, uint64_t desired_accuracy)
{
	// Create the accuracy vector
	accuracy_vector_t accuracy; 

	// Key Variables
	accuracy.delivered_accuracy = (uint64_t) params.u_nsec;
	accuracy.desired_accuracy = desired_accuracy;

	// Store data in the global map
	timeline_qot_data[timeline_uuid][node_name] = accuracy;	
	return;
}

int PTP18::ChangeSyncRate()
{
	// Get the config pointer
	struct config *cfg = config_map[timeline_uuid];
	int current_sync_interval = config_get_int(cfg, NULL, "logSyncInterval");
	int mod_log_sync_interval = current_sync_interval;
	uint64_t desired_accuracy, delivered_accuracy;
	int change_sync_flag = 0; // 1-> indicates inrease rate by 2
	int total_nodes_on_timeline = 0;
	double exactness_factor = 0; // Captures the node with the least delivered/desired ratio

	// Get the data for the timeline from the global variable
	for (std::map<std::string, accuracy_vector_t>::iterator it = timeline_qot_data[timeline_uuid].begin();
		it != timeline_qot_data[timeline_uuid].end(); ++it)
	{
		desired_accuracy = it->second.desired_accuracy;
		delivered_accuracy = it->second.delivered_accuracy;
		total_nodes_on_timeline++;

		if (DEBUG)
		{
			std::cout << "PTP18: Node " << it->first << " on timeline " << timeline_uuid
		    	      << " delivered acc = " << delivered_accuracy 
		        	  << ", desired acc = " << desired_accuracy << std::endl;
		}

		// Accuracy has not been set or computed yet
		if (desired_accuracy == 0 || delivered_accuracy == 0)
			continue;

		// Accuracy not being delivered
		if (desired_accuracy < delivered_accuracy)
			change_sync_flag++;

		// The exactness factor indicates how far the desired accuracy is from the delivered accuracy
		// -> 1 indicates exact, fractional indicates rate should decrease, >1 indicates rate should increase
		if ((double)delivered_accuracy/(double)desired_accuracy > exactness_factor)
			exactness_factor = (double)delivered_accuracy/(double)desired_accuracy;
	}
	
	// Control the accuracy
	if (change_sync_flag > 0)
	{
	 	// Inccrease sync rate by factor of 2
	 	mod_log_sync_interval = current_sync_interval - 1;
	 	// Clamp the increase to 8 packets per second
	 	if (mod_log_sync_interval > -4)
	 	{
	 		config_set_int(cfg, "logSyncInterval", mod_log_sync_interval);
	 		std::cout << "PTP18: Increasing log sync rate to " << mod_log_sync_interval << std::endl;
	 	}
	 	else
	 		std::cout << "PTP18: Cannot increase sync rate anymore" << std::endl;
	}
	else if (exactness_factor < 0.75 && exactness_factor > 0) // best desired error is 2x the delivered error
	{
		// We can reduce the sync rate
		mod_log_sync_interval = current_sync_interval + 1;
	 	// Clamp the increase to 1 packets every 4 seconds
	 	if (mod_log_sync_interval <= 2)
	 	{
	 		config_set_int(cfg, "logSyncInterval", mod_log_sync_interval);
	 		std::cout << "PTP18: Decreasing log sync rate to " << mod_log_sync_interval << std::endl;
	 	}
	 	else
	 		std::cout << "PTP18: Cannot decrease sync rate anymore" << std::endl;
	}
	else
	{
		std::cout << "PTP18: Sync rate unchanged" << std::endl;
	}

	// Log the rate or no change
	if (LOGGING_FLAG == 1)
	{
		struct timespec now;
		clock_gettime(CLOCK_REALTIME, &now);
		rate_logfile << now.tv_sec << "," << mod_log_sync_interval  << "\n";
		rate_logfile.flush();
	}
	return 0;
}
#endif
#endif



int PTP18::SyncThread(int timelineid, int *timelinesfd, uint16_t timelines_size)
{
	BOOST_LOG_TRIVIAL(info) << "PTP (linuxptp-1.8) Sync thread started ";
	char *config = NULL, *req_phc = NULL;
	int c, err = -1, print_level;
	struct clock *clock = NULL;
	int required_modes = 0;
	int counter = 0;
	// int count = 0;
	// int interval =0;

	#ifdef QOT_TIMELINE_SERVICE
    // Map the timeline clock into the memory space
    tl_clk_params = comm.request_ov_clk_memory(timelineid);

    if (tl_clk_params == NULL)
      return -1;
    #endif

  	#ifdef QOT_TIMELINE_SERVICE
    #ifdef NATS_SERVICE
    // Connect to NATS Service
    sync_uncertainty.natsConnect(nats_server.c_str());

    // Enable the uncertainty parameters to be published to the master sync
    std::string topic = "qot.timeline.";
    topic.append(timeline_uuid);
    topic.append(std::string(".syncmaster"));
    sync_uncertainty.SetNodeUUID(node_uuid);
	sync_uncertainty.StartMasterSyncPublish(topic);
    #endif
    #endif

	if (config_set_int(cfg, "delay_mechanism", DM_AUTO))
	{
		config_destroy(cfg);
		return err;
	}

	enum transport_type trans_type = TRANS_IEEE_802_3; // Can also be TRANS_UDP_IPV4 or TRANS_UDP_IPV6
	if (config_set_int(cfg, "network_transport", trans_type))
	{
		config_destroy(cfg);
		return err;
	}

	enum timestamp_type ts_type = TS_HARDWARE; // Can also be TS_SOFTWARE, TS_LEGACY_HW
	if (config_set_int(cfg, "time_stamping", ts_type))
	{
		config_destroy(cfg);
		return err;
	}

	// Add the interface
	char ifname[MAX_IFNAME_SIZE];
	strncpy(ifname, baseiface.c_str(), MAX_IFNAME_SIZE);

	if (!config_create_interface(ifname, cfg))
	{
		config_destroy(cfg);
		return err;
	}

	// Verbose printing for debug
	if (DEBUG)
		config_set_int(cfg, "verbose", 1);

	// Read config from a file -> can be removed later
	if (config && (c = config_read(config, cfg))) {
		return c;
	}

	print_set_verbose(config_get_int(cfg, NULL, "verbose"));
	print_set_syslog(config_get_int(cfg, NULL, "use_syslog"));
	print_set_level(config_get_int(cfg, NULL, "logging_level"));

	assume_two_step = config_get_int(cfg, NULL, "assume_two_step");
	sk_check_fupsync = config_get_int(cfg, NULL, "check_fup_sync");
	sk_tx_timeout = config_get_int(cfg, NULL, "tx_timestamp_timeout");

	if (config_get_int(cfg, NULL, "clock_servo") == CLOCK_SERVO_NTPSHM) {
		config_set_int(cfg, "kernel_leap", 0);
		config_set_int(cfg, "sanity_freq_limit", 0);
	}

	if (STAILQ_EMPTY(&cfg->interfaces)) {
		fprintf(stderr, "no interface specified\n");
		config_destroy(cfg);
		return err;
	}

	#ifdef QOT_TIMELINE_SERVICE

	clock = clock_create(cfg->n_interfaces > 1 ? CLOCK_TYPE_BOUNDARY :
			     CLOCK_TYPE_ORDINARY, cfg, req_phc, timelineid, timelinesfd, tl_clk_params);
	if (!clock) {
		fprintf(stderr, "failed to create a clock\n");
		goto out;
	}
	#else
	clock = clock_create(cfg->n_interfaces > 1 ? CLOCK_TYPE_BOUNDARY :
			     CLOCK_TYPE_ORDINARY, cfg, req_phc, timelineid, timelinesfd, NULL);
	if (!clock) {
		fprintf(stderr, "failed to create a clock\n");
		goto out;
	}
	#endif

	err = 0;

	while (is_running() && !kill) {
		if (clock_poll(clock))
			break;

		// Check if a new skew statistic data point has been added
		if(last_clocksync_data_point.data_id < ptp_clocksync_data_point[timelineid].data_id)
		{
			// New statistic received -> Replace old value
			last_clocksync_data_point = ptp_clocksync_data_point[timelineid];

			// Add Synchronization Uncertainty Sample
			#ifdef QOT_TIMELINE_SERVICE
			sync_uncertainty.CalculateBounds(last_clocksync_data_point.offset, ((double)last_clocksync_data_point.drift)/1000000000LL, -1, tl_clk_params, timeline_uuid);
			#else
			sync_uncertainty.CalculateBounds(last_clocksync_data_point.offset, ((double)last_clocksync_data_point.drift)/1000000000LL, timelinesfd[0], NULL, timeline_uuid);
			#endif
			if (LOGGING_FLAG == 1)
			{
				ptp_logfile << tl_clk_params->last << "," << tl_clk_params->mult << "," << tl_clk_params->nsec << "," << tl_clk_params->u_nsec << "," << tl_clk_params->u_mult  << "\n";
				ptp_logfile.flush();
			}

		}
		#ifdef NATS_SERVICE
		// Check if this node is the master -> spawn a subscriber to listen to qot of other nodes
		if (timeline_master_flag[timelineid] == 1 && !qot_subscriber_flag)
		{
			qot_subscriber_flag = true;
			// I am the master, I don't need to publish to myself
			sync_uncertainty.StopMasterSyncPublish();	

			// Start listening for messages from other nodes on the timeline
			sync_uncertainty.natsSubscribe(topic, ptp_sync_tuner);
		}
		else if (timeline_master_flag[timelineid] == 0 && qot_subscriber_flag)
		{
			qot_subscriber_flag = false;
			// I am no longer the master, stop listening for messages from other nodes on the timeline
			sync_uncertainty.natsUnSubscribe();
			// I am no longer the master, start publishing
			sync_uncertainty.StartMasterSyncPublish(topic);
		}

		// Decide to adapt the Sync Rate
		counter++;
		if (counter % DECISION_MAKING_PERIOD == 0 && qot_subscriber_flag)
		{
			ChangeSyncRate();
		}
		#endif

		// count = count + 1;
		// if (count % 50 == 0)
		// {
		// 	interval = interval + 1;
		// 	BOOST_LOG_TRIVIAL(info) << "Changing PTP synchronization to " << (interval) % 3;
		// 	config_set_int(cfg, "logSyncInterval", (interval) % 3);
		// }

	}
out:
	if (clock)
		clock_destroy(clock);
	config_destroy(cfg);
	return err;
}
