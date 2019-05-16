/*
 * @file qot_coreapi.cpp
 * @brief The Core C++ application programmer interface to the QoT stack
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

/* System includes */
extern "C"
{
    #include <math.h>
    #include <pthread.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <signal.h>
    #include <errno.h>
    #include <poll.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <sys/shm.h>    // Shared Memory
    #include <sys/mman.h>   // Memory Management
    #include <errno.h>      // Error

    #include <linux/ptp_clock.h>
}

#include <iostream>

/* This file includes */
#include "qot_coreapi.hpp"

#ifdef QOT_TIMELINE_SERVICE
// To serialize timeline service messages to JSON
#include "../../micro-services/timeline-service/qot_tlmsg_serialize.hpp"

// To de-serialize timeline clock parameters from JSON
#ifdef NATS_SERVICE
#include "../../micro-services/sync-service/qot_clkparams_serialize.hpp"
#endif

#endif

using namespace qot_coreapi;

#define DEBUG 0

/* Private Functions */

#ifdef QOT_TIMELINE_SERVICE
#ifdef NATS_SERVICE
/* NATS Subscription handler*/
void timeline_param_handler(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure)
{
    CircularBuffer *param_buffer = (CircularBuffer*) closure;

    if (DEBUG)
    {
        printf("Received msg: %s - %.*s\n",
               natsMsg_GetSubject(msg),
               natsMsg_GetDataLength(msg),
               natsMsg_GetData(msg));
    }

    /* De-serialize data */
    tl_translation_t rcv_clk_params;
    nlohmann::json data = nlohmann::json::parse(std::string(natsMsg_GetData(msg)));
    deserialize_clkparams(data, rcv_clk_params);

    if (DEBUG)
        printf("Deserialized params are mult = %lld last = %lld\n", (long long)rcv_clk_params.mult, (long long)rcv_clk_params.last);

    if (param_buffer)
        param_buffer->AddElement(rcv_clk_params);

    // Need to destroy the message!
    natsMsg_Destroy(msg);

}

/* Subscribe to a NATS topic (subject) */
int TimelineBinding::natsSubscribe(std::string &topic)
{
    if (DEBUG)
        printf("Subscribing to NATS subject %s\n", topic.c_str());

    // Creates a connection to the default NATS URL
    s = natsConnection_ConnectTo(&conn, "nats://nats.default.svc.cluster.local:4222");
    if (s == NATS_OK)
    {
        nats_status_flag = 1; // Indicates the connection went through
        if (DEBUG)
            printf("Connected to NATS server\n");

        // Create Circular Buffer
        param_buffer = new CircularBuffer(CIRBUFF_DEFSIZE);

        // Creates an asynchronous subscription on subject "foo".
        // When a message is sent on subject "foo", the callback
        // onMsg() will be invoked by the client library.
        // You can pass a closure as the last argument.
        s = natsConnection_Subscribe(&sub, conn, topic.c_str(), timeline_param_handler, (void*) param_buffer);
    }

    // If there was an error, print a stack trace and exit
    if (s != NATS_OK)
    {
        nats_PrintLastErrorStack(stderr);
        return (int)s;
    }
    else
    {
        if (DEBUG)
            printf("Succesfully subscribed to timeline clock parameter topic\n");
    }

    return NATS_OK;
}

/* Un-subscribe to a NATS topic (subject) */
int TimelineBinding::natsUnSubscribe()
{
    // Anything that is created need to be destroyed
    if (s == NATS_OK)
    {
        natsSubscription_Destroy(sub);
        natsConnection_Destroy(conn);
        delete param_buffer;
    }
    else if (s != NATS_OK && nats_status_flag == 1)
    {
        // Connection went through subscription failed
        natsConnection_Destroy(conn);
        delete param_buffer;
    }
    param_buffer = NULL;
    conn = NULL;
    sub  = NULL;
    done  = false;
    return 0;
}

#endif
#endif

/* Is the given timeline a valid one */
qot_return_t TimelineBinding::timeline_check_fd() 
{
    if (fcntl(timeline.fd, F_GETFD)==-1)
        return QOT_RETURN_TYPE_ERR;
    return QOT_RETURN_TYPE_OK;
}

#ifdef QOT_TIMELINE_SERVICE
/* Send a message to the socket */
qot_return_t TimelineBinding::send_message(qot_timeline_msg_t &msg)
{
    /* Add dummy aux_data for now, this is a hack */
    msg.aux_data = std::string("NULL");
    /* Serialize Message */
    nlohmann::json data = serialize_tlmsg(msg);
    std::string msg_string = data.dump();

    int bytesSent = send(sock, msg_string.c_str() , msg_string.length(), 0); 
    if ((msg.msgtype != TIMELINE_SHM_CLOCK && msg.msgtype != TIMELINE_OV_SHM_CLOCK) && bytesSent > 0)
    {
        const unsigned int MAX_BUF_LENGTH = 4096;
        std::vector<char> buffer(MAX_BUF_LENGTH);
        std::string rcv;   
        int bytesReceived = 0;
        int recv_flag = 0;
        do {
            bytesReceived = recv(sock, &buffer[0], buffer.size(), 0);
            // Append string from buffer.
            if (bytesReceived == -1 && recv_flag == 0) { 
                return QOT_RETURN_TYPE_ERR;
            } else {
                if (DEBUG)
                    printf("Received %d bytes from service\n", bytesReceived);
                rcv.append( buffer.cbegin(), buffer.cend() );
                recv_flag = 1;
            }
        } while ( bytesReceived == MAX_BUF_LENGTH );
        /* De-serialize data */
        data = nlohmann::json::parse(rcv);
        deserialize_tlmsg(data, msg);
        return msg.retval;
    }
    else
    {
        msg.retval = QOT_RETURN_TYPE_OK;
        return QOT_RETURN_TYPE_OK;
    }

    return QOT_RETURN_TYPE_ERR;
}

