/*
 * @file qot_timeline.cpp
 * @brief Timeline Class functions in the QoT stack
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
#include <new>

// Internal Timeline Class Header
#include "qot_timeline.hpp"

// Timeline service header
#include "qot_timeline_service.hpp"

// Sync service header
#include "../sync-service/qot_sync_service.hpp"

using namespace qot_core;

// Must be initialized in the timeline service
TimelineClock* qot_core::GlobalClock = NULL;
TimelineClock* qot_core::LocalClock = NULL;

/* Private functions */

/* Update timeline QoT requirements */
void TimelineCore::update_timeline_qot(std::string meta_data)
{
    timequality_t demand;
    // Initialize the quality parameters to a high value
    demand.resolution.sec = 1000000000;
    demand.resolution.asec = 0;
    demand.accuracy.below = demand.resolution;
    demand.accuracy.above = demand.resolution;

    // If no bindings exist then set to zero
    if (binding_map.empty())
    {
        demand.resolution.sec = 0;
        demand.resolution.asec = 0;
        demand.accuracy.below = demand.resolution;
        demand.accuracy.above = demand.resolution;
        tl_clock->set_quality(demand);
        if (tl_overlay_clock)
            tl_overlay_clock->set_quality(demand);
        return;
    }

    for (std::map<int,qot_binding_t>::iterator it=binding_map.begin(); it!=binding_map.end(); ++it)
    {
        // Update resolution as minimum required resolution
        if (timelength_cmp(&demand.resolution, &it->second.demand.resolution) == -1)
            demand.resolution = it->second.demand.resolution;

        // Update accuracy lower bound as the minimum required accuracy
        if (timelength_cmp(&demand.accuracy.below, &it->second.demand.accuracy.below) == -1)
            demand.accuracy.below = it->second.demand.accuracy.below;

        // Update accuracy lower bound as the minimum required accuracy
        if (timelength_cmp(&demand.accuracy.above, &it->second.demand.accuracy.above) == -1)
            demand.accuracy.above = it->second.demand.accuracy.above;
    }
    // Set the demand for the timeline clock
    tl_clock->set_quality(demand);
    if (tl_overlay_clock)
            tl_overlay_clock->set_quality(demand);

    // Update the qot on the rest interface
    unsigned long long accuracy = TL_TO_nSEC(demand.accuracy.above);
    unsigned long long resolution = TL_TO_nSEC(demand.resolution);
    rest_interface.put_node(std::string(timeline_info.name), node_uuid, accuracy, resolution);

    // Send the sync service a message
    qot_sync_msg_t msg;
    msg.demand = demand;
    msg.msgtype = TL_CREATE_UPDATE;
    msg.info = timeline_info;
    msg.data = meta_data;
    communicator.send_request(msg);

    // If local timeline start or update the peer sync -> If peers empty assume PTP?
    if (timeline_info.type == QOT_TIMELINE_LOCAL && !peers.empty())
        start_peer_sync();
}

/* Public functions */

