/*
 * @file qot_timeline_registry.hpp
 * @brief Timeline Registry class header in the QoT stack
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

#ifndef QOT_TIMELINE_REGISTRY_HPP
#define QOT_TIMELINE_REGISTRY_HPP

#include <map>
#include <mutex>
#include <set>

// Include the QoT Data Types
extern "C"
{
	#include "../../qot_types.h"
}

namespace qot_core
{
	// Timeline Registry class
	class TimelineRegistry
	{
		// Constructor and Destructor
		public: TimelineRegistry();
		public: ~TimelineRegistry();

		// Find if a timeline already exists in the registry
		private: qot_timeline_t* qot_timeline_find(char *name);

		// Insert a timeline into the registry
	    private: qot_return_t qot_timeline_insert(qot_timeline_t &timeline);

	    // Delete a timeline from the registryß
	    private: qot_return_t qot_timeline_delete(qot_timeline_t timeline);

		/* Map used to store timelines */
		private: std::map<std::string, qot_timeline_t> qot_timeline_map;

		/* Map used to store timeline class pointers */
		private: std::map<int, void*> qot_tl_class_map;

		/* Timeline map mutex used to protect the data structure */
		private: std::mutex qot_timeline_mutex;

		// Set used to give out timeline ids
		private: std::set<int> timeline_ids;

		// Lock and unlock the data structure while iterating
		public: void qot_timeline_lock();
		public: void qot_timeline_unlock();

		// Query the timeline registry to get information about a timeline
		public: qot_return_t qot_timeline_get_info(qot_timeline_t &timeline);

		/* Register a new timeline */
		public: qot_return_t qot_timeline_register(qot_timeline_t &timeline);

		/* Remove a timeline */
		public: qot_return_t qot_timeline_remove(qot_timeline_t &timeline, bool admin_flag);

		/* Register the pointer to a timeline class */
		public: qot_return_t qot_tl_class_register(int tl_index, void *tl_ptr);

		/* Remove the pointer to a timeline class */
		public: qot_return_t qot_tl_class_remove(int tl_index, bool admin_flag);

		/* Get a pointer to a timeline class */
		public: void* qot_tl_class_get(int tl_index);

		/* Update the registry entry */
		public: qot_return_t qot_timeline_set_info(qot_timeline_t &timeline);

		/* Delete all the existing timelines */
		public: void qot_timeline_remove_all();

		/* Iterators for iterating over the Timeline Registry */
		/* Note: Hold the timeline lock while iterating */
		public: typedef std::map<std::string, qot_timeline_t>::iterator iterator;
  		public: typedef std::map<std::string, qot_timeline_t>::const_iterator const_iterator;
		public: iterator begin() { return qot_timeline_map.begin(); }
  		public: iterator end() { return qot_timeline_map.end(); }

	};
}



#endif