/* Convert from core time to timeline time */
qot_return_t TimelineBinding::qot_loc2rem(utimepoint_t &est, int period, int instant_flag)
{    
    int64_t val;

    // Search the circular-buffer cache for the appropriate parameters(if instant_flag not set)
    #ifdef NATS_SERVICE
    if (instant_flag == 0)
    {
        tl_translation_t params;
        if(param_buffer->FindParams(est.estimate, params) == 0)
        {
            // Params found
            val = TP_TO_nSEC(est.estimate);

            /* Calculate sync uncertainty */
            int64_t u_bound = (params.u_mult*(val - params.last))/1000000000L + params.u_nsec;
            int64_t l_bound = (params.l_mult*(val - params.last))/1000000000L + params.l_nsec;

            /* Write the uncertainty */
            TL_FROM_nSEC(est.interval.above, (unsigned long long)u_bound);
            TL_FROM_nSEC(est.interval.below, (unsigned long long)l_bound);

            // Check if this is correct -> makes the assumption that val is mostly greater than 1s (1 billion ns) (consider using floating point ops)
            if (period)
                val += (params.mult*val)/1000000000L;
            else
            {
                val -= params.last;
                val  = params.nsec + val + ((params.mult*val)/1000000000L);
            }
            TP_FROM_nSEC(est.estimate, val); 
            return QOT_RETURN_TYPE_OK;
        }
        else
        {
            return QOT_RETURN_TYPE_ERR;
        }
    }
    #endif

    // If instantaneous flag (get time instantaneously) {if NATS service doe not exist this is the default behaviour}
    if (!tl_clk_params)
        return QOT_RETURN_TYPE_ERR;

    val = TP_TO_nSEC(est.estimate);

    // Check if this is correct -> makes the assumption that val is mostly greater than 1s (1 billion ns) (consider using floating point ops)
    if (period)
        val += (tl_clk_params->mult*val)/1000000000L;
    else
    {
        val -= tl_clk_params->last;
        val  = tl_clk_params->nsec + val + ((tl_clk_params->mult*val)/1000000000L);
    }

    // Check if an overlay map exists (indicates local timeline) -> Apply projections
    if (tl_ov_clk_params)
    {
        if (period)
        {
            val += tl_ov_clk_params->mult*(val/1000000000L); // different formula for overlay clock
            //val += int64_t(tl_ov_clk_params->slope*val);
        }
        else
        {
            val -= tl_ov_clk_params->last;
            val  = tl_ov_clk_params->nsec + val + tl_ov_clk_params->mult*(val/1000000000L); // different formula for overlay clock
            //val += tl_ov_clk_params->nsec + int64_t(tl_ov_clk_params->slope*val);
        }
    }

    // Convert to timepoint
    TP_FROM_nSEC(est.estimate, val); 
    
    return QOT_RETURN_TYPE_OK;
}

/* Convert from timeline time to core time */
qot_return_t TimelineBinding::qot_rem2loc(utimepoint_t &est, int period)
{
    int64_t val;
    int64_t rem;

    if(!tl_clk_params)
        return QOT_RETURN_TYPE_ERR;

    val = TP_TO_nSEC(est.estimate);

    // Check if an overlay map exists (indicates local timeline) -> Apply projections
    if (tl_ov_clk_params)
    {
        if (period)
        {
            val = (int64_t)floor(((double)val/(double)(tl_ov_clk_params->mult + 1000000000LL))*1000000000LL);
            //val = (int64_t)floor((double)val/(double)(tl_ov_clk_params->slope + 1));
        }
        else
        {
            int64_t diff = (val - tl_ov_clk_params->nsec);
            int64_t quot = (int64_t)floor(((double)diff/(double)(tl_ov_clk_params->mult + 1000000000LL))*1000000000LL);
            val = tl_ov_clk_params->last + quot; 
            //val = (int64_t)floor((double)diff/(double)(tl_ov_clk_params->slope + 1));
        }
        // printf("Overlay Clock Parameters %lld %lld %lld\n", (long long)tl_ov_clk_params->mult, (long long)tl_ov_clk_params->last, (long long)tl_ov_clk_params->nsec);
        // printf("Main Clock Parameters    %lld %lld %lld\n", (long long)tl_clk_params->mult, (long long)tl_clk_params->last, (long long)tl_clk_params->nsec);
    }

    if (period)
    {
        val = (int64_t)floor(((double)val/(double)(tl_clk_params->mult + 1000000000LL))*1000000000LL);
    }
    else
    {
        int64_t diff = (val - tl_clk_params->nsec);
        int64_t quot = (int64_t)floor(((double)diff/(double)(tl_clk_params->mult + 1000000000LL))*1000000000LL);
        val = tl_clk_params->last + quot; 
    }

    // Convert to timepoint
    TP_FROM_nSEC(est.estimate, val); 

    return QOT_RETURN_TYPE_OK;
}

