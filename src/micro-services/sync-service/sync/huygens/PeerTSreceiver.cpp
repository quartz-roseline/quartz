/**
 * @file PeerTSreceiver.cpp
 * @brief Peer to Peer receiver to get offsets from the compute server
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
 */
#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <iomanip>

#include "PeerTSreceiver.hpp"

// Include PTP files
extern "C" 
{
  #include "../ptp/linuxptp-1.8/clockadj.h"
  #include "../ptp/linuxptp-1.8/phc.h"
  #include "../ptp/linuxptp-1.8/config.h"
}

// Add header to Modern JSON C++ Library
#include "../../../../../thirdparty/json-modern-cpp/json.hpp"

using namespace qot;

#define DEBUG_FLAG 0
#define LOGGING_FLAG 1

// Global Variable for node name
std::string global_node_name;

// Global Log File
std::ofstream logfile;

#ifdef PEER_SERVICE
// Variable defined to make linking against ptp library compatible
int assume_two_step = 0;
#endif

// Global variable for the clock id being disciplined
clockid_t global_clkid;

// Global counter indicating how much data came in mod 10
int set_counter = 0;

// Global variable indicating if the clock should be disciplined
bool global_disc_flag = false;

#ifdef NATS_SERVICE

/* NATS Subscription handler*/
void offset_handler(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure)
{
    // Get the data pointer (typecast it)
    struct data_ptrs *ptr_data = (struct data_ptrs *) closure;

    // Get the required pointer for the param buffer
    CircBuffer *param_buffer = ptr_data->param_buffer;//(CircBuffer*) closure;

    // Get the required pointer for the sync uncertainty
    SyncUncertainty *sync_uncertainty = ptr_data->sync_uncertainty;

    // Variable to hold the parameters
    peer_clk_params_t params;

    if (DEBUG_FLAG)
    {
        printf("Received msg: %s - %.*s\n",
               natsMsg_GetSubject(msg),
               natsMsg_GetDataLength(msg),
               natsMsg_GetData(msg));
    }

    /* De-serialize data */
    nlohmann::json data = nlohmann::json::parse(std::string(natsMsg_GetData(msg)));

    // Unroll the JSON
    for (auto it = data.begin(); it != data.end(); ++it)
    {
        if (it.key().compare(global_node_name) == 0)
        {
            params.timestamp = uint64_t(data[it.key()]["final time"].get<double>()*1000000000ULL);
            params.offset_ns = int64_t(data[it.key()]["offset"].get<double>()*1000000000LL);

            if (DEBUG_FLAG)
            {
              std::cout << "Node " << it.key() << "\n";
              std::cout << "final time is :" << params.timestamp << " ns\n";
              std::cout << "offset is     :" << params.offset_ns << " ns\n";
            }
            if (fabs(params.offset_ns) > 10 && set_counter % 10 == 0 && global_disc_flag)
            {
              std::cout << "PeerTSreceiver: Stepping the clock\n";
              clockadj_step(global_clkid, -params.offset_ns);
            }
            set_counter = (set_counter + 1) % 10;

            if (LOGGING_FLAG == 1 && ptr_data->clk_params)
                logfile << params.timestamp << "," << params.offset_ns << "," << ptr_data->clk_params->u_nsec << "," << ptr_data->clk_params->u_mult  << "\n";
            else if (LOGGING_FLAG == 1)
                logfile << params.timestamp << "," << params.offset_ns << ",0,0"  << "\n";

        }
    }

    // Add data to the circular buffer 
    if (param_buffer)
        param_buffer->AddElement(params);

    // Get the current offseted time
    struct timespec now;
    clock_gettime(global_clkid, &now);
    std::cout << "PeerTSreceiver: PHC Time         : " << now.tv_sec << " s " << now.tv_nsec << "\n";
    param_buffer->GetOffsettedTime(&now);
    std::cout << "PeerTSreceiver: Offset + PHC Time: " << now.tv_sec << " s " << now.tv_nsec << "\n";

    // Set the synchronization uncertainty
    if (sync_uncertainty && param_buffer && params.offset_ns != 0)
        sync_uncertainty->CalculateBounds(params.offset_ns, param_buffer->GetLatestDrift(), -1, ptr_data->clk_params, std::string("local"));

    // Need to destroy the message!
    natsMsg_Destroy(msg);

}

