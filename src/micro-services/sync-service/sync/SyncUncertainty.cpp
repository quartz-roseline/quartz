/**
 * @file SyncUncertainty.cpp
 * @brief Synchronization Uncertainty Estimation Logic
 *        based on "Safe Estimation of Time Uncertainty of Local Clocks ISPCS 2009"
 * @author Anon D'Anon
 *
 * Copyright (c) Anon, 2017. All rights reserved.
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

#include "SyncUncertainty.hpp"

#include "ProbabilityLib.hpp"

#ifdef NATS_SERVICE
// Header to clkparam serialization library
#include "../qot_clkparams_serialize.hpp"
#endif

/* So that we might expose a meaningful name through PTP interface */
#define DEBUG true
#define QOT_IOCTL_BASE          "/dev"
#define QOT_IOCTL_PTP           "ptp"
#define QOT_IOCTL_PTP_FORMAT    "%3s%d"
#define QOT_MAX_PTP_NAMELEN     32

using namespace qot;

#ifdef NATS_SERVICE
int SyncUncertainty::getNatsStatus()
{
	return (int)s;
}

int SyncUncertainty::natsConnect(const char* nats_url)
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
	return (int)s;
}
#endif

// Constructor 
SyncUncertainty::SyncUncertainty(struct uncertainty_params uncertainty_config)
: drift_popvar(0), drift_samvar(0), offset_popvar(0), drift_bound(0), 
  offset_bound(0), drift_pointer(0), offset_pointer(0), master_sync_topic_flag(0), 
  config{50,50,0.999999,0.999999,0.999999,0.999999}
{
	// Configure the parameters
	Configure(uncertainty_config);

	#ifdef NATS_SERVICE
	// Initialize NATS Parameters
	conn = NULL;
    msg  = NULL;
    s = NATS_ERR;
    node_uuid = std::string("default");

	#endif
}

// Constructor 2
SyncUncertainty::SyncUncertainty()
: drift_popvar(0), drift_samvar(0), offset_popvar(0), drift_bound(0), 
  offset_bound(0), drift_pointer(0), offset_pointer(0), master_sync_topic_flag(0),
  config{50,50,0.999999,0.999999,0.999999,0.999999}
{
	#ifdef NATS_SERVICE
	// Initialize NATS Parameters
	conn = NULL;
    msg  = NULL;
    s = NATS_ERR;
    node_uuid = std::string("default");

	#endif
	return;
}

// Destructor
SyncUncertainty::~SyncUncertainty() 
{
	#ifdef NATS_SERVICE
	// Destroy the NATS connection
	if (s == NATS_OK)
	{
    	natsConnection_Destroy(conn);
    }
	#endif
}

#ifdef NATS_SERVICE
// Set the master sync topic to share uncertainty info with synchronization master (for local timelines)
bool SyncUncertainty::StartMasterSyncPublish(std::string topic)
{
	master_nats_topic = topic;
	master_sync_topic_flag = true;
	return true;
}

// Stop sending data to the master sync (for local timelines)
bool SyncUncertainty::StopMasterSyncPublish()
{
	master_sync_topic_flag = false;
	return true;
}

/* NATS Subscription handler*/
void subscription_handler(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure)
{
    // CircularBuffer *param_buffer = (CircularBuffer*) closure;
    subscription_callback_t callback = (subscription_callback_t) closure;

    // printf("Received msg: %s - %.*s\n",
    //            natsMsg_GetSubject(msg),
    //            natsMsg_GetDataLength(msg),
    //            natsMsg_GetData(msg));

    /* De-serialize data */
    tl_translation_t rcv_clk_params;
    json data = nlohmann::json::parse(std::string(natsMsg_GetData(msg)));
    deserialize_clkparams(data, rcv_clk_params);

    // Get the node uuid
    std::string node_uuid = data["node_uuid"].get<std::string>();

    // Get the timeline uuid
    std::string topic_prefix = "qot.timeline.";
    std::string timeline_uuid = std::string(natsMsg_GetSubject(msg));
    int pos_start = topic_prefix.length();
    int pos_end = timeline_uuid.find(".syncmaster"); // can cause issues if the timeline is named syncmaster
    timeline_uuid = timeline_uuid.substr(pos_start, pos_end - pos_start);

    // Get the desired accuracy
    uint64_t desired_accuracy = data["desired_accuracy"].get<uint64_t>();

    // Call the callback function
    callback(rcv_clk_params, timeline_uuid, node_uuid, desired_accuracy);
    //printf("Received delivered Qot msg from node %s on timeline %s\n", node_uuid.c_str(), timeline_uuid.c_str());
    //printf("Received delivered Qot msg from node %s on timeline %llu\n", node_uuid.c_str(), desired_accuracy);

    // Need to destroy the message!
    natsMsg_Destroy(msg);

}

