/*
 * @file qot_timeline_registry.cpp
 * @brief Timeline Registry Class functions in the QoT stack
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

#include <iostream>
#include <mutex>
#include <map>
#include <set>

// Internal Timeline Registry Class Header
#include "qot_timeline_registry.hpp"

using namespace qot_core;

/* Private functions */

/* Search for a timeline given by a name -> Should be held within qot_timeline_lock */
qot_timeline_t* TimelineRegistry::qot_timeline_find(char *name)
{
    // Check if pointer is not null
    if (!name)
        return NULL;
    
    qot_timeline_lock();
    // Find the data structure
    std::map<std::string, qot_timeline_t>::iterator it = qot_timeline_map.find(std::string(name));
    if (it != qot_timeline_map.end())
    {
        qot_timeline_unlock();
        return &it->second;
    }
    
    qot_timeline_unlock();
    return NULL;
}

/* Insert a timeline into our map data structure */
qot_return_t TimelineRegistry::qot_timeline_insert(qot_timeline_t &timeline)
{
    // Add timeline to map data structure
    qot_timeline_lock();
    qot_timeline_map[std::string(timeline.name)] = timeline;

    // Set the ID of the timeline
    if(!timeline_ids.empty())
        qot_timeline_map[std::string(timeline.name)].index = *timeline_ids.rbegin() + 1;
    else
        qot_timeline_map[std::string(timeline.name)].index = 0;

    // Add the ID to the set
    timeline_ids.insert(qot_timeline_map[std::string(timeline.name)].index);

    // Copy the data back
    timeline = qot_timeline_map[std::string(timeline.name)];

    qot_timeline_unlock();
    return QOT_RETURN_TYPE_OK;
}

/* Remove a timeline from our data structure */
qot_return_t TimelineRegistry::qot_timeline_delete(qot_timeline_t timeline)
{
    qot_timeline_lock();

    // Remove timeline id from the set
    timeline_ids.erase(timeline.index);

    // Remove timeline from the map data structure
    qot_timeline_map.erase(std::string(timeline.name));

    qot_timeline_unlock();
    return QOT_RETURN_TYPE_OK;
}

/* Public functions */

/* Hold the global timeline lock */
void TimelineRegistry::qot_timeline_lock()
{
    qot_timeline_mutex.lock();
}

/* Release the global timeline lock */
void TimelineRegistry::qot_timeline_unlock()
{
    qot_timeline_mutex.unlock();
}

/* Get information about a timeline */
qot_return_t TimelineRegistry::qot_timeline_get_info(qot_timeline_t &timeline)
{
    qot_timeline_t *timeline_priv = NULL;
    timeline_priv = qot_timeline_find(timeline.name);
    if (!timeline_priv)
        return QOT_RETURN_TYPE_ERR;
    timeline = *timeline_priv;
    return QOT_RETURN_TYPE_OK;
}

/* Update the timeline information  */
qot_return_t TimelineRegistry::qot_timeline_set_info(qot_timeline_t &timeline)
{
    qot_timeline_t *timeline_priv = NULL;
    timeline_priv = qot_timeline_find(timeline.name);
    if (!timeline_priv)
        return QOT_RETURN_TYPE_ERR;
    qot_timeline_lock();
    qot_timeline_map[std::string(timeline.name)] = timeline;
    qot_timeline_unlock();
    return QOT_RETURN_TYPE_OK;
}

/* Creata a new timeline */
qot_return_t TimelineRegistry::qot_timeline_register(qot_timeline_t &timeline)
{
    qot_timeline_t *timeline_priv = NULL;
    /* Make sure timeline doesn't already exist */
    timeline_priv = qot_timeline_find(timeline.name);
    if (timeline_priv)
    {
        /* If it exists return the timeline information */
        std::cout << "qot_timeline_registry: timeline already exists" << std::endl;
        timeline = *timeline_priv;
        return QOT_RETURN_TYPE_ERR;
    }

    /* Try and insert into the map */
    qot_timeline_insert(timeline);
    std::cout << "qot_timeline_registry: Timeline " << timeline.index << " registered name is " << timeline.name << std::endl;

    return QOT_RETURN_TYPE_OK;
}

/* Remove a timeline */
qot_return_t TimelineRegistry::qot_timeline_remove(qot_timeline_t &timeline, bool admin_flag)
{
    qot_timeline_t *timeline_priv = NULL;
 
    /* Make certain that timeline->index has been set and exists */
    timeline_priv = qot_timeline_find(timeline.name);
    if (!timeline_priv)
        return QOT_RETURN_TYPE_ERR;

    qot_timeline_delete(*timeline_priv);
    return QOT_RETURN_TYPE_OK;
}

/* Register the pointer to the new timeline class */
qot_return_t TimelineRegistry::qot_tl_class_register(int tl_index, void *tl_ptr)
{
    qot_timeline_lock();
    qot_tl_class_map[tl_index] = tl_ptr;
    qot_timeline_unlock();
}

/* Remove the pointer to the  timeline class */
qot_return_t TimelineRegistry::qot_tl_class_remove(int tl_index, bool admin_flag)
{
    qot_timeline_lock();
    qot_tl_class_map.erase(tl_index);
    qot_timeline_unlock();
}

/* Get the pointer to a timeline class */
void* TimelineRegistry::qot_tl_class_get(int tl_index)
{
    qot_timeline_lock();
    // Find the data structure
    std::map<int, void*>::iterator it = qot_tl_class_map.find(tl_index);
    if (it != qot_tl_class_map.end())
    {
        qot_timeline_unlock();
        return it->second;
    }
    return NULL;
    qot_timeline_unlock();
}

/* Remove all timelines */
void TimelineRegistry::qot_timeline_remove_all() {
    
    qot_timeline_lock();
    // Clear all the timelines in the map data structure
    qot_timeline_map.clear();
    qot_timeline_unlock();
}

// Constructor and Destructor can be expanded later based on intialization/destruction requirements
TimelineRegistry:: TimelineRegistry() {}
TimelineRegistry::~TimelineRegistry() 
{
    // Delete all the timelines
    qot_timeline_remove_all();
}