/* Private implementation function to compute the timestamp uncertainty */
qot_return_t TimelineBinding::timeline_computeqot(utimepoint_t &est)
{
    long long coretime;
    long long u_bound;
    long long l_bound;

    coretime = TP_TO_nSEC(est.estimate);
    /* Calculate sync uncertainty */
    u_bound = (tl_clk_params->u_mult*(coretime - tl_clk_params->last))/1000000000L + tl_clk_params->u_nsec;
    l_bound = (tl_clk_params->l_mult*(coretime - tl_clk_params->last))/1000000000L + tl_clk_params->l_nsec;

    // Check if an overlay map exists (indicates local timeline) -> Apply projections
    if (tl_ov_clk_params)
    {
        // This formula may be incorrect
        u_bound = u_bound + (tl_ov_clk_params->u_mult*(coretime + u_bound - tl_ov_clk_params->last))/1000000000L + tl_ov_clk_params->u_nsec;
        l_bound = l_bound + (tl_ov_clk_params->l_mult*(coretime - l_bound - tl_ov_clk_params->last))/1000000000L + tl_ov_clk_params->l_nsec;
    }

    if (DEBUG)
    {
        printf("Uncertainty Values\n");
        printf("Upper Bound %lld %lld\n", (long long)u_bound, (long long)tl_clk_params->u_nsec);
        printf("Lower Bound %lld %lld\n", (long long)l_bound, (long long)tl_clk_params->l_nsec);
    }

    /* Write the uncertainty */
    TL_FROM_nSEC(est.interval.above, (unsigned long long)u_bound);
    TL_FROM_nSEC(est.interval.below, (unsigned long long)l_bound);

    return QOT_RETURN_TYPE_OK;
}

/* Private implementation function to compute the current timeline time */
qot_return_t TimelineBinding::timeline_getvtime(utimepoint_t &est)
{
    // This function should be populated (use timeline->timeline_clock to translate from core time): URGENT
    qot_return_t retval;
    timeline_getcoretime(est);
    if (DEBUG)
    {
        printf("Reading time using shared memory\n");
        printf("Timeline Parameters are mult:%lld last:%lld\n", 
            (long long)tl_clk_params->mult, 
            (long long)tl_clk_params->last);
    }
    retval = timeline_computeqot(est);
    retval = qot_loc2rem(est, 0, 1);
    return retval;
}
#endif

/* Public Functions */

/* Constructor Default -> tries in an infinite loop to connect to the socket */
TimelineBinding::TimelineBinding()
 : status_flag(0)
{
    #ifdef QOT_TIMELINE_SERVICE
    // Initialize the socket connection
    struct sockaddr_un server;
    char buf[1024];

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("opening stream socket");
        status_flag = 1;
    }
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, TL_SOCKET_PATH);

    while (connect(sock, (struct sockaddr *) &server, sizeof(struct sockaddr_un)) < 0) {
        perror("error connecting stream socket, trying again");
        sleep(2);
    }

    tl_clk_params = NULL;
    tl_ov_clk_params = NULL;

    #ifdef NATS_SERVICE
    param_buffer = NULL;
    nats_status_flag = 0;
    #endif

    #endif
}

/* Constructor with timeout specified */
TimelineBinding::TimelineBinding(int timeout_seconds)
 : status_flag(0)
{
    #ifdef QOT_TIMELINE_SERVICE
    // Initialize the socket connection
    struct sockaddr_un server;
    char buf[1024];

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("opening stream socket");
        status_flag = 1;
    }
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, TL_SOCKET_PATH);

    if (connect(sock, (struct sockaddr *) &server, sizeof(struct sockaddr_un)) < 0) {
        perror("connecting to stream socket, waiting untill timeout to try again");
        sleep(timeout_seconds);
        if (connect(sock, (struct sockaddr *) &server, sizeof(struct sockaddr_un)) < 0)
        {
            close(sock);
            perror("connecting stream socket");
            status_flag = 2;
        }
    }

    tl_clk_params = NULL;
    tl_ov_clk_params = NULL;

    #ifdef NATS_SERVICE
    param_buffer = NULL;
    nats_status_flag = 0;
    #endif

    #endif
}

/* Destructor */
TimelineBinding::~TimelineBinding()
{
    #ifdef QOT_TIMELINE_SERVICE
    if (status_flag == 0)
        close(sock);
    #endif
}

