/**
 * @file qot_timeline_service.hpp
 * @brief Header for the Timeline Service
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

#ifndef QOT_TIMELINE_SERVICE_HPP
#define QOT_TIMELINE_SERVICE_HPP

// QoT Datatypes
extern "C"
{
    #include "../../qot_types.h"
}

// Hard coded socket path for communication
#define TL_SOCKET_PATH "/tmp/qot_timeline"

/**
 * @brief QoT Timeline Message Types
 */
typedef enum {   
    TIMELINE_CREATE         = (0),               /* Create a timeline                                */
    TIMELINE_DESTROY        = (1),               /* Destroy a timeline                               */
    TIMELINE_UPDATE         = (2),               /* Update timeline binding parameters               */
    TIMELINE_BIND           = (3),               /* Bind to a timeline                               */
    TIMELINE_UNBIND         = (4),               /* Unbind from a timeline                           */
    TIMELINE_QUALITY        = (5),               /* Get the QoT Spec for this timeline               */
    TIMELINE_INFO           = (6),               /* Get the timeline info                            */
    TIMELINE_SHM_CLOCK      = (7),               /* Get the timeline clock rd-only shm fd            */
    TIMELINE_SHM_CLKSYNC    = (8),               /* Get the timeline clock shm fd                    */
    TIMELINE_OV_SHM_CLOCK   = (9),               /* Get the overlay clock rd-only shm fd             */
    TIMELINE_OV_SHM_CLKSYNC = (10),              /* Get the overlay clock shm fd                     */
    TIMELINE_GET_SERVER     = (11),              /* Get the server for the timeline                  */
    TIMELINE_SET_SERVER     = (12),              /* Set the server for the timeline                  */
    TIMELINE_REQ_LATENCY    = (13),              /* Request for the latency between a pair of nodes  */
    TIMELINE_GET_LATENCY    = (14),              /* Read the latency between a pair of nodes         */
    TIMELINE_UNDEFINED      = (15),              /* Undefined function                               */
} tlmsg_type_t;

/**
 * @brief QoT Timeline Service Messaging format
 */
typedef struct qot_timelinemsg {
    qot_timeline_t info;                 /* Timeline Info                            */
    qot_binding_t binding;               /* Binding Information                      */
    timequality_t demand;                /* Requested QoT                            */
    tlmsg_type_t msgtype;                /* Message type                             */
    qot_return_t retval;                 /* Return Code (Populated by host daemon)   */
    std::string aux_data;                /* Auxiliary Data                           */
} qot_timeline_msg_t;


// Send timeline metadata to the Timeline Service
qot_return_t send_service_message(qot_timeline_msg_t *message);

#endif