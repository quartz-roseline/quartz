/**
 * @file qot_timeline_service.cpp
 * @brief UNIX Socket-based Timeline-Management Server 
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

// C++ Standard Library Headers
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

extern "C"
{
    #include <stdio.h> 
    #include <string.h>   //strlen 
    #include <stdlib.h> 
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

// Timeline Headers
#include "qot_timeline.hpp"
#include "qot_timeline_registry.hpp"
#include "qot_timeline_service.hpp"

// Include the QoT Data Types
extern "C"
{
    #include "../../qot_types.h"
}

// Add header to Modern JSON C++ Library
#include "../../../thirdparty/json-modern-cpp/json.hpp"

// Sync Service Header
#include "../sync-service/qot_sync_service.hpp"

// Add header to JSON Serializing functions
#include "qot_tlmsg_serialize.hpp"

// JSON C++ namespace
using json = nlohmann::json;

using namespace qot_core;

// Maximum Clients
#define MAX_CLIENTS 30

// Select Timeout (seconds)
#define TIMEOUT 5

// Default Node Unique name
#define NODE_UUID "test_node"

// Default NATS Server
#define NATS_SERVER "localhost:4222"

// Default REST Server
#define REST_SERVER "http://localhost:8502"

// Flag to start a default local timeline
//#define QOT_DEF_LOCAL_TL 1

// Running Flag
static int running = 1;

// Exit Handler to terminate the program on Ctrl+C
static void exit_handler(int s)
{
    std::cout << "Exit requested " << std::endl;
    running = 0;
}

// Catch Signal Handler functio
static void sigpipe_handler(int signum){

    printf("Caught signal SIGPIPE %d\n",signum);
}

int send_fd(int sock, int fd)
{
    // This function does the arcane magic for sending
    // file descriptors over unix domain sockets
    struct msghdr msg;
    struct iovec iov[1];
    struct cmsghdr *cmsg = NULL;
    char ctrl_buf[CMSG_SPACE(sizeof(int))];
    char data[1];

    memset(&msg, 0, sizeof(struct msghdr));
    memset(ctrl_buf, 0, CMSG_SPACE(sizeof(int)));

    data[0] = ' ';
    iov[0].iov_base = data;
    iov[0].iov_len = sizeof(data);

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_controllen =  CMSG_SPACE(sizeof(int));
    msg.msg_control = ctrl_buf;

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));

    *((int *) CMSG_DATA(cmsg)) = fd;

    return sendmsg(sock, &msg, 0);
}

/* Function which gets the relevant peer clients from the configuration */
std::vector<std::string> GetPeerClients(nlohmann::json &cluster_config_data, std::string &node_name)
{
    std::vector<std::string> peer_clients;
    int counter = 0;
    for (auto it = cluster_config_data["edges"].begin(); it != cluster_config_data["edges"].end(); ++it)
    {   
        counter = 0;
        // Iterate over inner array (edge)
        for (auto col = it->begin(); col != it->end(); col++) 
        {
            // do stuff ...
            if (counter == 0)
            {
                if ((*col).get<std::string>().compare(node_name) != 0)
                {
                    break;
                }
            }
            else 
            {
                // Start a peer client
                std::cout << "QoTTimelineService: Found a peer client in the cofig for " << (*col) << "\n";
                peer_clients.push_back(*col);
            }
            counter++;
        }
    }
    return peer_clients;
}