/* Bind to a timeline */
qot_return_t TimelineBinding::timeline_bind(const std::string uuid, const std::string name, timelength_t res, timeinterval_t acc)
{
    char qot_timeline_filename[15];
    int usr_file;

    if (strlen(uuid.c_str()) > QOT_MAX_NAMELEN)
        return QOT_RETURN_TYPE_ERR;

    // Check to find whether the timeline type is global (first three characters should be "gl_")
    std::size_t found_pos = uuid.find(std::string(GLOBAL_TL_STRING));
    if (found_pos == 0)
    {
        timeline.info.type = QOT_TIMELINE_GLOBAL;
        if (DEBUG) 
            printf("Global Timeline detected\n");
    }
    else
    {
        timeline.info.type = QOT_TIMELINE_LOCAL; // Default to Local
        if (DEBUG) 
            printf("Local Timeline detected\n");
    }

    // Copy timeline UUID to the timeline data structure
    if (uuid.length() <= QOT_MAX_NAMELEN)
        strcpy(timeline.info.name, uuid.c_str());
    else
        return QOT_RETURN_TYPE_ERR;

    timeline.info.index = 0;
    // Populate Binding fields
    strcpy(timeline.binding.name, name.c_str());
    timeline.binding.demand.resolution = res;
    timeline.binding.demand.accuracy = acc;
    timeline.binding.id = -1;
    TL_FROM_SEC(timeline.binding.period, 0);
    TP_FROM_SEC(timeline.binding.start_offset, 0);

    #ifdef QOT_TIMELINE_SERVICE

    // Create a timeline
    qot_timeline_msg_t tl_msg;
    tl_msg.info = timeline.info;
    tl_msg.msgtype = TIMELINE_CREATE;
    tl_msg.demand = timeline.binding.demand;
    tl_msg.binding = timeline.binding;
    tl_msg.retval = QOT_RETURN_TYPE_ERR;
    if (DEBUG) 
        printf("Sending timeline metadata to host\n");
    if(send_message(tl_msg) == QOT_RETURN_TYPE_ERR)
    {
        if (DEBUG) 
            printf("Failed to send timeline metadata to timeline service\n");
        return QOT_RETURN_TYPE_ERR;
    }
    else
    {
        if (DEBUG) 
            printf("Service replied with %d retval, timeline id is %d\n", tl_msg.retval, tl_msg.info.index);
        timeline.info = tl_msg.info;
    }
    #else
    // Open the QoT Core
    if (DEBUG) 
        printf("Opening IOCTL to qot_core\n");
    usr_file = open("/dev/qotusr", O_RDWR);
    if (DEBUG)
        printf("IOCTL to qot_core opened %d\n", usr_file);

    if (usr_file < 0)
    {
        printf("Error: Invalid file\n");
        return QOT_RETURN_TYPE_ERR;
    }

    timeline.qotusr_fd = usr_file;
    #endif
    
    // Bind to the timeline
    if (DEBUG) 
        printf("Binding to timeline %s\n", uuid.c_str());

    strcpy(timeline.info.name, uuid.c_str());  

    #ifdef QOT_TIMELINE_SERVICE
    /* Get the main clock shared memory descriptor */
    // Shared Memory File Descriptor Message Variables
    struct msghdr shmfd_msg;
    int clk_fd;
    int retval;
    void *clk_shm_base;

    // Send the message to get the timeline clock parameters
    tl_msg.info = timeline.info;
    tl_msg.msgtype = TIMELINE_SHM_CLOCK;
    tl_msg.retval = QOT_RETURN_TYPE_ERR;
    if (DEBUG) 
        printf("Requesting clock shm parameters from service\n");
    if(send_message(tl_msg) == QOT_RETURN_TYPE_ERR)
    {
        if (DEBUG) 
            printf("Failed to send clock shm fd request to timeline service\n");
        return QOT_RETURN_TYPE_ERR;
    }
    else
    {
        if (DEBUG) 
            printf("Service replied with %d retval\n", tl_msg.retval);
    }
    
    // Get the file descriptor for the clock shared memory
    struct iovec iov[1];
    char data[1];
    char cmsgbuf[CMSG_SPACE(sizeof(int))];

    memset(&shmfd_msg,   0, sizeof(shmfd_msg));
    memset(cmsgbuf, 0, CMSG_SPACE(sizeof(int)));

    shmfd_msg.msg_control = cmsgbuf; // make place for the ancillary message to be received
    shmfd_msg.msg_controllen = CMSG_SPACE(sizeof(int));

    data[0] = ' ';
    iov[0].iov_base = data;
    iov[0].iov_len = sizeof(data);

    shmfd_msg.msg_name = NULL;
    shmfd_msg.msg_namelen = 0;
    shmfd_msg.msg_iov = iov;
    shmfd_msg.msg_iovlen = 1;
    
    if (DEBUG)
        printf("Waiting on recvmsg for timeline clock shm file descriptor\n");
    retval = recvmsg(sock, &shmfd_msg, 0);

    if (DEBUG)
        printf("Received %d bytes of shm info\n", retval);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&shmfd_msg);
    
    if (cmsg == NULL || cmsg->cmsg_type != SCM_RIGHTS) {
         printf("The first control structure contains no timeline clock file descriptor\n");
         return QOT_RETURN_TYPE_ERR;
    }

    memcpy(&clk_fd, CMSG_DATA(cmsg), sizeof(clk_fd));
    
    if (DEBUG)
        printf("Received timeline clock shm descriptor = %d\n", clk_fd);

    // Map the shared memory region into the memory space
    clk_shm_base = mmap(0, sizeof(tl_translation_t), PROT_READ, MAP_SHARED, clk_fd, 0);
    if (clk_shm_base == MAP_FAILED) {
        printf("Shared memory mmap failed: \n");
        clk_shm_base = NULL;
        return QOT_RETURN_TYPE_ERR;
    }

    if (DEBUG)
        printf("Mapped clock memory into virtual memory space\n");

    // Write the memory pointer containing the clock parameters
    tl_clk_params = (tl_translation_t*) clk_shm_base;

    /* Get the overlay clock shared memory descriptor -> for a local timeline */
    if (timeline.info.type == QOT_TIMELINE_LOCAL)
    {
        // Send the message to get the overlay timeline clock parameters
        tl_msg.info = timeline.info;
        tl_msg.msgtype = TIMELINE_OV_SHM_CLOCK;
        tl_msg.retval = QOT_RETURN_TYPE_ERR;
        if (DEBUG) 
            printf("Requesting overlay clock shm parameters from service\n");
        if(send_message(tl_msg) == QOT_RETURN_TYPE_ERR)
        {
            if (DEBUG) 
                printf("Failed to send overlay clock shm fd request to timeline service\n");
            return QOT_RETURN_TYPE_ERR;
        }
        else
        {
            if (DEBUG) 
                printf("Service replied with %d retval\n", tl_msg.retval);
        }
        
        // Get the file descriptor for the clock shared memory
        memset(&shmfd_msg, 0, sizeof(shmfd_msg));
        memset(cmsgbuf, 0, CMSG_SPACE(sizeof(int)));

        shmfd_msg.msg_control = cmsgbuf; // make place for the ancillary message to be received
        shmfd_msg.msg_controllen = CMSG_SPACE(sizeof(int));

        data[0] = ' ';
        iov[0].iov_base = data;
        iov[0].iov_len = sizeof(data);

        shmfd_msg.msg_name = NULL;
        shmfd_msg.msg_namelen = 0;
        shmfd_msg.msg_iov = iov;
        shmfd_msg.msg_iovlen = 1;
        
        if (DEBUG)
            printf("Waiting on recvmsg for timeline overlay clock shm file descriptor\n");
        retval = recvmsg(sock, &shmfd_msg, 0);

        if (DEBUG)
            printf("Received %d bytes of shm info\n", retval);

        cmsg = CMSG_FIRSTHDR(&shmfd_msg);
        
        if (cmsg == NULL || cmsg->cmsg_type != SCM_RIGHTS) {
             printf("The first control structure contains no timeline clock file descriptor\n");
             return QOT_RETURN_TYPE_ERR;
        }

        memcpy(&clk_fd, CMSG_DATA(cmsg), sizeof(clk_fd));
        
        if (DEBUG)
            printf("Received timeline overlay clock shm descriptor = %d\n", clk_fd);

        // Map the shared memory region into the memory space
        clk_shm_base = mmap(0, sizeof(tl_translation_t), PROT_READ, MAP_SHARED, clk_fd, 0);
        if (clk_shm_base == MAP_FAILED) {
            printf("Shared memory mmap failed: \n");
            clk_shm_base = NULL;
            return QOT_RETURN_TYPE_ERR;
        }

        if (DEBUG)
            printf("Mapped clock overlay memory into virtual memory space\n");

        // Write the memory pointer containing the clock parameters
        tl_ov_clk_params = (tl_translation_t*) clk_shm_base;
    }
    else
    {
        // Set to NULL for global timelines
        tl_ov_clk_params = NULL;
    }

    #else
    // Try to create a new timeline if none exists
    if(ioctl(timeline.qotusr_fd, QOTUSR_CREATE_TIMELINE, &timeline.info) < 0)
    {
        // If it exists try to get information
        if(ioctl(timeline.qotusr_fd, QOTUSR_GET_TIMELINE_INFO, &timeline.info) < 0)
        {
            return QOT_RETURN_TYPE_ERR;
        } 
    }
    // Construct the file handle to the posix clock /dev/timelineX
    sprintf(qot_timeline_filename, "/dev/timeline%d", timeline.info.index);

    // Open the clock
    if (DEBUG) 
        printf("Opening clock %s\n", qot_timeline_filename);
    timeline.fd = open(qot_timeline_filename, O_RDWR);
    if (!timeline.fd)
    {
        printf("Cant open /dev/timeline%d\n", timeline.info.index);
        return QOT_RETURN_TYPE_ERR;
    }
  
    if (DEBUG) 
        printf("Opened clock %s\n", qot_timeline_filename);
    #endif
    
    if (DEBUG) 
        printf("Binding to timeline %s\n", uuid.c_str());

    #ifdef QOT_TIMELINE_SERVICE
    // Bind to the timeline
    tl_msg.info = timeline.info;
    tl_msg.binding = timeline.binding;
    tl_msg.msgtype = TIMELINE_BIND;
    tl_msg.demand = timeline.binding.demand;
    tl_msg.retval = QOT_RETURN_TYPE_ERR;
    if (DEBUG) 
        printf("Sending binding request to host\n");
    if(send_message(tl_msg) == QOT_RETURN_TYPE_ERR)
    {
        if (DEBUG) 
            printf("Failed to send timeline bind to timeline service\n");
        return QOT_RETURN_TYPE_ERR;
    }
    else
    {
        if (DEBUG) 
            printf("Service replied with %d retval, Service binding id is %d\n", tl_msg.retval, tl_msg.binding.id);
        timeline.binding = tl_msg.binding;
    }

    #ifdef NATS_SERVICE
    // Open the connection to the NATS server
    conn = NULL;
    sub  = NULL;
    done  = false;

    // Construct the topic name
    std::string nats_subject = "qot.timeline.";
    nats_subject.append(uuid);
    nats_subject.append(std::string(".params"));

    if (natsSubscribe(nats_subject) != NATS_OK)
    {
        perror("unable to connect to NATS server");
    }

    #endif

    #else
    // Bind to the timeline
    if(ioctl(timeline.fd, TIMELINE_BIND_JOIN, &timeline.binding) < 0)
    {
        return QOT_RETURN_TYPE_ERR;
    }
    #endif
    if (DEBUG) 
        printf("Bound to timeline %s\n", uuid.c_str());

    return QOT_RETURN_TYPE_OK;
}

