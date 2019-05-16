/*
 * @file qot_tlmsg_serialize.cpp
 * @brief Library to serialize messages sent to the timeline service to nlohmann::json
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

/* Only build if the timeline service is being built */
#include <iostream>
#include <string>

/* Include basic types, time math and ioctl interface */
extern "C"
{
	#include "../../qot_types.h"
}

// Header to the timeline service
#include "qot_tlmsg_serialize.hpp"

// Serialize Timeline Service Messages
nlohmann::json serialize_tlmsg(qot_timeline_msg_t &msg)
{
	nlohmann::json j;

	/* Formulate Message as nlohmann::json */
	/* Timeline Info */
	j["info"]["name"] = std::string(msg.info.name);
	j["info"]["index"] = msg.info.index;
	j["info"]["type"] = (int)msg.info.type;      

	/* Binding Information */      
    j["binding"]["name"] = std::string(msg.binding.name);
    j["binding"]["id"] = msg.binding.id; 

    /* Requested QoT */         
    j["demand"]["resolution"]["sec"] = msg.demand.resolution.sec;
    j["demand"]["resolution"]["asec"] = msg.demand.resolution.asec;
    j["demand"]["accuracy"]["above"]["sec"] = msg.demand.accuracy.above.sec;
    j["demand"]["accuracy"]["above"]["asec"] = msg.demand.accuracy.above.asec;
    j["demand"]["accuracy"]["below"]["sec"] = msg.demand.accuracy.below.sec;
    j["demand"]["accuracy"]["below"]["asec"] = msg.demand.accuracy.below.asec;

    /* Message type */             
    j["msgtype"] = (int)msg.msgtype; 

    /* Return Code */               
    j["retval"] = (int)msg.retval;   

    /* Auxilliary Data */
    j["data"] = msg.aux_data;

	return j;
}

// Deserialize Timeline Service Messages
void deserialize_tlmsg(nlohmann::json &data, qot_timeline_msg_t &msg)
{
	/* Note: For now we only de-serialize the essential objects */
	// Get the timeline info
	strcpy(msg.info.name, data["info"]["name"].get<std::string>().c_str());
	msg.info.index = data["info"]["index"].get<int>();
	msg.info.type = (qot_timeline_type_t)data["info"]["type"].get<int>();

	// Get the binding info
	strcpy(msg.binding.name, data["binding"]["name"].get<std::string>().c_str());
	msg.binding.id = data["binding"]["id"].get<int>();

	// Get the QoT demand information
	msg.demand.resolution.sec = data["demand"]["resolution"]["sec"].get<uint64_t>();
    msg.demand.resolution.asec = data["demand"]["resolution"]["asec"].get<uint64_t>();
    msg.demand.accuracy.above.sec = data["demand"]["accuracy"]["above"]["sec"].get<uint64_t>();
    msg.demand.accuracy.above.asec = data["demand"]["accuracy"]["above"]["asec"].get<uint64_t>();
    msg.demand.accuracy.below.sec = data["demand"]["accuracy"]["below"]["sec"].get<uint64_t>();
    msg.demand.accuracy.below.asec = data["demand"]["accuracy"]["below"]["asec"].get<uint64_t>();
	msg.binding.demand = msg.demand;

	// Message type
	msg.msgtype = (tlmsg_type_t)data["msgtype"].get<int>();

	// Return code
	msg.retval = (qot_return_t) data["retval"].get<int>();

	// Get the auxilliary data
	msg.aux_data = data["data"].get<std::string>();

	return;
}