/* Constructor: Create a new timeline */
TimelineCore::TimelineCore(qot_timeline_t& timeline, TimelineRegistry& registry, std::string &node_name, std::string &rest_server, std::string &nats_server)
 : tl_registry(registry), status_flag(0), tl_clock(NULL), tl_overlay_clock(NULL), rest_interface(rest_server), node_uuid(node_name), subscriber(nats_server, std::string(timeline.name), this)
{
    qot_return_t retval;
    // Register the timeline into the registry
    retval = registry.qot_timeline_register(timeline);

    // If the timeline already exists
    if (retval == QOT_RETURN_TYPE_ERR)
    {
        status_flag = 1;  // Timeline already exists
        return;
    }

    /* Copy over the timeline information to some new private memory */
    qot_timeline_t timeline_new = timeline;
    
    /* Try and register the clock for the new timeline */
    try
    {
        if (timeline.type == QOT_TIMELINE_LOCAL)
        {
            tl_clock = LocalClock; // Primary clock
            tl_overlay_clock = new TimelineClock(timeline_new, false); // Overlay clock
        }
        else    // Timeline is global
        {
            tl_clock = GlobalClock;
            tl_overlay_clock = NULL;    // Not required for global timeline
        }

    }
    catch (std::bad_alloc &ba)
    {
        status_flag = 2;            // Memory allocation failed

        // Unregister the timeline
        registry.qot_timeline_remove(timeline,1);
        return;
    }

    if (timeline_new.index < 0) {
        std::cout << "qot_timeline: cannot create the clock" << std::endl;
        // Delete the Clock
        if (timeline.type == QOT_TIMELINE_LOCAL) 
            delete tl_overlay_clock;

        // Set TL clocks back to null
        tl_clock = NULL;
        tl_overlay_clock = NULL;

        // Unregister the timeline
        registry.qot_timeline_remove(timeline,1);

        // Copy over the timeline data structure
        timeline = timeline_new;

        // Change the status flag
        status_flag = 3;            // Unable to create a timeline clock
        return;
    }

    /* Update the timeline registry with the timeline ID */
    registry.qot_timeline_set_info(timeline_new);

    /* Add the class to the registry */
    registry.qot_tl_class_register(timeline_new.index, (void*)this);

    /* Subscribe to notifications from the Coordination Service */
    subscriber.natsSubscribe();

    /* Register Timeline with Coordination Service */
    rest_interface.post_timeline(std::string(timeline_new.name));

    /* Copy the Timeline data structure with the assigned ID back to the user and to the in class data structure*/
    timeline = timeline_new;
    timeline_info = timeline;
    
    std::cout << "qot_timeline: Timeline " << timeline.index << " created name is " << timeline.name << std::endl;

    return;
}

/* Destructor: Remove a timeline */
TimelineCore::~TimelineCore()
{
    if (status_flag != 0)
    {
        return;
    }

    // Unregister the timeline
    tl_registry.qot_timeline_remove(timeline_info,1);

    // Remove the class from the registry
    tl_registry.qot_tl_class_remove(timeline_info.index,1);

    // Send the sync service a message
    // qot_timeline_msg_t msg;
    // msg.msgtype = TIMELINE_DESTROY;
    qot_sync_msg_t msg;
    msg.msgtype = TL_DESTROY;
    msg.info = timeline_info;
    communicator.send_request(msg);

    std::cout << "qot_timeline: Timeline " << timeline_info.index << " destroyed name is " << timeline_info.name << std::endl;

    // Delete the node on the timeline 
    rest_interface.delete_node(std::string(timeline_info.name), node_uuid);

    // Remove the subscriber
    subscriber.natsUnSubscribe();

    // Stop the Peer Sync if it exists
    if (timeline_info.type == QOT_TIMELINE_LOCAL && !peers.empty())
    {
        stop_peer_sync();
    }

    // Delete the timeline clock -> Removing for now as we have a single timeline clock
    if (tl_overlay_clock && timeline_info.type == QOT_TIMELINE_LOCAL)
        delete tl_overlay_clock;

    return;
}

/* Query the status flag */
int TimelineCore::query_status_flag()
{
    return status_flag;
}

/* Get the timeline info */
qot_timeline_t TimelineCore::get_timeline_info()
{
    return timeline_info;
}