/* Timeline Service Main Function */
int main(int argc , char *argv[])  
{  
    int opt = 1; // TRUE 
    int master_socket, addrlen, new_socket, client_socket[MAX_CLIENTS], max_clients = MAX_CLIENTS , activity, i , valread , sd;  
    int max_sd;  

    // Get the node uuid
    std::string node_uuid = NODE_UUID;
    if (argc > 1)
        node_uuid = std::string(argv[1]);

    // Get the NATS Server 
    std::string pub_server = NATS_SERVER;
    if (argc > 2)
        pub_server = std::string(argv[2]);

    // Get the NATS Server 
    std::string rest_server = REST_SERVER;
    if (argc > 3)
        rest_server = std::string(argv[3]);

    // Get the Config file for the Peer2Peer Sync
    std::string peer_file = "NULL";
    int peer_flag = 0; // Flag indicating peer sync is being used
    if (argc > 4)
    {
        peer_file = std::string(argv[4]);
        peer_flag = 1;
    }

    // Pointer to a timeline
    TimelineCore *tl_ptr;

    // Consructor status flag
    int status_flag = 0;

    // Socket Address
    struct sockaddr_un address;
        
    // Set of socket descriptors 
    fd_set readfds; 

    // Timeval for select timeout
    struct timeval timeout = {TIMEOUT,0};

    // Shared Memory File Descriptor Message Variables
    int clk_fd;
    int n_bytes;

    // QoT Virtualization Message
    qot_timeline_msg_t tl_msg; 

    // Read the cluster configuration file
    int cluster_config_valid = 0;
    nlohmann::json cluster_config_data;
    std::vector<std::string> peer_clients;

    if (peer_file.compare(std::string("NULL")) != 0)
    {
        std::ifstream cluster_config(peer_file);
        if (cluster_config.is_open())
        {
            cluster_config_data = nlohmann::json::parse(cluster_config);
            cluster_config_valid = 1;
            std::cout << "Cluster config file read succesfully" << "\n";
            std::cout << "nodes: " << cluster_config_data["nodes"] << "\n";
            std::cout << "edges: " << cluster_config_data["edges"] << "\n";
            cluster_config.close();
            peer_clients = GetPeerClients(cluster_config_data, node_uuid);
        }
        else
        {
            std::cout << "Unable to open cluster config file " << peer_file;
            peer_flag = 0;
        }
    }

    // Checking to see if the sync service is up 
    std::cout << "Waiting for QoT Sync service to come up ....\n";
    struct sockaddr_un sync_server;
    int sync_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sync_sock < 0) {
        perror("opening stream socket to sync service failed");
        exit(EXIT_FAILURE);  
    }
    sync_server.sun_family = AF_UNIX;
    strcpy(sync_server.sun_path, SYNC_SOCKET_PATH);

    while (connect(sync_sock, (struct sockaddr *) &sync_server, sizeof(struct sockaddr_un)) < 0) {
        perror("error connecting to sync service stream socket, trying again");
        sleep(2);
    }
    std::cout << "Sync service is up\n";

    // Close the socket to the sync service
    close(sync_sock);

    // Initialise all client_socket[] to 0 so not checked 
    for (i = 0; i < max_clients; i++)  
    {  
        client_socket[i] = 0;  
    } 
        
    // Create a master socket 
    if((master_socket = socket(AF_UNIX, SOCK_STREAM, 0)) == 0)  
    {  
        std::cout << "Failed to create a socket " << "\n";  
        exit(EXIT_FAILURE);  
    }  
    
    // Set master socket to allow multiple connections -> this is just a good habit, it will work without this 
    if(setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)  
    {  
        std::cout << "Failed to set the socket options " << "\n";
        exit(EXIT_FAILURE);  
    }  
    
    // Type of socket created -> UNIX socker
    address.sun_family = AF_UNIX;  

    // Add Socket path to the address
    snprintf(address.sun_path, sizeof(address.sun_path), TL_SOCKET_PATH);

    // Unlink the socket path (incase it was not deleted previously)
    unlink(TL_SOCKET_PATH);
        
    // Bind the socket to the address
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)  
    {  
        std::cout << "Failed to bind to the socket " << "\n";
        exit(EXIT_FAILURE);  
    }  

    // Listen for Connections
    std::cout << "Listening for connections ..." << "\n";  
        
    // Try to specify maximum of 3 pending connections for the master socket 
    if (listen(master_socket, 3) < 0)  
    {  
        std::cout << "listen failed" << "\n";  
        exit(EXIT_FAILURE);  
    }  
        
    // Accept the incoming connection 
    addrlen = sizeof(address);  
    std::cout << "Waiting for connections ..." << "\n";  

    // Install SIGINT Signal Handler for exit
    signal(SIGINT, exit_handler);

    // Catch Signal Handler SIGPIPE 
    signal(SIGPIPE, sigpipe_handler);

    // Instantiate the Timeline registry
    TimelineRegistry tl_registry;

    // Instantiate the Global Timeline and Timeline Clock
    /* Note the global timeline is not used explicitly 
       the pointer to the global clock is shared accross
       all the instantiated global timelines */
    qot_timeline_t global_timeline;
    strcpy(global_timeline.name, "gl_global");
    global_timeline.type = QOT_TIMELINE_GLOBAL;
    try
    {
        GlobalClock = new TimelineClock(global_timeline, true);
    }
    catch (std::bad_alloc &ba)
    {
        // Unlink the socket    
        unlink(TL_SOCKET_PATH);
        exit(EXIT_FAILURE); 
    }

    // Instantiate the Local Timeline and Timeline Clock
    /* Note the local timeline is not used explicitly 
       the pointer to the local clock is shared accross
       all the instantiated local timelines */
    qot_timeline_t local_timeline;
    strcpy(local_timeline.name, "local");
    local_timeline.type = QOT_TIMELINE_LOCAL;
    try
    {
        LocalClock = new TimelineClock(local_timeline, true);
    }
    catch (std::bad_alloc &ba)
    {
        // Unlink the socket    
        unlink(TL_SOCKET_PATH);
        exit(EXIT_FAILURE); 
    }

    // Create a timeline and a binding to the default global timeline named "global" to kickstart the global clock sync
    qot_binding_t timeline_serv_binding;
    tl_ptr = new TimelineCore(global_timeline, tl_registry, node_uuid, rest_server, pub_server);
    status_flag = tl_ptr->query_status_flag();
    // If an error is detected during the class creation, call the destructor and exit
    if (status_flag > 0)
    {
        delete tl_ptr;
        // Unlink the socket and exit 
        unlink(TL_SOCKET_PATH);
        exit(EXIT_FAILURE);   
    }
    else
    {
        // create an initial binding with a default QoT spec to kickstart the global sync
        // Set the binding info
        strcpy(timeline_serv_binding.name, "timeline_service");
        timeline_serv_binding.id = 0;

        // Get the QoT demand information
        timeline_serv_binding.demand.resolution.sec = 0;
        timeline_serv_binding.demand.resolution.asec = 10000000000ULL;          // 10 ns resolution
        timeline_serv_binding.demand.accuracy.above.sec = 0;
        timeline_serv_binding.demand.accuracy.above.asec = 1000000000000000ULL; // 1 ms accuracy
        timeline_serv_binding.demand.accuracy.below.sec = 0;
        timeline_serv_binding.demand.accuracy.above.asec = 1000000000000000ULL; // 1 ms accuracy
        tl_ptr->create_binding(timeline_serv_binding);     
    }

    #ifdef QOT_DEF_LOCAL_TL
    std::cout << "Starting a Local timeline ..." << "\n";  
    // Create a default local timeline to kickstart the local clock sync
    strcpy(local_timeline.name, "local_tl");
    tl_ptr = new TimelineCore(local_timeline, tl_registry, node_uuid, rest_server, pub_server);
    status_flag = tl_ptr->query_status_flag();
    // If an error is detected during the class creation, call the destructor and exit
    if (status_flag > 0)
    {
        delete tl_ptr;
        // Unlink the socket and exit 
        unlink(TL_SOCKET_PATH);
        exit(EXIT_FAILURE);   
    }
    else
    {
        // create an initial binding with a default QoT spec to kickstart the global sync
        // Set the binding info
        strcpy(timeline_serv_binding.name, "timeline_service");
        timeline_serv_binding.id = 0;

        // Get the QoT demand information
        timeline_serv_binding.demand.resolution.sec = 0;
        timeline_serv_binding.demand.resolution.asec = 10000000000ULL;          // 10 ns resolution
        timeline_serv_binding.demand.accuracy.above.sec = 0;
        timeline_serv_binding.demand.accuracy.above.asec = 1000000000000000ULL; // 1 ms accuracy
        timeline_serv_binding.demand.accuracy.below.sec = 0;
        timeline_serv_binding.demand.accuracy.above.asec = 1000000000000000ULL; // 1 ms accuracy
        tl_ptr->create_binding(timeline_serv_binding);     
    }
    #endif


    
    // Main Loop listening for commands
    while(running)  
    {  
        // Clear the socket set 
        FD_ZERO(&readfds);  
    
        // Add master socket to set 
        FD_SET(master_socket, &readfds);  
        max_sd = master_socket;  
            
        // Add child sockets to set 
        for (i = 0; i < max_clients; i++)  
        {  
            // Socket descriptor 
            sd = client_socket[i];  
                
            // If valid socket descriptor then add to read list 
            if(sd > 0)  
                FD_SET(sd, &readfds);
                
            // Highest file descriptor number, need it for the select function 
            if(sd > max_sd)  
                max_sd = sd;
        }  

        // Reset Timeout
        timeout.tv_sec = TIMEOUT;
        timeout.tv_usec = 0;
    
        // Wait for an activity on one of the sockets, timeout is set to 5s (to enable program termination), 
        activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);  

        if ((activity < 0) && (errno!=EINTR))  
        {  
            std::cout << "select experienced an error" << "\n";  
        }  
        else if ((activity < 0) && (errno==EINTR))
        {
            std::cout << "Received Interrupt\n";
            continue;
        }
        else
        { 
            // If something happened on the master socket, then its an incoming connection 
            if (FD_ISSET(master_socket, &readfds))  
            {  
                if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)  
                {  
                    std::cout << "accept failure\n";  

                    // Delete the global timeline clock
                    delete GlobalClock;
                    // Unlink the socket    
                    unlink(TL_SOCKET_PATH);
                    exit(EXIT_FAILURE);  
                }  
                
                // Inform user of socket number - used in send and receive commands 
                std::cout << "New connection, socket fd is " << new_socket << "\n";   
                    
                // Add new socket to array of sockets 
                for (i = 0; i < max_clients; i++)  
                {  
                    // If position is empty 
                    if(client_socket[i] == 0)  
                    {  
                        client_socket[i] = new_socket;  
                        printf("Adding to list of sockets as %d\n" , i);        
                        break;  
                    }  
                }  
            }  
                
            // Else it is some IO operation on some other socket
            for (i = 0; i < max_clients; i++)  
            {  
                sd = client_socket[i];  
                    
                if (FD_ISSET(sd, &readfds))  
                {  
                    // Check if it was for closing, and also read the incoming message 
                    const unsigned int MAX_BUF_LENGTH = 4096;
                    std::vector<char> buffer(MAX_BUF_LENGTH);
                    std::string rcv;   
                    int bytesReceived = 0;
                    int recv_flag = 0;
                    do {
                        bytesReceived = read(sd, &buffer[0], buffer.size());
                        // Append string from buffer.
                        if (bytesReceived == -1 && recv_flag == 0) 
                        { 
                            // error 0 -> handle it ! (TBD)
                        } 
                        else if (bytesReceived == 0)
                        {
                            // Somebody disconnected, get details and print 
                            getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);  
                            std::cout << "Host disconnected fd is " << sd << "\n"; 
                                
                            // Close the socket and mark as 0 in list for reuse 
                            close(sd);  
                            client_socket[i] = 0;  
                            break;
                        }
                        else 
                        {
                            rcv.append(buffer.cbegin(), buffer.cend());
                            recv_flag = 1;
                        }
                    } while ( bytesReceived == MAX_BUF_LENGTH );  

                    if (recv_flag == 0) // No message received
                        continue;

                    /* De-serialize data */
                    nlohmann::json data = nlohmann::json::parse(rcv);
                    deserialize_tlmsg(data, tl_msg);

                    // Parse the message and send data to kernel module/ application
                    tl_msg.retval = QOT_RETURN_TYPE_OK;
                    std::cout << "Message Received \n";
                    std::cout << "Type           : " << tl_msg.msgtype << "\n";
                    std::cout << "Guest TL ID    : " << tl_msg.info.index << "\n";
                    std::cout << "Guest TL Name  : " << tl_msg.info.name << "\n";
                
                    // Take action based on the message type
                    switch(tl_msg.msgtype)
                    {
                        case TIMELINE_CREATE:
                            tl_ptr = new TimelineCore(tl_msg.info, tl_registry, node_uuid, rest_server, pub_server);
                            status_flag = tl_ptr->query_status_flag();
                            // If an error is detected during the class creation, call the destructor
                            if (status_flag > 0)
                            {
                                delete tl_ptr;
                                if (status_flag > 1) 
                                    tl_msg.retval = QOT_RETURN_TYPE_ERR;
                                else // Timeline exists
                                    tl_msg.retval = QOT_RETURN_TYPE_OK;
                            }
                            else
                            {
                                // Check if timeline is local and set the peers (this may need to be fetched from the coord service)
                                if (tl_msg.info.type == QOT_TIMELINE_LOCAL && peer_flag == 1)
                                    tl_ptr->update_local_peers(peer_clients);
                                tl_msg.retval = QOT_RETURN_TYPE_OK;
                            }

                            break;
                        case TIMELINE_DESTROY:
                            tl_ptr = (TimelineCore*)tl_registry.qot_tl_class_get(tl_msg.info.index);
                            // If a valid pointer is returned
                            if (tl_ptr)
                            {
                                // Check if the timeline has no bindings before destroying
                                std::cout << "TimelineDestroy:Timeline binding count is " << tl_ptr->get_binding_count() << "\n";
                                if (tl_ptr->get_binding_count() == 0)
                                    delete tl_ptr;
                                tl_msg.retval = QOT_RETURN_TYPE_OK;
                            }
                            else
                                tl_msg.retval = QOT_RETURN_TYPE_ERR;

                            break;
                        case TIMELINE_UPDATE:
                            tl_ptr = (TimelineCore*)tl_registry.qot_tl_class_get(tl_msg.info.index);
                            // If a valid pointer is returned
                            if (tl_ptr)
                            {
                                tl_ptr->update_binding(tl_msg.binding);
                                tl_msg.retval = QOT_RETURN_TYPE_OK;
                            }
                            else
                                tl_msg.retval = QOT_RETURN_TYPE_ERR;
                            
                            break;
                        case TIMELINE_BIND:
                            tl_ptr = (TimelineCore*)tl_registry.qot_tl_class_get(tl_msg.info.index);
                            // If a valid pointer is returned
                            if (tl_ptr)
                            {
                                tl_ptr->create_binding(tl_msg.binding);
                                tl_msg.retval = QOT_RETURN_TYPE_OK;
                                std::cout << "TimelineBind:Timeline binding count is " << tl_ptr->get_binding_count() << "\n";
                            }
                            else
                                tl_msg.retval = QOT_RETURN_TYPE_ERR;

                            break;
                        case TIMELINE_UNBIND:
                            tl_ptr = (TimelineCore*)tl_registry.qot_tl_class_get(tl_msg.info.index);
                            // If a valid pointer is returned
                            if (tl_ptr)
                            {
                                tl_ptr->delete_binding(tl_msg.binding);
                                tl_msg.retval = QOT_RETURN_TYPE_OK;
                                std::cout << "TimelineUnBind:Timeline binding count is " << tl_ptr->get_binding_count() << "\n";
                            }
                            else
                                tl_msg.retval = QOT_RETURN_TYPE_ERR;

                            break;
                        case TIMELINE_QUALITY:
                            tl_ptr = (TimelineCore*)tl_registry.qot_tl_class_get(tl_msg.info.index);
                            // If a valid pointer is returned
                            if (tl_ptr)
                            {
                                tl_msg.demand = tl_ptr->get_desired_qot();
                                tl_msg.binding.demand = tl_msg.demand;
                                tl_msg.retval = QOT_RETURN_TYPE_OK;
                            }
                            else
                                tl_msg.retval = QOT_RETURN_TYPE_ERR;

                            break;
                        case TIMELINE_INFO:
                            tl_ptr = (TimelineCore*)tl_registry.qot_tl_class_get(tl_msg.info.index);
                            // If a valid pointer is returned
                            if (tl_ptr)
                            {
                                tl_msg.info = tl_ptr->get_timeline_info();
                                tl_msg.retval = QOT_RETURN_TYPE_OK;
                            }
                            else
                                tl_msg.retval = QOT_RETURN_TYPE_ERR;

                            break;
                        case TIMELINE_SHM_CLOCK:
                            tl_ptr = (TimelineCore*)tl_registry.qot_tl_class_get(tl_msg.info.index);
                            // If a valid pointer is returned
                            if (tl_ptr)
                            {
                                // Get the readonly file descriptor
                                clk_fd = tl_ptr->get_rdonly_shm_fd();
                                tl_msg.retval = QOT_RETURN_TYPE_OK;
                                n_bytes = send_fd(sd, clk_fd);
                                if (n_bytes < 0)
                                {
                                    perror("sendmsg() sending clock shm fd failed");
                                    tl_msg.retval = QOT_RETURN_TYPE_ERR;
                                }
                                else
                                {
                                    std::cout << "Sent rd-only shm fd to client process\n";
                                    tl_msg.retval = QOT_RETURN_TYPE_OK;
                                }
                            }
                            else
                                tl_msg.retval = QOT_RETURN_TYPE_ERR;

                            break;

                        case TIMELINE_SHM_CLKSYNC:
                            tl_ptr = (TimelineCore*)tl_registry.qot_tl_class_get(tl_msg.info.index);
                            // If a valid pointer is returned
                            if (tl_ptr)
                            {
                                // Get the read-write file descriptor
                                clk_fd = tl_ptr->get_shm_fd();
                                tl_msg.retval = QOT_RETURN_TYPE_OK;
                                if (send_fd(sd, clk_fd) < 0)
                                {
                                    perror("sendmsg() sending clock shm fd failed");
                                    tl_msg.retval = QOT_RETURN_TYPE_ERR;
                                }
                                else
                                {
                                    std::cout << "Sent shm fd to clock-sync process\n";
                                    tl_msg.retval = QOT_RETURN_TYPE_OK;
                                }
                            }
                            else
                                tl_msg.retval = QOT_RETURN_TYPE_ERR;

                            break;

                        case TIMELINE_OV_SHM_CLOCK:
                            tl_ptr = (TimelineCore*)tl_registry.qot_tl_class_get(tl_msg.info.index);
                            // If a valid pointer is returned
                            if (tl_ptr)
                            {
                                // Get the readonly file descriptor
                                clk_fd = tl_ptr->get_overlay_rdonly_shm_fd();
                                tl_msg.retval = QOT_RETURN_TYPE_OK;
                                n_bytes = send_fd(sd, clk_fd);
                                if (n_bytes < 0)
                                {
                                    perror("sendmsg() sending overlay clock shm fd failed");
                                    tl_msg.retval = QOT_RETURN_TYPE_ERR;
                                }
                                else
                                {
                                    std::cout << "Sent rd-only overlay shm fd to client process\n";
                                    tl_msg.retval = QOT_RETURN_TYPE_OK;
                                }
                            }
                            else
                                tl_msg.retval = QOT_RETURN_TYPE_ERR;

                            break;

                        case TIMELINE_OV_SHM_CLKSYNC:
                            tl_ptr = (TimelineCore*)tl_registry.qot_tl_class_get(tl_msg.info.index);
                            // If a valid pointer is returned
                            if (tl_ptr)
                            {
                                // Get the read-write file descriptor
                                clk_fd = tl_ptr->get_overlay_shm_fd();
                                tl_msg.retval = QOT_RETURN_TYPE_OK;
                                if (send_fd(sd, clk_fd) < 0)
                                {
                                    perror("sendmsg() sending overlay clock shm fd failed");
                                    tl_msg.retval = QOT_RETURN_TYPE_ERR;
                                }
                                else
                                {
                                    std::cout << "Sent overlay shm fd to clock-sync process\n";
                                    tl_msg.retval = QOT_RETURN_TYPE_OK;
                                }
                            }
                            else
                                tl_msg.retval = QOT_RETURN_TYPE_ERR;

                            break;

                        case TIMELINE_GET_SERVER:
                            tl_ptr = (TimelineCore*)tl_registry.qot_tl_class_get(tl_msg.info.index);
                            // If a valid pointer is returned
                            if (tl_ptr)
                            {
                                qot_server_t server;
                                if (tl_ptr->get_server(server) == 0)
                                {
                                    // Send the data as space separated values [hostname type stratum]
                                    tl_msg.aux_data = server.hostname + " " + server.type + " "  + std::to_string(server.stratum);
                                    tl_msg.retval = QOT_RETURN_TYPE_OK;
                                }
                                else
                                    tl_msg.retval = QOT_RETURN_TYPE_ERR;
                            }
                            else
                                tl_msg.retval = QOT_RETURN_TYPE_ERR;

                            break;

                        case TIMELINE_SET_SERVER:
                            tl_ptr = (TimelineCore*)tl_registry.qot_tl_class_get(tl_msg.info.index);
                            // If a valid pointer is returned
                            if (tl_ptr)
                            {
                                qot_server_t server;
                                // Parse the data as space separated values [hostname type stratum]
                                std::istringstream iss(tl_msg.aux_data);
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
                                std::cout << "qot_timeline_service: TIMELINE_SET_SERVER: hostname " << server.hostname << " type " << server.type << " stratum " << server.stratum << "\n";
                                if (tl_ptr->set_server(server) == 0)
                                    tl_msg.retval = QOT_RETURN_TYPE_OK;
                                else
                                    tl_msg.retval = QOT_RETURN_TYPE_ERR;
                            }
                            else
                                tl_msg.retval = QOT_RETURN_TYPE_ERR;

                            break;

                        case TIMELINE_REQ_LATENCY:
                            tl_ptr = (TimelineCore*)tl_registry.qot_tl_class_get(tl_msg.info.index);
                            // If a valid pointer is returned -> Function TBD
                            if (tl_ptr)
                            {
                               tl_msg.retval = QOT_RETURN_TYPE_OK; 
                            }
                            else
                               tl_msg.retval = QOT_RETURN_TYPE_ERR; 

                            break;

                        case TIMELINE_GET_LATENCY:
                            tl_ptr = (TimelineCore*)tl_registry.qot_tl_class_get(tl_msg.info.index);
                            // If a valid pointer is returned -> Function TBD
                            if (tl_ptr)
                            {
                                tl_msg.retval = QOT_RETURN_TYPE_OK;
                            }
                            else
                               tl_msg.retval = QOT_RETURN_TYPE_ERR; 
                            
                            break;

                        default:
                            tl_msg.retval = QOT_RETURN_TYPE_ERR;
                            break;
                    }

                    // Check if the request was for a shm file descriptor
                    if ((tl_msg.msgtype == TIMELINE_SHM_CLOCK || tl_msg.msgtype == TIMELINE_OV_SHM_CLOCK)  && tl_msg.retval != QOT_RETURN_TYPE_ERR)
                    {
                        std::cout << "Succesfully sent read-only shm file descriptor\n";
                    }
                    else if ((tl_msg.msgtype == TIMELINE_SHM_CLKSYNC || tl_msg.msgtype == TIMELINE_OV_SHM_CLKSYNC) && tl_msg.retval != QOT_RETURN_TYPE_ERR)
                    {
                        std::cout << "Succesfully sent shm file descriptor\n";
                    }
                    else
                    {
                        // Send Populated message struct back to the user
                        std::cout << "Generated Reply\n";
                        std::cout << "Type          : " << tl_msg.msgtype << "\n";
                        std::cout << "Host TL ID    : " << tl_msg.info.index << "\n";
                        std::cout << "Host TL Name  : " << tl_msg.info.name << "\n";
                        std::cout << "Retval        : " << tl_msg.retval << "\n";
                
                        /* Serialize Message */
                        data = serialize_tlmsg(tl_msg);
                        std::string msg_string = data.dump();
                        int bytes = send(sd, msg_string.c_str() , msg_string.length(), 0); 
                    }
                }  
            }
        }  
    }  

    std::cout << "Timeline service stopping ...\n";

    // Delete the Global Timeline Clock
    delete GlobalClock;

    // Delete the Global Timeline and its binding
    tl_ptr = (TimelineCore*)tl_registry.qot_tl_class_get(global_timeline.index);
    // If a valid pointer is returned
    if (tl_ptr)
    {
        tl_ptr->delete_binding(timeline_serv_binding);
        std::cout << "TimelineUnBind:The timeline service binding is deleted and the Timeline binding count is " << tl_ptr->get_binding_count() << "\n";
        delete tl_ptr;
        std::cout << "TimelineDestroy: Destroyed the default global timeline\n";
    }
    

    // Unlink the socket    
    unlink(TL_SOCKET_PATH);
    return 0;  
}