qot_return_t TimelineBinding::timeline_unbind() 
{
    // Unbind from the timeline
    #ifdef QOT_TIMELINE_SERVICE
    qot_timeline_msg_t tl_msg;
    tl_msg.info = timeline.info;
    tl_msg.binding = timeline.binding;
    tl_msg.msgtype = TIMELINE_UNBIND;
    tl_msg.demand = timeline.binding.demand;
    tl_msg.retval = QOT_RETURN_TYPE_ERR;
    if (DEBUG) 
        printf("Sending unbind command to host for binding id %d\n", timeline.binding.id);
    if(send_message(tl_msg) == QOT_RETURN_TYPE_ERR)
    {
        if (DEBUG) 
            printf("Failed to send timeline unbind to timeline service\n");
        return QOT_RETURN_TYPE_ERR;
    }
    #else
    if(ioctl(timeline.fd, TIMELINE_BIND_LEAVE, &timeline.binding) < 0)
    {
        return QOT_RETURN_TYPE_ERR;
    }

    // Close the timeline file
    if (timeline.fd)
        close(timeline.fd);
    #endif

    // Try to destroy the timeline if possible (will destroy if no other bindings exist)
    #ifdef QOT_TIMELINE_SERVICE
    // Unmap the shared memory locations
    munmap((void*)tl_clk_params, sizeof(tl_translation_t));
    tl_clk_params = NULL;
    if (timeline.info.type == QOT_TIMELINE_LOCAL)
    {
        munmap((void*)tl_ov_clk_params, sizeof(tl_translation_t));
        tl_ov_clk_params = NULL;
    }

    // Send the timeline destroy message
    tl_msg.info = timeline.info;
    tl_msg.binding = timeline.binding;
    tl_msg.msgtype = TIMELINE_DESTROY;
    tl_msg.demand = timeline.binding.demand;
    tl_msg.retval = QOT_RETURN_TYPE_ERR;
    if (DEBUG) 
        printf("Sending timeline destroy command to host\n");
    if(send_message(tl_msg) == QOT_RETURN_TYPE_ERR)
    {
        if (DEBUG) 
            printf("Failed to send timeline destroy to timeline service\n");
        return QOT_RETURN_TYPE_ERR;
    }

    #ifdef NATS_SERVICE
    // Close the connection to the NATS server
    natsUnSubscribe();
    #endif

    #else
    if(ioctl(timeline.qotusr_fd, QOTUSR_DESTROY_TIMELINE, &timeline.info) == 0)
    {
       if(DEBUG)
          printf("Timeline %d destroyed\n", timeline.info.index);
    }
    else
    {
       if(DEBUG)
          printf("Timeline %d not destroyed\n", timeline.info.index);
    }

    if (timeline.qotusr_fd)
       close(timeline.qotusr_fd);

    #endif

    return QOT_RETURN_TYPE_OK;
}