/* Subscribe to a NATS topic (subject) */
int SyncUncertainty::natsSubscribe(std::string &topic, subscription_callback_t callback)
{
    printf("Subscribing to NATS subject %s\n", topic.c_str());
	// Need to connect to NATS before    
    if (s == NATS_OK)
    {
        nats_status_flag = 1; // Indicates the connection went through

        // Creates an asynchronous subscription on subject "foo".
        // When a message is sent on subject "foo", the callback
        // onMsg() will be invoked by the client library.
        // You can pass a closure as the last argument.
        s = natsConnection_Subscribe(&sub, conn, topic.c_str(), subscription_handler, (void*) callback);
    }

    // If there was an error, print a stack trace and exit
    if (s != NATS_OK)
    {
        nats_PrintLastErrorStack(stderr);
        return (int)s;
    }
    else
    {
        printf("Succesfully subscribed to timeline clock parameter topic\n");
    }

    return NATS_OK;
}

/* Un-subscribe to a NATS topic (subject) */
int SyncUncertainty::natsUnSubscribe()
{
    // Anything that is created need to be destroyed
    if (s == NATS_OK)
    {
        natsSubscription_Destroy(sub);
    }
    sub  = NULL;
    done  = false;
    return 0;
}

// Set the node name
bool SyncUncertainty::SetNodeUUID(std::string node_name)
{
	node_uuid = node_name;
	return true;
}

// Set the desired accuracy
bool SyncUncertainty::SetNodeAccuracy(uint64_t accuracy)
{
	desired_accuracy = accuracy;
	return true;
}

#endif

// Set Bounds Directly (not required if CalculateBounds is called)
bool SyncUncertainty::SetBounds(tl_translation_t* tl_clk_params, qot_bounds_t bounds, int timelinefd, const std::string &timeline_uuid)
{
	#ifdef QOT_TIMELINE_SERVICE
	// Write to shared memory
	if (tl_clk_params != NULL)
	{
		tl_clk_params->u_nsec = bounds.u_nsec;
		tl_clk_params->l_nsec = -bounds.l_nsec;  // Take care of negative sign here only -> Kernel Space implementation does it in the kernel
		tl_clk_params->u_mult = bounds.u_drift;
		tl_clk_params->l_mult = -bounds.l_drift; // Take care of negative sign here only -> Kernel Space implementation does it in the kernel
	}

	#ifdef NATS_SERVICE
    // Publish the message to NATS
    if (s == NATS_OK && tl_clk_params != NULL)
    {
	    msg = NULL;
	    // Convert the params to json
	    json params = serialize_clkparams(*tl_clk_params);
	    std::string data = params.dump();

	    // Construct the topic name
	    std::string nats_subject = "qot.timeline.";
	    nats_subject.append(timeline_uuid);
	    nats_subject.append(std::string(".params"));

	    s = natsMsg_Create(&msg, nats_subject.c_str(), NULL, data.c_str(), data.length());
	    if (s == NATS_OK)
			natsConnection_PublishMsg(conn, msg);
		natsMsg_Destroy(msg);
		std::cout << "Published Message to NATS on topic " << nats_subject << "\n";
	}
	#endif

	#else
	// Write calculated uncertainty information to the timeline character device
	if(ioctl(timelinefd, TIMELINE_SET_SYNC_UNCERTAINTY, &bounds)){
		std::cout << "Setting sync uncertainty failed for timeline\n";
	}
	#endif

	return true;
}

