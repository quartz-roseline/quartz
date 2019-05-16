/**
 * @file qot_sync_service.hpp
 * @brief Header for the Clock Synchronization Service
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
 *
 */

#ifndef QOT_SYNC_SERVICE_HPP
#define QOT_SYNC_SERVICE_HPP

// QoT Datatypes
extern "C"
{
    #include "../../qot_types.h"
}

// Hard coded socket path for communication
#define SYNC_SOCKET_PATH "/tmp/qot_clocksync"

/**
 * @brief QoT Clock Sync Message Types
 */
typedef enum {   
    TL_CREATE_UPDATE  = (0),               /* Create/Update a timeline                 */
    TL_DESTROY        = (1),               /* Destroy a timeline                       */
	PEER_START        = (2),			   /* Start a Peer Synchronization Client      */
	PEER_STOP         = (3),			   /* Stop a Peer Synchronization Client       */
    GLOB_SYNC_UPDATE  = (4),               /* Update the Global NTP sync               */
    SET_NODE_UUID     = (5),               /* Set the node UUID                        */
    TL_UNDEFINED      = (6),               /* Undefined function                       */
} csmsg_type_t;

/**
 * @brief QoT Synchronization Service Messaging format
 */
typedef struct qot_syncmsg {
    qot_timeline_t info;                 /* Timeline Info                            */
    timequality_t demand;                /* Requested QoT                            */
    csmsg_type_t msgtype;                /* Message type                             */
    std::string data;					 /* Auxilliary field to store extra data     */
    qot_return_t retval;                 /* Return Code (Populated by host daemon)   */
} qot_sync_msg_t;

#endif