// Get the NATS connection status
int PeerTSreceiver::getNatsStatus()
{
    return (int)s;
}

// Connect to the NATS server
natsStatus PeerTSreceiver::natsConnect(const char* nats_url)
{
    s = natsConnection_ConnectTo(&conn, nats_url);
    if (s == NATS_OK)
    {
        std::cout << "Connected to NATS service\n";
    }
    else
    {
        std::cout << "Error Connecting to NATS service\n";
    }
    return s;
}

/* Subscribe to a NATS topic (subject) */
int PeerTSreceiver::natsSubscribe(std::string &topic)
{
    if (DEBUG_FLAG)
        printf("Subscribing to NATS subject %s\n", topic.c_str());

    // Create a new circular buffer
    try
    {
        param_buffer = new CircBuffer(CIRBUFF_DEFSIZE);
        data.param_buffer = param_buffer;
    }
    catch (std::bad_alloc &ba)
    {
        std::cout << "ERROR: Failed to allocate memory for param buffer class"; 
        param_buffer = NULL;
    }
    

    // Try to subscribe if the connection was succesful
    s = natsConnect(nats_server.c_str());
    if (s == NATS_OK)
    {
        if (DEBUG_FLAG)
            printf("Connected to NATS server\n");

        // Creates an asynchronous subscription on subject "foo".
        // When a message is sent on subject "foo", the callback
        // onMsg() will be invoked by the client library.
        // You can pass a closure as the last argument.
        s = natsConnection_Subscribe(&sub, conn, topic.c_str(), offset_handler, (void*) &data);
    }

    // If there was an error, print a stack trace and exit
    if (s != NATS_OK)
    {
        nats_PrintLastErrorStack(stderr);
        return (int)s;
    }
    else
    {
        if (DEBUG_FLAG)
            printf("Succesfully subscribed to timeline clock parameter topic\n");
    }

    return 0;
}

/* Un-subscribe to a NATS topic (subject) */
int PeerTSreceiver::natsUnSubscribe()
{
    // Anything that is created need to be destroyed
    if (s == NATS_OK)
    {
      natsSubscription_Destroy(sub);
      natsConnection_Destroy(conn);
      delete param_buffer;
    }
    param_buffer = NULL;
    conn = NULL;
    sub  = NULL;
    return 0;
}
#endif

/* Set the pointer to the variable which holds the estimated clock parameters */
int PeerTSreceiver::SetClkParamVar(tl_translation_t *set_clk_params)
{
  clk_params = set_clk_params;
  data.clk_params = clk_params;

  // Send the pointer to the parameter buffer
  if (param_buffer && clk_params)
  {
    param_buffer->SetClkParamVar(set_clk_params);
    return 0;
  }
  else
  {
    return -1;
  }
}