// Add Latest Statistic and Calculate Bounds
bool SyncUncertainty::CalculateBounds(int64_t offset, double drift, int timelinefd, tl_translation_t* tl_clk_params, const std::string &timeline_uuid)
{
	qot_bounds_t bounds; // Calculated bound values

	// Add Newest Sample
	AddSample(offset, drift);

	if(drift_samples.size() < config.M && offset_samples.size() < config.N)
	{
		// Insufficient samples for calculating uncertainty
		#ifdef NATS_SERVICE
	    // Publish the message to NATS
	    if (s == NATS_OK && tl_clk_params != NULL)
	    {
		    msg = NULL;
		    // Convert the params to json
		    json params = serialize_clkparams(*tl_clk_params);
		    std::string data = params.dump();

		    // Construct the topic name
		    std::string nats_subject = "qot.timeline.";
		    nats_subject.append(timeline_uuid);
		    nats_subject.append(std::string(".params"));

		    s = natsMsg_Create(&msg, nats_subject.c_str(), NULL, data.c_str(), data.length());
		    if (s == NATS_OK)
		    {
				natsConnection_PublishMsg(conn, msg);
				std::cout << "Published Message to NATS on topic " << nats_subject << "\n";
		    }
			natsMsg_Destroy(msg);
		}
		#endif
		return false;
	}

	// Calculate Drift and Offset Variance Bounds
	CalcVarBounds();

	// Predictor Function Coefficients (Needs to be multiplied with (t-t0)^(3/2))
	//right_predictor = 2*inv_error_pdv*sqrt(diffusion_coef)/3; // Like the paper we assume that the sync algorithm corrects the offset and drift
	right_predictor = 2*upper_confidence_limit_gaussian(sqrt(drift_bound),config.pdv)/3;
	left_predictor = -right_predictor;

	// Margin Functions
	right_margin = sqrt(2)*inv_error_pov*sqrt(offset_bound);
	left_margin = -right_margin;

	std::cout << "Timeline " << timeline_uuid << " Right Predictor = " << right_predictor
	          << " Right Margin = " << right_margin << "\n";

	// Poulate the bounds
	bounds.u_drift = (s64)ceil(right_predictor*1000000000LL); // Upper bound (Right Predictor) function for drift
	bounds.l_drift = (s64)ceil(left_predictor*1000000000LL);  // Lower bound (Left Predictor) function for drift
	bounds.u_nsec  = (s64)ceil(right_margin);                 // Upper bound (Right Margin) function for offset
	bounds.l_nsec  = (s64)ceil(left_margin);                  // Lower bound (Left Margin) function for offset

	#ifdef QOT_TIMELINE_SERVICE
	// Write to shared memory
	if (tl_clk_params != NULL)
	{
		tl_clk_params->u_nsec = bounds.u_nsec;
		tl_clk_params->l_nsec = -bounds.l_nsec;  // Take care of negative sign here only -> Kernel Space implementation does it in the kernel
		tl_clk_params->u_mult = bounds.u_drift;
		tl_clk_params->l_mult = -bounds.l_drift; // Take care of negative sign here only -> Kernel Space implementation does it in the kernel
	}

	#ifdef NATS_SERVICE
    // Publish the message to NATS
    if (s == NATS_OK && tl_clk_params != NULL)
    {
	    msg = NULL;
	    // Convert the params to json
	    json params = serialize_clkparams(*tl_clk_params);
	    // Add node name
	    params["node_uuid"] = node_uuid;
	    // Add node desired accuracy
	    params["desired_accuracy"] = desired_accuracy;
	    std::string data = params.dump();

	    // Construct the topic name
	    std::string nats_subject = "qot.timeline.";
	    nats_subject.append(timeline_uuid);
	    nats_subject.append(std::string(".params"));

	    s = natsMsg_Create(&msg, nats_subject.c_str(), NULL, data.c_str(), data.length());
	    if (s == NATS_OK)
			natsConnection_PublishMsg(conn, msg);
		natsMsg_Destroy(msg);
		std::cout << "Published Message to NATS on topic " << nats_subject << "\n";

		// Send the uncertainty information to the sync master
		if (master_sync_topic_flag)
	    {
		    msg = NULL;
		    s = natsMsg_Create(&msg, master_nats_topic.c_str(), NULL, data.c_str(), data.length());
		    if (s == NATS_OK)
				natsConnection_PublishMsg(conn, msg);
			natsMsg_Destroy(msg);
			std::cout << "Published Message on NATS to sync master on topic " << master_nats_topic << "\n";
		}
	}

	#endif

	#else
	// Write calculated uncertainty information to the timeline character device
	if(ioctl(timelinefd, TIMELINE_SET_SYNC_UNCERTAINTY, &bounds)){
		std::cout << "Setting sync uncertainty failed for timeline\n";
	}
	#endif

	return true;
}