// Create a binding to this timeline
qot_return_t TimelineCore::create_binding(qot_binding_t &binding)
{
    std::string meta_data;
    binding_mutex.lock();
    
    // Check if the binding exists
    if (binding_map.find(binding.id) != binding_map.end() && !binding_ids.empty())
    {
        binding_mutex.unlock();
         std::cout << "qot_timeline: new binding failed, binding id = " << binding.id << "exists\n";
        return QOT_RETURN_TYPE_ERR;
    }

    // Set the ID of the binding
    if(!binding_ids.empty())
    {
        binding.id = *binding_ids.rbegin() + 1;
    }
    else
    {
        binding.id = 0;
    }

    unsigned long long accuracy = TL_TO_nSEC(binding.demand.accuracy.above);
    unsigned long long resolution = TL_TO_nSEC(binding.demand.resolution);
    std::cout << "qot_timeline: new binding " << binding.id << " created on timeline " << timeline_info.name << "\n";
    std::cout << "qot_timeline: acc = " << accuracy << " res = " << resolution << "\n";

    // Add to the Binding ID set
    binding_ids.insert(binding.id);

    // Populate the map with new binding
    binding_map[binding.id] = binding;

    if (binding.id == 0)
    {
        // First binding post the node 
        rest_interface.post_node(std::string(timeline_info.name), node_uuid, accuracy, resolution);

        // Check if the timeline is local & PTP is being used (peers.empty())
        if (timeline_info.type == QOT_TIMELINE_LOCAL && peers.empty())
        {
            meta_data = rest_interface.get_timeline_metadata(std::string(timeline_info.name));
            std::cout << "qot_timeline: Got Timeline Metadata " << meta_data << "\n";
            // Check if meta-data is not set
            if ((meta_data.compare(std::string("NULL"))) == 0) 
            {
                // Get the coordination ID
                int coord_id = rest_interface.get_timeline_coord_id(std::string(timeline_info.name));
                
                // Set a PTP domain
                meta_data = std::to_string(coord_id);
                rest_interface.put_timeline_metadata(std::string(timeline_info.name), meta_data);
            }
        }
    }

    update_timeline_qot(meta_data);
    binding_mutex.unlock();
    return QOT_RETURN_TYPE_OK;
}

// Delete a binding from this timeline
qot_return_t TimelineCore::delete_binding(qot_binding_t binding)
{
    binding_mutex.lock();
    // Check if the binding exists
    if (binding_map.find(binding.id) != binding_map.end())
    {
        binding_map.erase(binding.id);
        binding_ids.erase(binding.id);
        update_timeline_qot(std::string("NULL"));
        binding_mutex.unlock();
        return QOT_RETURN_TYPE_OK;
    }
    else    // Binding does not exists
    {
        binding_mutex.unlock();
        return QOT_RETURN_TYPE_ERR;
    }
}

// Update the QoT requirements of a binding
qot_return_t TimelineCore::update_binding(qot_binding_t &binding)
{
    binding_mutex.lock();
    // Check if the binding exists
    if (binding_map.find(binding.id) != binding_map.end())
        return QOT_RETURN_TYPE_ERR;
    binding_map[binding.id] = binding;
    update_timeline_qot(std::string("NULL"));
    binding_mutex.unlock();
    return QOT_RETURN_TYPE_OK;
}

// Get the number of bindings
int TimelineCore::get_binding_count()
{
    return binding_map.size();
}

// Get the desired timeline QoT info
timequality_t TimelineCore::get_desired_qot()
{
    if (tl_overlay_clock)
        return tl_overlay_clock->get_desired_quality();
    else
        return tl_clock->get_desired_quality();
}

/* Get the Shared Memory file descriptor for the main clock */
int TimelineCore::get_shm_fd()
{
    if (!tl_clock)
        return -1;
    // Needs to be tested
    return tl_clock->get_shm_fd();
}

/* Get the Read-only shared memory file descriptor for the main clock*/
int TimelineCore::get_rdonly_shm_fd()
{
    if (!tl_clock)
        return -1;
    // Needs to be tested
    return tl_clock->get_rdonly_shm_fd();
}

/* Get the translation parameters of the main clock */
int TimelineCore::get_translation_params(tl_translation_t &params)
{
    if (!tl_clock)
        return -1;
    // Copy over the parameters if the pointer is valid
    params = tl_clock->get_translation_params();
    return 0;
}

/* Get the Shared Memory file descriptor for the overlay clock */
int TimelineCore::get_overlay_shm_fd()
{
    if (!tl_overlay_clock)
        return -1;
    // Needs to be tested
    return tl_overlay_clock->get_shm_fd();
}

