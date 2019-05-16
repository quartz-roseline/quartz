/*
 * @file qot_synccomm.hpp
 * @brief Implementation of the Interface to communicate with the ClockSynchronization Service  
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


// Internal Sync Communicator Header
#include "qot_synccomm.hpp"
#include "../sync-service/qot_sync_service.hpp"
#include "../sync-service/qot_syncmsg_serialize.hpp"

using namespace qot_core;

/* Private functions */

/* Public functions */

// Constructor -> Connect to the socket 
SyncCommunicator::SyncCommunicator()
 :status_flag(0)
{
    // Initialize the socket connection
    struct sockaddr_un server;
    char buf[1024];

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("opening stream socket to the sync service");
        status_flag = 1;
    }
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, SYNC_SOCKET_PATH);

    if (connect(sock, (struct sockaddr *) &server, sizeof(struct sockaddr_un)) < 0) {
        close(sock);
        perror("connecting stream socket to the sync socket");
        status_flag = 2;
    }
}

// Destructor -> Connect to the socket
SyncCommunicator::~SyncCommunicator()
{
    if (status_flag == 0)
        close(sock);
}

// Send a request to the sync service -> synchronous call
qot_return_t SyncCommunicator::send_request(qot_sync_msg_t &sync_msg)
{
    // Populate sync message
    sync_msg.retval = QOT_RETURN_TYPE_OK;

    // Check if message is valid
    if (sync_msg.msgtype >= TL_UNDEFINED)
        return QOT_RETURN_TYPE_ERR;

    // Serialize Message
    nlohmann::json data = serialize_syncmsg(sync_msg);
    std::string msg_string = data.dump();

    int n = send(sock, msg_string.c_str(), msg_string.length(), 0); 
    if (n > 0)
    {
        const unsigned int MAX_BUF_LENGTH = 4096;
        std::vector<char> buffer(MAX_BUF_LENGTH);
        std::string rcv;
        int n = recv(sock, &buffer[0], buffer.size(), 0);
        if (n > 0)
        {
            /* De-serialize data */
            rcv.append(buffer.cbegin(), buffer.cend());
            nlohmann::json data = nlohmann::json::parse(rcv);
            deserialize_syncmsg(data, sync_msg);
            return sync_msg.retval;
        }
    }
    else
    {
        return QOT_RETURN_TYPE_ERR;
    }

    return QOT_RETURN_TYPE_ERR;
}