qot_return_t TimelineBinding::timeline_get_accuracy(timeinterval_t& acc) 
{
    acc = timeline.binding.demand.accuracy;
    return QOT_RETURN_TYPE_OK;
}

qot_return_t TimelineBinding::timeline_get_resolution(timelength_t& res) 
{
    res = timeline.binding.demand.resolution;
    return QOT_RETURN_TYPE_OK;
}

qot_return_t TimelineBinding::timeline_get_name(std::string& name) 
{
    name = timeline.binding.name;
    return QOT_RETURN_TYPE_OK;
}

qot_return_t TimelineBinding::timeline_get_uuid(std::string& uuid) 
{
    uuid =timeline.info.name;
    return QOT_RETURN_TYPE_OK;
}

qot_return_t TimelineBinding::timeline_set_accuracy(timeinterval_t& acc) 
{
    #ifndef QOT_TIMELINE_SERVICE
    if (fcntl(timeline.fd, F_GETFD)==-1)
        return QOT_RETURN_TYPE_ERR;
    #endif
    
    timeline.binding.demand.accuracy = acc;

    #ifdef QOT_TIMELINE_SERVICE
    qot_timeline_msg_t tl_msg;
    tl_msg.info = timeline.info;
    tl_msg.binding = timeline.binding;
    tl_msg.msgtype = TIMELINE_UPDATE;
    tl_msg.demand = timeline.binding.demand;
    tl_msg.retval = QOT_RETURN_TYPE_ERR;
    if (DEBUG) 
        printf("Sending accuracy request to host\n");
    if(send_message(tl_msg) == QOT_RETURN_TYPE_ERR)
    {
        if (DEBUG) 
            printf("Failed to send timeline update accuracy to timeline service\n");
        return QOT_RETURN_TYPE_ERR;
    }
    else
    {
        timeline.binding.demand.accuracy = tl_msg.binding.demand.accuracy;
    }
    #else
    // Update the binding
    if(ioctl(timeline.fd, TIMELINE_BIND_UPDATE, &timeline.binding) < 0)
    {
        return QOT_RETURN_TYPE_ERR;
    }
    #endif
    acc = timeline.binding.demand.accuracy;
    return QOT_RETURN_TYPE_OK;
}

qot_return_t TimelineBinding::timeline_set_resolution(timelength_t& res) 
{
    #ifndef QOT_TIMELINE_SERVICE
    if (fcntl(timeline.fd, F_GETFD)==-1)
        return QOT_RETURN_TYPE_ERR;
    #endif

    timeline.binding.demand.resolution = res;
    // Update the binding
    #ifdef QOT_TIMELINE_SERVICE
    qot_timeline_msg_t tl_msg;
    tl_msg.info = timeline.info;
    tl_msg.binding = timeline.binding;
    tl_msg.msgtype = TIMELINE_UPDATE;
    tl_msg.demand = timeline.binding.demand;
    tl_msg.retval = QOT_RETURN_TYPE_ERR;
    if (DEBUG) 
        printf("Sending resolution change request to host\n");
    if(send_message(tl_msg) == QOT_RETURN_TYPE_ERR)
    {
        if (DEBUG) 
            printf("Failed to send resolution update to timeline service\n");
        return QOT_RETURN_TYPE_ERR;
    }
    else
    {
        timeline.binding.demand.resolution = tl_msg.binding.demand.resolution;
    }
    #else
    if(ioctl(timeline.fd, TIMELINE_BIND_UPDATE, &timeline.binding) < 0)
    {
        return QOT_RETURN_TYPE_ERR;
    }
    #endif
    res = timeline.binding.demand.resolution;
    return QOT_RETURN_TYPE_OK;
}

qot_return_t TimelineBinding::timeline_set_schedparams(timelength_t& period, timepoint_t& start_offset) 
{
    #ifndef QOT_TIMELINE_SERVICE
    if (fcntl(timeline.fd, F_GETFD)==-1)
        return QOT_RETURN_TYPE_ERR;
    #endif

    timeline.binding.start_offset = start_offset;
    timeline.binding.period = period;
    #ifndef QOT_TIMELINE_SERVICE
    // Update the binding
    if(ioctl(timeline.fd, TIMELINE_BIND_UPDATE, &timeline.binding) < 0)
    {
        return QOT_RETURN_TYPE_ERR;
    }
    #endif
    return QOT_RETURN_TYPE_OK;
}

