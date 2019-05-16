/**
 * @file SyncUncertainty.hpp
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

#ifndef SYNC_UNCERTAINTY_HPP
#define SYNC_UNCERTAINTY_HPP

// System includes
#include <cstdint>
#include <vector>
#include <iostream>

// System dependencies
extern "C"
{
	#include <string.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <dirent.h>
	#include <math.h>

	#include "../../../qot_types.h"
}

#ifdef NATS_SERVICE
// NATS client header
#include <nats/nats.h>

// NATS Callback function
typedef void (*subscription_callback_t)(tl_translation_t params, std::string timeline_uuid, std::string node_name, uint64_t desired_accuracy);

#endif 

namespace qot
{	
	// Uncertainty Calculation Parameters Defintion
	struct uncertainty_params {
		int M;			// Number of Estimated Drift Samples used -> Set to 50 (as per paper)
		int N;          // Number of Estimated Offset Samples used -> Set to 50 (as per paper)
		double pds;     // Probability that drift variance less than upper bound -> Set to 0.999999 (as per paper)
		double pdv;     // Probability of computing a safe bound on drift variation -> Set to 0.999999 (as per paper)
		double pos;     // Probability that offset variance less than upper bound -> Set to 0.999999 (as per paper)
		double pov;     // Probability of computing a safe bound on offset variance -> Set to 0.999999 (as per paper)
	};

	// Base functionality
	class SyncUncertainty {
		// Constructor and destructor
		public: SyncUncertainty(struct uncertainty_params uncertainty_config);
		public: SyncUncertainty();
		public: ~SyncUncertainty();

		// Add Latest Statistic and Calculate Bounds
		public: bool CalculateBounds(int64_t offset, double drift, int timelinefd, tl_translation_t* tl_clk_params, const std::string &timeline_uuid);

		// Set Bounds Directly (not required if CalculateBounds is called)
		public: bool SetBounds(tl_translation_t* tl_clk_params, qot_bounds_t bounds, int timelinefd, const std::string &timeline_uuid);

		// Configure the Parameters of the Synchronization Uncertainty Calculation Algorithm
		public: void Configure(struct uncertainty_params configuration);

		#ifdef NATS_SERVICE
		// Set the master sync topic to share uncertainty info with synchronization master (for local timelines)
		public: bool StartMasterSyncPublish(std::string topic);

		// Stop sending data to the master sync (for local timelines)
		public: bool StopMasterSyncPublish();

		/* Subscribe to a NATS topic (subject) */
		public: int natsSubscribe(std::string &topic, subscription_callback_t callback);

		/* Un-subscribe from a NATS topic (subject) */
		public: int natsUnSubscribe();

		// Set the node name
		public: bool SetNodeUUID(std::string node_name);

		// Set the node desired accuracy
		public: bool SetNodeAccuracy(uint64_t accuracy);
		
		#endif

		// Add a new sample to the vector queues
		private: void AddSample(int64_t offset, double drift);

		// Calculate variance bounds
	    private: void CalcVarBounds();

	    // Helper functions for variance calculation
	    private: double GetPopulationVarianceDouble(std::vector<double> samples);
	    private: double GetPopulationVariance(std::vector<int64_t> samples);
	    private: double GetSampleVarianceDouble(std::vector<double> samples);

		// Uncertainty Calculation Parameters
		private: struct uncertainty_params config; 

		// Vector (Circular Buffer) of samples of uncertainty estimation
		private: std::vector<int64_t> offset_samples;  // Nanosecond offset
		private: std::vector<double> drift_samples;      // ppb/1Billion drift  

		// Pointers to maintain vector circular buffers (point to lates added elements)
		private: int offset_pointer; // Pointer to last element in the offset_samples buffer
		private: int drift_pointer;  // Pointer to last element in the drift_samples buffer

		// Estimated Variances
		private: double drift_popvar;   // Drift population variance
		private: double drift_samvar;   // Drift Sample Variance
		private: double offset_popvar;  // Offset Population Variance
 
		// Variance Bounds
		private: double drift_bound;    // Drift Variance Safe Upper Bound
		private: double offset_bound;   // Offset Variance Safe Upper Bound

		// Sync Uncertainty Calculation Constants
		private: double inv_error_pdv;
		private: double inv_error_pov;
		private: double left_predictor;
		private: double right_predictor;
		private: double right_margin;
		private: double left_margin; 

		#ifdef NATS_SERVICE
		// Connect to the NATS Server
		public: int natsConnect(const char* nats_url);
		// Return if the nats connection succeeded or not
		public: int getNatsStatus();

		// NATS messaging variables
		private: natsConnection      *conn;
	    private: natsMsg             *msg;
	    private: natsStatus          s;

	    // NATS Subscription
	    private: natsSubscription    *sub;
	    private: volatile bool       done;
		private: int nats_status_flag;    // Indicates connection went through

	    // Internal Sync NATS topic to share uncertainty info with SyncMaster
		private: std::string master_nats_topic;
		private: bool master_sync_topic_flag; // Flag indicating topic is set

		// Node Name
		private: std::string node_uuid;

		// Node desired accuracy
		private: uint64_t desired_accuracy;

		#endif
	};
}

#endif