// Configure the Parameters of the Synchronization Uncertainty Calculation Algorithm
void SyncUncertainty::Configure(struct uncertainty_params configuration)
{
	config = configuration;
	inv_error_pov = get_inverse_error_func(config.pov);
	return;
}

// Add a new sample to the vector queues
void SyncUncertainty::AddSample(int64_t offset, double drift)
{
	// Add new drift sample
	if (drift_samples.size() < config.M)
	{
		drift_samples.push_back(drift);
	}
	else
	{
		drift_samples[drift_pointer] = drift;
	}
	drift_pointer = (drift_pointer + 1) % config.M;

	// Add new offset sample
	if (offset_samples.size() < config.N)
	{
		offset_samples.push_back(offset);
	}
	else
	{
		offset_samples[offset_pointer] = offset;
	}
	offset_pointer = (offset_pointer + 1) % config.N;

	return;
}

// Calculate population variance
double SyncUncertainty::GetPopulationVarianceDouble(std::vector<double> samples)
{
	double var = 0;
	double mean = 0;
	int numPoints = samples.size();
	int n;

	// Calculate Mean
	for(n = 0; n < numPoints; n++)
	{
		mean = mean + samples[n];
	}
	mean = mean/numPoints;

	// Calculate Variance
	for(n = 0; n < numPoints; n++)
	{
	   var += (samples[n] - mean)*(samples[n] - mean);
	}
	var = var/numPoints;

	return var;
}

// Calculate population variance
double SyncUncertainty::GetPopulationVariance(std::vector<int64_t> samples)
{
	double var = 0;
	double mean = 0;
	int numPoints = samples.size();
	int n;

	// Calculate Mean
	for(n = 0; n < numPoints; n++)
	{
		mean = mean + (double)samples[n];
	}
	mean = mean/numPoints;

	// Calculate Variance
	for(n = 0; n < numPoints; n++)
	{
	   var += ((double)samples[n] - mean)*((double)samples[n] - mean);
	}
	var = var/numPoints;

	return var;
}

// Calculate sample variance
double SyncUncertainty::GetSampleVarianceDouble(std::vector<double> samples)
{
	double var = 0;
	double mean = 0;
	int numPoints = samples.size();
	int n;

	// Calculate Mean
	for(n = 0; n < numPoints; n++)
	{
		mean = mean + samples[n];
	}
	mean = mean/numPoints;

	// Calculate Variance
	for(n = 0; n < numPoints; n++)
	{
	   var += (samples[n] - mean)*(samples[n] - mean);
	}
	var = var/(numPoints-1);

	return var;
}

// Calculate variance bounds
void SyncUncertainty::CalcVarBounds()
{
	// Calculate Variances
	drift_popvar  = GetPopulationVarianceDouble(drift_samples);   // Drift population variance
    drift_samvar  = GetSampleVarianceDouble(drift_samples);       // Drift Sample Variance
    offset_popvar = GetPopulationVariance(offset_samples);  // Offset Population Variance

    std::cout << "Drift Variance = " << drift_popvar << "\n";
    // Get the upper bound on the drift variance using the chi squared distribution
    drift_bound = upper_confidence_limit_on_std_deviation(sqrt(drift_popvar), config.M, config.pds);

    std::cout << "Offset Variance = " << offset_popvar << "\n";
    // Get the upper bound on the offset variance using the chi squared distribution
    offset_bound = upper_confidence_limit_on_std_deviation(sqrt(offset_popvar), config.N, config.pos);;
}