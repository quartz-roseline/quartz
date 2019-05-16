/*
 * @file qot_tlcomm.cpp
 * @brief Implementation of the Interface to communicate with the Timeline Service  
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
#include <sstream>
#include <string>

extern "C"
{
    #include <errno.h> 
    #include <unistd.h>   //close 
    #include <sys/types.h> 
    #include <sys/socket.h> 
    #include <sys/un.h> 
    #include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros 
    #include <fcntl.h>
    #include <signal.h>   // SIGINT
    #include <poll.h>
    #include <pthread.h>
    #include <sys/shm.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
}

// To get the messaging data structure
#include "../../timeline-service/qot_timeline_service.hpp"

// Timeline Message Serialization
#include "../../timeline-service/qot_tlmsg_serialize.hpp"

// Internal Timeline-service Communicator Header
#include "qot_tlcomm.hpp"

#define DEBUG 0

using namespace qot;

/* Private functions */

/* Public functions */

// Constructor -> Connect to the socket 
TLCommunicator::TLCommunicator()
 :status_flag(0)
{
    // Initialize the socket connection
    struct sockaddr_un server;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("opening stream socket to the sync service");
        status_flag = 1;
    }
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, TL_SOCKET_PATH);

    if (connect(sock, (struct sockaddr *) &server, sizeof(struct sockaddr_un)) < 0) {
        close(sock);
        perror("connecting stream socket to the sync socket");
        status_flag = 2;
    }
}

// Destructor -> Connect to the socket
TLCommunicator::~TLCommunicator()
{
    if (status_flag == 0)
        close(sock);
}

// Send a request to the timeline service -> synchronous call
tl_translation_t* TLCommunicator::request_clk_memory(int timeline_id)
{
    qot_timeline_msg_t msg;

    // Populate message information
    msg.msgtype = TIMELINE_SHM_CLKSYNC;
    msg.info.index = timeline_id;

    if (DEBUG)
        printf("Requesting shm memory for timeline %d socket is %d\n", msg.info.index, sock);

    // Message Timeline name and binding name (not essential for the timeline service, but prevents JSON module from throwing an exception)
    strcpy(msg.info.name, "invalid");
    strcpy(msg.binding.name, "invalid");

    // Serialize data
    nlohmann::json data = serialize_tlmsg(msg);
    std::string msg_string = data.dump();

    int n = send(sock, msg_string.c_str(), msg_string.length(), 0); 
    if (n > 0)
    {
        // Shared Memory File Descriptor Message Variables
        struct msghdr shmfd_msg;
        int clk_fd;
        int retval;
        void *clk_shm_base;

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
             if (DEBUG)
                printf("The first control structure contains no timeline clock file descriptor\n");
             return NULL;
        }

        memcpy(&clk_fd, CMSG_DATA(cmsg), sizeof(clk_fd));
        
        if (DEBUG)
            printf("Received timeline clock shm descriptor = %d\n", clk_fd);

        // Map the shared memory region into the memory space
        clk_shm_base = mmap(0, sizeof(tl_translation_t), PROT_READ | PROT_WRITE, MAP_SHARED, clk_fd, 0);
        if (clk_shm_base == MAP_FAILED) {
            printf("Shared memory mmap failed: \n");
            clk_shm_base = NULL;
            return NULL;
        }

        if (DEBUG)
            printf("Mapped clock memory into virtual memory space\n");

        return (tl_translation_t*) clk_shm_base;
    }
    else
    {
        return NULL;
    }
}