qot_return_t TimelineBinding::timeline_getcoretime(utimepoint_t& core_now)
{
    #ifndef QOT_TIMELINE_SERVICE
    if (fcntl(timeline.fd, F_GETFD)==-1)
        return QOT_RETURN_TYPE_ERR;
    #endif
    #ifdef QOT_TIMELINE_SERVICE
    // Read CLOCK_REALTIME 
    struct timespec ts_core_now;
    clock_gettime(CLOCK_REALTIME, &ts_core_now);
    timepoint_from_timespec(&core_now.estimate, &ts_core_now);
    #else
    // Get the core time
    if(ioctl(timeline.fd, TIMELINE_GET_CORE_TIME_NOW, &core_now) < 0)
    {
        return QOT_RETURN_TYPE_ERR;
    }
    #endif
    return QOT_RETURN_TYPE_OK;
}

qot_return_t TimelineBinding::timeline_gettime(utimepoint_t& est) 
{    
    #ifndef QOT_TIMELINE_SERVICE
    if (fcntl(timeline.fd, F_GETFD)==-1)
        return QOT_RETURN_TYPE_ERR;
    #endif

    #ifdef QOT_TIMELINE_SERVICE
    // Read CLOCK_REALTIME and apply projection params
    return timeline_getvtime(est);
    #else 
    // Get the timeline time
    if(ioctl(timeline.fd, TIMELINE_GET_TIME_NOW, &est) < 0)
    {
        return QOT_RETURN_TYPE_ERR;
    }
    #endif
    
    return QOT_RETURN_TYPE_OK;
}

qot_return_t TimelineBinding::timeline_waituntil(utimepoint_t& utp) 
{
    qot_sleeper_t sleeper;
    
    #ifndef QOT_TIMELINE_SERVICE
    if (fcntl(timeline.fd, F_GETFD)==-1)
        return QOT_RETURN_TYPE_ERR;
    #endif

    sleeper.timeline = timeline.info;
    sleeper.wait_until_time = utp;

    if(DEBUG)
        printf("Task invoked wait until secs %lld %llu\n", (long long)utp.estimate.sec, (unsigned long long)utp.estimate.asec);
    
    // Blocking wait on remote timeline time
    #ifdef QOT_TIMELINE_SERVICE
    struct timespec request;
    struct timespec remain;
    
    // Translate and wait on CLOCK_REALTIME -> The translation may change later .. 
    if (qot_rem2loc(utp, 0) == QOT_RETURN_TYPE_ERR)
        return QOT_RETURN_TYPE_ERR;

    timespec_from_timepoint(&request, &utp.estimate);

    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &request, &remain);

    // Add some time estimation logic on wakeup -> TBD

    #else 
    if(ioctl(timeline.qotusr_fd, QOTUSR_WAIT_UNTIL, &sleeper) < 0)
    {
        return QOT_RETURN_TYPE_ERR;
    }
    #endif
    utp = sleeper.wait_until_time;
    return QOT_RETURN_TYPE_OK;
}

qot_return_t TimelineBinding::timeline_waituntil_nextperiod(utimepoint_t& utp) 
{
    qot_sleeper_t sleeper;
    timelength_t elapsed_time;
    timepoint_t wakeup_time;
    u64 elapsed_ns = 0;
    u64 period_ns = 0;
    u64 num_periods = 0;
    
    #ifndef QOT_TIMELINE_SERVICE
    if (fcntl(timeline.fd, F_GETFD)==-1)
        return QOT_RETURN_TYPE_ERR;
    #endif

    sleeper.timeline = timeline.info;

    #ifdef QOT_TIMELINE_SERVICE
    // Read CLOCK_REALTIME and apply projection params
    if (timeline_getvtime(sleeper.wait_until_time) == QOT_RETURN_TYPE_ERR)
        return QOT_RETURN_TYPE_ERR;

    #else 
    // Get the timeline time
    if(ioctl(timeline.fd, TIMELINE_GET_TIME_NOW, &sleeper.wait_until_time) < 0)
    {
        return QOT_RETURN_TYPE_ERR;
    }
    #endif
    // Check Start Offset
    if(timepoint_cmp(&timeline.binding.start_offset, &sleeper.wait_until_time.estimate) < 0)
    {
        sleeper.wait_until_time.estimate = timeline.binding.start_offset;
    }
    else 
    {
        // Calculate Next Wakeup Time
        timepoint_diff(&elapsed_time, &sleeper.wait_until_time.estimate, &timeline.binding.start_offset);
        elapsed_ns = TL_TO_nSEC(elapsed_time);
        period_ns = TL_TO_nSEC(timeline.binding.period);
        num_periods = (elapsed_ns/period_ns);
        if(elapsed_ns % period_ns != 0)
            num_periods++;
        elapsed_ns = period_ns*num_periods;
        TL_FROM_nSEC(elapsed_time, elapsed_ns);
        wakeup_time = timeline.binding.start_offset;
        timepoint_add(&wakeup_time, &elapsed_time);
        sleeper.wait_until_time.estimate = wakeup_time;
     }

    // Blocking wait on remote timeline time
    #ifdef QOT_TIMELINE_SERVICE
    struct timespec request;
    struct timespec remain;
    // Translate and wait on CLOCK_REALTIME -> The translation may change later .. 
    if (qot_rem2loc(sleeper.wait_until_time, 0) == QOT_RETURN_TYPE_ERR)
        return QOT_RETURN_TYPE_ERR;

    timespec_from_timepoint(&request, &sleeper.wait_until_time.estimate);

    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &request, &remain);

    // Add some time estimation logic on wakeup -> TBD

    #else 
    if(ioctl(timeline.qotusr_fd, QOTUSR_WAIT_UNTIL, &sleeper) < 0)
    {
        return QOT_RETURN_TYPE_ERR;
    }
    #endif
    utp = sleeper.wait_until_time;
    return QOT_RETURN_TYPE_OK;
}