/* Get the Read-only shared memory file descriptor for the overlay clock*/
int TimelineCore::get_overlay_rdonly_shm_fd()
{
    if (!tl_overlay_clock)
        return -1;
    // Needs to be tested
    return tl_overlay_clock->get_rdonly_shm_fd();
}

/* Get the translation parameters of the overlay clock */
int TimelineCore::get_overlay_translation_params(tl_translation_t &params)
{
    if (!tl_overlay_clock)
        return -1;
    // Copy over the parameters if the pointer is valid
    params = tl_overlay_clock->get_translation_params();
    return 0;
}

// Update the Global Node Info from the coordination service
qot_return_t TimelineCore::update_global_coordination_info(std::vector<qot_node_phy_t> &node_vector)
{
    // Print the nodes obtained
    std::vector<qot_node_phy_t>::iterator it;
    std::cout << "TimelineCore: Changes in global nodes on timelines\n";
    for(it = node_vector.begin(); it != node_vector.end(); it++)
    {
        std::cout << " Node (name: " << it->name << ", acc_ns: " << it->accuracy_ns << ", res_ns: " << it->resolution_ns << ")\n";
    } 

    return QOT_RETURN_TYPE_OK;
}

// Update the Local Node Info from the coordination service
qot_return_t TimelineCore::update_local_coordination_info(std::vector<qot_node_phy_t> &node_vector)
{
    // Print the nodes obtained
    std::vector<qot_node_phy_t>::iterator it;
    std::cout << "TimelineCore: Changes in local nodes on timelines\n";
    for(it = node_vector.begin(); it != node_vector.end(); it++)
    {
        std::cout << " Node (name: " << it->name << ", acc_ns: " << it->accuracy_ns << ", res_ns: " << it->resolution_ns << ")\n";
    } 

    return QOT_RETURN_TYPE_OK;
}

// Update the List of Corresponding peer clients & the overlay clock
qot_return_t TimelineCore::update_local_peers(std::vector<std::string> &node_vector)
{
    std::cout << "TimelineCore: Updating list of peer sync nodes\n";
    peers = node_vector;
    return QOT_RETURN_TYPE_OK;   
}

// Start the Peer Sync (Cluster-Local Clock Syncronization)
qot_return_t TimelineCore::start_peer_sync()
{
    qot_sync_msg_t msg;
    msg.demand = this->get_desired_qot();
    msg.info = timeline_info;

    std::cout << "TimelineCore: Starting peer sync\n";

    // Start the sync client (sync service) for each of the peers
    for (auto it = peers.begin(); it != peers.end(); ++it)
    {
        msg.msgtype = PEER_START;
        msg.data = *it;
        communicator.send_request(msg);
        std::cout << "TimelineCore: Started peer sync for " << *it << "\n";
    }
    return QOT_RETURN_TYPE_OK;
}

// Stop the Peer Sync (Cluster-Local Clock Syncronization)
qot_return_t TimelineCore::stop_peer_sync()
{
    qot_sync_msg_t msg;
    msg.demand = this->get_desired_qot();
    msg.info = timeline_info;

    // Stop the sync client (sync service) for each of the peers
    for (auto it = peers.begin(); it != peers.end(); ++it)
    {
        msg.msgtype = PEER_STOP;
        msg.data = *it;
        communicator.send_request(msg);
    }
    return QOT_RETURN_TYPE_OK;
}

// Get the Timeline Server
int TimelineCore::get_server(qot_server_t &server)
{
    std::vector<qot_server_t> servers = rest_interface.get_timeline_servers(std::string(timeline_info.name));
    // Check if vector is empty
    if (servers.empty())
        return -1;
    else
        server = servers[0];    // For now return first element of the vector
    return 0;
}

// Set the Timeline Server
int TimelineCore::set_server(qot_server_t &server)
{
    return rest_interface.post_timeline_server(std::string(timeline_info.name), server);
}


