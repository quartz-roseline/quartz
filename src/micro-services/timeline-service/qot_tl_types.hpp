/*
 * @file qot_tl_types.hpp
 * @brief Header for Timeline REST API Types
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

#ifndef QOT_TIMELINE_TYPES_HPP
#define QOT_TIMELINE_TYPES_HPP

#include <string>

/* QoT Node type */
typedef struct qot_node_phy {
    std::string name;
    uint64_t accuracy_ns;
    uint64_t resolution_ns;
} qot_node_phy_t;

/* QoT Server Type */
typedef struct qot_server {
	std::string hostname;    	// Server Hostname	
	std::string type;		 	// Server Type (local or global)
	int stratum;			 	// NTP stratum
	uint64_t info_timestamp;	// Timestamp at which this info was valid
	uint64_t root_dispersion;	// Root dispersion from Stratum 1
	uint64_t root_delay;		// Root delay from Startum 1
	int timeline_id;			// Timeline ID
} qot_server_t;

#endif