// Constructor
PeerTSreceiver::PeerTSreceiver(const std::string &node_name, const std::string &pub_server, const std::string &iface_name, bool discipline_flag)
  : node_uuid(node_name), proc_period_ns(1000000000), nats_server(pub_server), param_buffer(NULL), iface(iface_name), disc_flag(discipline_flag), sync_uncertainty(NULL)
{
    // Uncertainty Information Config
    struct uncertainty_params uncertainty_config;
    uncertainty_config.M = 50;      // Number of Estimated Drift Samples used -> Set to 50 (as per paper)
    uncertainty_config.N = 50;          // Number of Estimated Offset Samples used -> Set to 50 (as per paper)
    uncertainty_config.pds = 0.999999;  // Probability that drift variance less than upper bound -> Set to 0.999999 (as per paper)
    uncertainty_config.pdv = 0.999999;  // Probability of computing a safe bound on drift variation -> Set to 0.999999 (as per paper)
    uncertainty_config.pos = 0.999999;  // Probability that offset variance less than upper bound -> Set to 0.999999 (as per paper)
    uncertainty_config.pov = 0.999999;  // Probability of computing a safe bound on offset variance -> Set to 0.999999 (as per paper)

    // Set necessary variables
    global_node_name = node_uuid;
    global_disc_flag = disc_flag;

    // Initialize the "data_ptrs"
    data.sync_uncertainty = NULL;
    data.param_buffer = NULL;
    data.clk_params = NULL;

    // Create the sync uncertainty class
    try
    {
        sync_uncertainty = new SyncUncertainty(uncertainty_config);
        data.sync_uncertainty = sync_uncertainty;
    }
    catch (std::bad_alloc &ba)
    {
        std::cout << "ERROR: Failed to allocate memory for sync uncertainty calculation class"; 
        sync_uncertainty = NULL;
    }

    if (LOGGING_FLAG == 1)
    {
      std::string logfile_name = "/opt/qot-stack/doc/data/peerlog.csv";
      logfile.open(logfile_name);
    }

    #ifdef NATS_SERVICE
    // Initialize NATS Parameters
    conn = NULL;
    sub  = NULL;
    s = NATS_ERR;

    #endif
}

// Destructor
PeerTSreceiver::~PeerTSreceiver()
{
    if (LOGGING_FLAG == 1)
      logfile.close();

    if (sync_uncertainty)
      delete sync_uncertainty;
}

// Initialize the PHC which we are going to discipline
int PeerTSreceiver::phc_initialize()
{
  char phc[32];
  int phc_index;
  struct config *cfg;
  struct interface *iface_ptp;

  // Initialize Default Config
  cfg = config_create();;

  if (!config_create_interface(const_cast<char*>(iface.c_str()), cfg))
  {
    config_destroy(cfg);
    return -1;
  }

  iface_ptp = STAILQ_FIRST(&cfg->interfaces);

  if (iface_ptp->ts_info.valid) 
    phc_index = iface_ptp->ts_info.phc_index;
  else
    return -1;

  // Open the PTP clock for disciplining
  if (phc_index >= 0) {
    snprintf(phc, 31, "/dev/ptp%d", phc_index);
    clkid = phc_open(phc);
    if (clkid == CLOCK_INVALID) {
      printf("Failed to open %s: %m", phc);
      return -1;
    }
    int max_adj = phc_max_adj(clkid);
    if (!max_adj) {
      printf("clock is not adjustable");
      return -1;
    }
    clockadj_init(clkid);
    global_clkid = clkid;
  }
  else
  {
    return -1;
  }

  config_destroy(cfg);
  return 0;
}

int PeerTSreceiver::Start(uint64_t period_ns)
{
  int retval = 0;
  std::string topic = "qot.peer.offsets";
  /* Set the processing period */
  proc_period_ns = period_ns;

  // if (disc_flag)
  // {
    if (phc_initialize() < 0)
    {
      std::cout << "PeerTSreceiver: Failed to initialize PHC, falling back to CLOCK_REALTIME instead\n";
      global_clkid = CLOCK_REALTIME;
    }
  // }

  #ifdef NATS_SERVICE
  // Connect to NATS Service
  std::cout << "PeerTSreceiver: Connecting to NATS server on " << nats_server << "\n";
  retval = natsSubscribe(topic);
  #endif

  return retval;
}

int PeerTSreceiver::Stop()
{
  #ifdef NATS_SERVICE
  // Destroy the NATS connection
  std::cout << "PeerTSreceiver: Unsubscribing and destroying nats connection\n";
  natsUnSubscribe();
  #endif
  return 0;
}



