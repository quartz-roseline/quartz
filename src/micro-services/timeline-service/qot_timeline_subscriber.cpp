/*
 * @file qot_timeline_subscriber.cpp
 * @brief Timeline CoordinationService-Subscriber class implementation
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

#include "qot_timeline_subscriber.hpp"

// For the QoT Nodes data structure
#include "qot_timeline_rest.hpp"

#include "qot_timeline.hpp"

#include <cpprest/json.h>

using namespace qot_core;

/* NATS Subscription handler for global node changes */
void global_node_change_handler(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure)
{
    /* De-serialize data */
    json::value answer = json::value::parse(std::string(natsMsg_GetData(msg)));

    // Get the TimelineCore pointer
    TimelineCore *tl_core = (TimelineCore*)closure;
    std::vector<qot_node_phy_t> node_vector;

    // Parse the JSON
    // Unpack the timelines
	for(auto array_iter = answer.as_object().cbegin(); array_iter != answer.as_object().cend(); ++array_iter)
	{ 
		const utility::string_t &key = array_iter->first;
		const json::value &value = array_iter->second;

		// Extract the name of the node
		qot_node_phy_t new_node;
		new_node.name = key;

		for(auto iter = value.as_object().cbegin(); iter != value.as_object().cend(); ++iter)
		{
			const utility::string_t &inner_key = iter->first;
			const json::value &inner_value = iter->second;

			// Extract the resolution and accuracy
			if (inner_key.compare("accuracy") == 0)
			{
			    new_node.accuracy_ns = inner_value.as_integer();
			}
			else if (inner_key.compare("resolution") == 0)
			{
			    new_node.resolution_ns = inner_value.as_integer();
			}
		}
		// Add the node to the vector
		node_vector.push_back(new_node);
	}

    if (closure != NULL)
    	tl_core->update_global_coordination_info(node_vector);

    // Need to destroy the message!
    natsMsg_Destroy(msg);
}

/* NATS Subscription handler for local node changes */
void local_node_change_handler(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure)
{
    /* De-serialize data */
    json::value answer = json::value::parse(std::string(natsMsg_GetData(msg)));

    // Get the TimelineCore pointer
    TimelineCore *tl_core = (TimelineCore*)closure;
    std::vector<qot_node_phy_t> node_vector;

    // Parse the JSON
    // Unpack the timelines
    for(auto array_iter = answer.as_object().cbegin(); array_iter != answer.as_object().cend(); ++array_iter)
    { 
        const utility::string_t &key = array_iter->first;
        const json::value &value = array_iter->second;

        // Extract the name of the node
        qot_node_phy_t new_node;
        new_node.name = key;

        for(auto iter = value.as_object().cbegin(); iter != value.as_object().cend(); ++iter)
        {
            const utility::string_t &inner_key = iter->first;
            const json::value &inner_value = iter->second;

            // Extract the resolution and accuracy
            if (inner_key.compare("accuracy") == 0)
            {
                new_node.accuracy_ns = inner_value.as_integer();
            }
            else if (inner_key.compare("resolution") == 0)
            {
                new_node.resolution_ns = inner_value.as_integer();
            }
        }
        // Add the node to the vector
        node_vector.push_back(new_node);
    }

    if (closure != NULL)
        tl_core->update_local_coordination_info(node_vector);

    // Need to destroy the message!
    natsMsg_Destroy(msg);
}

// Constructor and Destructor
TimelineSubscriber::TimelineSubscriber(std::string nats_host, std::string timeline_uuid, void *parent)
 : timeline_uuid(timeline_uuid), nats_host(nats_host), parent_class(parent)
{
	// Can be used to initialize the class
	std::cout << "TimelineSubscriber: Initialized for timeline " << timeline_uuid << "\n";
}

TimelineSubscriber::~TimelineSubscriber()
{
	// Can be used to terminate the class
}

/* Subscribe to a NATS topic (subject) */
int TimelineSubscriber::natsSubscribe()
{
    std::string global_topic = "coordination.timelines." + timeline_uuid + ".global";
    std::string local_topic = "coordination.timelines." + timeline_uuid + ".local";

    std::string host = "nats://" + nats_host;

    // Creates a connection to the default NATS URL
    s = natsConnection_ConnectTo(&conn, host.c_str());
    if (s == NATS_OK)
    {
        std::cout << "TimelineSubscriber: Connected to NATS server\n";

        // Creates an asynchronous subscription on the specified topic.
        s = natsConnection_Subscribe(&sub, conn, global_topic.c_str(), global_node_change_handler, parent_class);
    }

    if (s == NATS_OK)
    {
        std::cout << "TimelineSubscriber: Succesfully subscribed to global timeline node topic\n";

        // Creates an asynchronous subscription on the specified topic.
        s = natsConnection_Subscribe(&sub, conn, local_topic.c_str(), local_node_change_handler, parent_class);
    }

    // If there was an error, print a stack trace and exit
    if (s != NATS_OK)
    {
        nats_PrintLastErrorStack(stderr);
        return (int)s;
    }
    else
    {
        std::cout << "TimelineSubscriber: Succesfully subscribed to local timeline node topic\n";
    }

    return NATS_OK;
}

/* Un-subscribe from a NATS topic (subject) */
int TimelineSubscriber::natsUnSubscribe()
{
    // Anything that is created need to be destroyed
    if (s == NATS_OK)
    {
        natsSubscription_Destroy(sub);
        natsConnection_Destroy(conn);
    }
    
    conn = NULL;
    sub  = NULL;
    done  = false;
    return 0;
}

	