// Request pointer to timeline overlay clock shared memory from the timeline service
tl_translation_t* TLCommunicator::request_ov_clk_memory(int timeline_id)
{
    qot_timeline_msg_t msg;

    // Populate message information
    msg.msgtype = TIMELINE_OV_SHM_CLKSYNC;
    msg.info.index = timeline_id;

    if (DEBUG)
        printf("Requesting overlay shm memory for timeline %d socket is %d\n", msg.info.index, sock);

    // Message Timeline name and binding name (not essential for the timeline service, but prevents JSON module from throwing an exception)
    strcpy(msg.info.name, "invalid");
    strcpy(msg.binding.name, "invalid");

    // Serialize data
    nlohmann::json data = serialize_tlmsg(msg);
    std::string msg_string = data.dump();

    int n = send(sock, msg_string.c_str(), msg_string.length(), 0); 
    if (n > 0)
    {
        // Shared Memory File Descriptor Message Variables
        struct msghdr shmfd_msg;
        int clk_fd;
        int retval;
        void *clk_shm_base;

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
            printf("Waiting on recvmsg for timeline overlay clock shm file descriptor\n");
        retval = recvmsg(sock, &shmfd_msg, 0);

        if (DEBUG)
            printf("Received %d bytes of shm info\n", retval);

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&shmfd_msg);
        
        if (cmsg == NULL || cmsg->cmsg_type != SCM_RIGHTS) {
             if (DEBUG)
                printf("The first control structure contains no timeline overlay clock file descriptor\n");
             return NULL;
        }

        memcpy(&clk_fd, CMSG_DATA(cmsg), sizeof(clk_fd));
        
        if (DEBUG)
            printf("Received timeline overlay clock shm descriptor = %d\n", clk_fd);

        // Map the shared memory region into the memory space
        clk_shm_base = mmap(0, sizeof(tl_translation_t), PROT_READ | PROT_WRITE, MAP_SHARED, clk_fd, 0);
        if (clk_shm_base == MAP_FAILED) {
            printf("Shared memory mmap failed: \n");
            clk_shm_base = NULL;
            return NULL;
        }

        if (DEBUG)
            printf("Mapped clock overlay memory into virtual memory space\n");

        return (tl_translation_t*) clk_shm_base;
    }
    else
    {
        return NULL;
    }
}

int TLCommunicator::send_message(qot_timeline_msg_t &msg)
{
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
        return msg.retval;
    }

    return msg.retval;
}

// Get the Timeline NTP Server
int TLCommunicator::get_timeline_server(int timeline_id, qot_server_t &server)
{
    qot_timeline_msg_t msg;

    // Populate message information
    msg.msgtype = TIMELINE_GET_SERVER;
    msg.info.index = timeline_id;

    if (DEBUG)
        printf("Requesting overlay shm memory for timeline %d socket is %d\n", msg.info.index, sock);

    // Message Timeline name and binding name (not essential for the timeline service, but prevents JSON module from throwing an exception)
    strcpy(msg.info.name, "invalid");
    strcpy(msg.binding.name, "invalid");

    // Send Message
    if (send_message(msg) != 0)
        return -1;

    // Unroll the message
    std::istringstream iss(msg.aux_data);
    std::string word;
    int ctr = 0;
    while(iss >> word) {
        /* unroll the string */
        if (ctr == 0)
            server.hostname = word;
        else if (ctr == 1)
            server.type = word;
        else if (ctr == 2)
            server.stratum = std::stoi(word);

        ctr++;
    }

    return 0;
}

// Set the Timeline NTP Server
int TLCommunicator::set_timeline_server(int timeline_id, qot_server_t &server)
{
    qot_timeline_msg_t msg;

    // Populate message information
    msg.msgtype = TIMELINE_SET_SERVER;
    msg.info.index = timeline_id;

    if (DEBUG)
        printf("Requesting overlay shm memory for timeline %d socket is %d\n", msg.info.index, sock);

    // Message Timeline name and binding name (not essential for the timeline service, but prevents JSON module from throwing an exception)
    strcpy(msg.info.name, "invalid");
    strcpy(msg.binding.name, "invalid");

    // Set the server info in the required format
    msg.aux_data = server.hostname + " " + server.type + " "  + std::to_string(server.stratum);

    // Send Message
    return send_message(msg);
}