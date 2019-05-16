/*
 * @file qot_timeline.hpp
 * @brief Timeline class header in the QoT stack
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

#ifndef QOT_TIMELINE_CORE_HPP
#define QOT_TIMELINE_CORE_HPP

#include <map>
#include <mutex>
#include <vector>

// Timeline Coordination Service REST Interface
#include "qot_timeline_rest.hpp"

// Include the QoT Data Types
extern "C"
{
	#include "../../qot_types.h"
}

// Include the timeline registry class header
#include "qot_timeline_registry.hpp"

// Include the timeline clock class header
#include "qot_timeline_clock.hpp"

// Include the sync service communication interface class header
#include "qot_synccomm.hpp"

// Include the timeline subscriber
#include "qot_timeline_subscriber.hpp"

namespace qot_core
{
	// Must be initialized in the timeline service
	extern TimelineClock* GlobalClock;

	// Must be initialized in the timeline service
	extern TimelineClock* LocalClock;

	// Timeline class
	class TimelineCore
	{
		// Constructor and Destructor
		public: TimelineCore(qot_timeline_t& timeline, TimelineRegistry& registry, std::string &node_name, std::string &rest_server, std::string &nats_server);
		public: ~TimelineCore();

		// Query the status flag to know the construction status
		public: int query_status_flag();

		// Get the timeline info
		public: qot_timeline_t get_timeline_info();

		// Get the desired timeline QoT info
		public: timequality_t get_desired_qot();

		// Create a binding to this timeline
		public: qot_return_t create_binding(qot_binding_t &binding);

		// Delete a binding from this timeline
		public: qot_return_t delete_binding(qot_binding_t binding);

		// Update the QoT requirements of a binding
		public: qot_return_t update_binding(qot_binding_t &binding); 

		// Update the Global Node Info from the coordination service
		public: qot_return_t update_global_coordination_info(std::vector<qot_node_phy_t> &node_vector); 

		// Update the Local Node Info from the coordination service
		public: qot_return_t update_local_coordination_info(std::vector<qot_node_phy_t> &node_vector); 

		// Update the List of Corresponding peer clients & the overlay clock
		public: qot_return_t update_local_peers(std::vector<std::string> &node_vector); 

		// Start the Peer Sync (Cluster-Local Clock Syncronization)
		public: qot_return_t start_peer_sync(); 

		// Stop the Peer Sync (Cluster-Local Clock Syncronization)
		public: qot_return_t stop_peer_sync(); 

		// Get the number of bindings
		public: int get_binding_count();

		/* Get the Clock Parameters Shared Memory file descriptor for the main clock */
		public: int get_shm_fd();

		/* Get the Clock Parameters Read-only shared memory file descriptor for the main clock */
		public: int get_rdonly_shm_fd();

		/* Get the translation parameters of the main timeline clock */
		public: int get_translation_params(tl_translation_t &params);

		/* Get the Clock Parameters Shared Memory file descriptor for the overlay clock */
		public: int get_overlay_shm_fd();

		/* Get the Clock Parameters Read-only shared memory file descriptor for the overlay clock */
		public: int get_overlay_rdonly_shm_fd();

		/* Get the translation parameters of the overlay timeline clock */
		public: int get_overlay_translation_params(tl_translation_t &params);

		// Get the Timeline Server
		public: int get_server(qot_server_t &server); 

		// Set the Timeline Server
		public: int set_server(qot_server_t &server); 
		
		// Update the timeline QoT requirements
		private: void update_timeline_qot(std::string meta_data); 

		// Private Variables
		private: qot_timeline_t timeline_info;   	// Timeline Info
		private: int status_flag; 					// Status of the Constructor
		private: TimelineRegistry &tl_registry;		// Reference to timeline registry
		private: TimelineClock* tl_clock; 			// Pointer to the primary timeline clock (global/local)
		private: TimelineClock* tl_overlay_clock; 	// Pointer to the overlay timeline clock (only for local clocks)

		/* Timeline map mutex used to protect the data structure */
		private: std::mutex binding_mutex;

		// Set used to give out binding ids
		private: std::set<int> binding_ids;

		// Binding Map
		private: std::map<int, qot_binding_t> binding_map;

		// Communication channel with the synchronization service
		private: SyncCommunicator communicator;

		// Coordination Service REST Interface
		private: TimelineRestInterface rest_interface; 

		// Timeline Coordination Service Subscriber
		private: TimelineSubscriber subscriber;

		// Node Unique ID
		private: std::string node_uuid; 

		// Vector of Peers (Peer Sync)
		private: std::vector<std::string> peers; 

	};
}



#endif