qot_return_t TimelineBinding::timeline_sleep(utimelength_t& utl) 
{
    qot_sleeper_t sleeper;

    #ifndef QOT_TIMELINE_SERVICE
    if (fcntl(timeline.fd, F_GETFD)==-1)
        return QOT_RETURN_TYPE_ERR;
    #endif

    // Get the timeline time
    #ifdef QOT_TIMELINE_SERVICE
    // Read CLOCK_REALTIME and apply projection params
    if (timeline_getvtime(sleeper.wait_until_time) == QOT_RETURN_TYPE_ERR)
        return QOT_RETURN_TYPE_ERR;
    #else 
    if(ioctl(timeline.fd, TIMELINE_GET_TIME_NOW, &sleeper.wait_until_time) < 0)
    {
        return QOT_RETURN_TYPE_ERR;
    }
    #endif

    // Convert timelength to a timepoint
    sleeper.wait_until_time.interval =  utl.interval;
    timepoint_add(&sleeper.wait_until_time.estimate, &utl.estimate);
    
    // Blocking wait on remote timeline time
    #ifdef QOT_TIMELINE_SERVICE
    struct timespec request;
    struct timespec remain;
    // Translate and wait on CLOCK_REALTIME -> The translation may change later .. 
    if (qot_rem2loc(sleeper.wait_until_time, 1) == QOT_RETURN_TYPE_ERR)
        return QOT_RETURN_TYPE_ERR;

    timespec_from_timepoint(&request, &sleeper.wait_until_time.estimate);

    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &request, &remain);

    // Add some time estimation logic on wakeup -> TBD

    #else 
    if(ioctl(timeline.qotusr_fd, QOTUSR_WAIT_UNTIL, &sleeper) < 0)
    {
        return QOT_RETURN_TYPE_ERR;
    }
    #endif

    return QOT_RETURN_TYPE_OK;
}

qot_return_t TimelineBinding::timeline_timer_create(qot_timer_t& timer, qot_timer_callback_t callback) 
{
    struct sigaction act;

    #ifndef QOT_TIMELINE_SERVICE
    if (fcntl(timeline.fd, F_GETFD)==-1)
        return QOT_RETURN_TYPE_ERR;
    #endif

    // Create a timer
    #ifdef QOT_TIMELINE_SERVICE
    // TBD ...
    return QOT_RETURN_TYPE_ERR;
    #else 
    if(ioctl(timeline.fd, TIMELINE_CREATE_TIMER, &timer) < 0)
    {
        printf("Failed To Create Timer\n");
        return QOT_RETURN_TYPE_ERR;
    }
    #endif

    memset(&act, 0, sizeof(struct sigaction));
    sigemptyset(&act.sa_mask);

    act.sa_sigaction = callback;
    act.sa_flags = SA_SIGINFO;

    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        printf("sigaction failed !\n");
        return QOT_RETURN_TYPE_ERR;
    }

    return QOT_RETURN_TYPE_OK;
    
}

qot_return_t TimelineBinding::timeline_timer_cancel(qot_timer_t& timer) 
{
    #ifndef QOT_TIMELINE_SERVICE
    if (fcntl(timeline.fd, F_GETFD)==-1)
        return QOT_RETURN_TYPE_ERR;
    #endif

    // Create a timer
    #ifdef QOT_TIMELINE_SERVICE
    // TBD ...
    return QOT_RETURN_TYPE_ERR;
    #else 
    if(ioctl(timeline.fd, TIMELINE_DESTROY_TIMER, &timer) < 0)
    {
        return QOT_RETURN_TYPE_ERR;
    }
    #endif

    return QOT_RETURN_TYPE_OK;
}

qot_return_t TimelineBinding::timeline_core2rem(timepoint_t& est) 
{    
    #ifndef QOT_TIMELINE_SERVICE
    if (fcntl(timeline.fd, F_GETFD)==-1)
        return QOT_RETURN_TYPE_ERR;
    #endif
    
    // Get the timeline time
    #ifdef QOT_TIMELINE_SERVICE
    qot_return_t retval;
    utimepoint_t utp;
    utp.estimate = est;
    retval = qot_loc2rem(utp, 0, 0);
    est = utp.estimate; 
    #else 
    if(ioctl(timeline.fd, TIMELINE_CORE_TO_REMOTE, &est) < 0)
    {
        return QOT_RETURN_TYPE_ERR;
    }
    #endif
    
    return QOT_RETURN_TYPE_OK;
}

qot_return_t TimelineBinding::timeline_rem2core(timepoint_t& est) 
{    
    #ifndef QOT_TIMELINE_SERVICE
    if (fcntl(timeline.fd, F_GETFD)==-1)
        return QOT_RETURN_TYPE_ERR;
    #endif
    
    // Get the timeline time
    #ifdef QOT_TIMELINE_SERVICE
    qot_return_t retval;
    utimepoint_t utp;
    utp.estimate = est;
    retval = qot_rem2loc(utp, 0);
    est = utp.estimate; 
    #else 
    if(ioctl(timeline.fd, TIMELINE_REMOTE_TO_CORE, &est) < 0)
    {
        return QOT_RETURN_TYPE_ERR;
    }
    #endif
    
    return QOT_RETURN_TYPE_OK;
}