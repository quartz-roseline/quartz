/**
 * @file qot_sync_service.cpp
 * @brief Clock Synchronization Service main file
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

// Boost includes
#include <boost/log/core.hpp>
#include <boost/thread.hpp> 
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

// C++ Standard Library Headers
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <thread>

// Header to the sync class
#include "sync/Sync.hpp"

// Sync Service Headers
#include "qot_sync_service.hpp"

// C Headers
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

// Include the QoT Data Types
extern "C"
{
    #include "../../qot_types.h"
}

// Peer timestamping headers
#include "sync/huygens/PeerTSserver.hpp"
#include "sync/huygens/PeerTSclient.hpp"
#include "sync/huygens/PeerTSreceiver.hpp"

// Add header to Modern JSON C++ Library
#include "../../../thirdparty/json-modern-cpp/json.hpp"

// Sync Messages serialization library
#include "qot_syncmsg_serialize.hpp"

// Timeline Server type
#include "../timeline-service/qot_tl_types.hpp"

// JSON C++ namespace
using json = nlohmann::json;

using namespace qot;

// Maximum Clients
#define MAX_CLIENTS 30

// Select Timeout (seconds)
#define TIMEOUT 5

// Default NATS Server
#define NATS_SERVER "nats://localhost:4222"

// Running Flag
int sync_service_running = 1;

// Flag to indicate if the global timeline has a synchronization session going on
static int global_tlsync_flag = 0;

// Flag to indicate if the local timelines have a synchronization session going on
static int local_tlsync_flag = 0;

// Flag indicating if PTP must be used
static int ptp_flag = 0; // Flag indicating if PTP must be used 

// Pointer to global timeline sync service
boost::shared_ptr<Sync> GlobalSync = NULL;

// Pointer to local timeline sync service (PTP-based)
boost::shared_ptr<Sync> LocalSync = NULL;

// Data Structure maintaining the peer receivers (timeline uuid -> reciever map)
std::map<std::string, PeerTSreceiver*> peer_receivermap;

// Exit Handler to terminate the program on Ctrl+C
static void exit_handler(int s)
{
    std::cout << "Exit requested " << std::endl;
    sync_service_running = 0;
}

// Catch Signal Handler functio
static void sigpipe_handler(int signum){

    printf("Caught signal SIGPIPE %d\n",signum);
}

// Internal data structure to hold sync service and timeline info
typedef struct timeline_sync {
	qot_timeline_t info;
	boost::shared_ptr<Sync> sync;
} tl_sync_t;

// Generate a random string
std::string RandomString(uint32_t length)
{
	auto randchar = []() -> char
	{
		const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		const size_t max_index = (sizeof(charset) - 1);
		return charset[ rand() % max_index ];
	};
	std::string str(length,0);
	std::generate_n( str.begin(), length, randchar );
	return str;
}

/* Function which monitors if the peer server needs to be restarted */
int PeerServerMon(PeerTSserver *peerserver, std::string &hostname)
{
    while(sync_service_running)  
    {
        sleep(1);
        // Check error status of peer server
        if (peerserver->GetErrorStatus())
        {
            peerserver->Stop();
            peerserver->Start(hostname);
        }
    }
    return 0;
}

/* Function which monitors if the peer client needs to be restarted */
int PeerClientMon(PeerTSclient *peerclient, std::string &hostname, uint64_t tx_period_ns, int *peer_client_running)
{
    while(sync_service_running && *peer_client_running)  
    {
        sleep(1);
        // Check error status of peer server
        if (peerclient->GetErrorStatus())
        {
            peerclient->Stop();
            peerclient->Start(hostname, tx_period_ns);
        }
    }
    return 0;
}

/* Function which for now kick starts the peer sync */
int StartPeerClients(nlohmann::json &cluster_config_data, std::string node_name)
{
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
                std::cout << "QoTSyncService: Starting a peer client for " << (*col) << "\n";
            }
            counter++;
        }
    }
    return 0;
}

/* Handle Deferred Message */
int deferred_message_handler(qot_sync_msg_t tl_msg)
{
    void **temp_dptr;
    void *temp_ptr;
    switch(tl_msg.msgtype)
    {
        case TL_CREATE_UPDATE:
            // Get the primary local timeline shared memory 
            if (tl_msg.info.type == QOT_TIMELINE_LOCAL && local_tlsync_flag == 0)  
            {   
                std::cout << "Deferred Request executing to get local timeline main clock\n";
                temp_ptr = (void*)&tl_msg.info.index;
                temp_dptr = &temp_ptr;
                if (!GlobalSync->ExtControl(temp_dptr, REQ_LOCAL_TL_CLOCK_MAIN))
                {
                    local_tlsync_flag = 1;
                }
            }

            // Get the overlay local timeline shared memory 
            if (tl_msg.info.type == QOT_TIMELINE_LOCAL && ptp_flag == 0)  
            {  
                std::cout << "Deferred Request executing to setup peer sync\n";
                temp_ptr = (void*)&tl_msg.info.index;
                temp_dptr = &temp_ptr;
                if (!GlobalSync->ExtControl(temp_dptr, REQ_LOCAL_TL_CLOCK_OV))
                {
                    // Set the memory location to the peer receiver   
                    temp_ptr = *temp_dptr;  
                    peer_receivermap[std::string(tl_msg.info.name)]->SetClkParamVar((tl_translation_t*)temp_ptr);
                }
            }

            if (tl_msg.info.type == QOT_TIMELINE_GLOBAL && global_tlsync_flag == 1)  
            { 
                // Add the timeline to the QoT Map
                temp_ptr = (void*) &tl_msg;
                temp_dptr = &temp_ptr;
                GlobalSync->ExtControl(temp_dptr, ADD_TL_SYNC_DATA);
                
                // Get the server for the timeline
                qot_server_t server;
                server.timeline_id = tl_msg.info.index;
                temp_ptr = (void*) &server;
                temp_dptr = &temp_ptr;
                if (!GlobalSync->ExtControl(temp_dptr, GET_TIMELINE_SERVER))
                {
                    std::cout << "Got the Server for timeline " << server.timeline_id << " hostname " << server.hostname << "\n";
                    // Set the server -> Make a client call using chronyc client
                    std::string server_command = "add server " + server.hostname;
                    char *server_data =  const_cast<char*>(server_command.c_str());
                    temp_ptr = (void*) server_data;
                    temp_dptr = &temp_ptr;
                    if (!GlobalSync->ExtControl(temp_dptr, MODIFY_SYNC_PARAMS))
                    {
                        std::cout << "Set the Server for timeline " << server.timeline_id << " hostname " << server.hostname << "\n";
                    }
                    else
                    {
                        std::cout << "Failed to set the server for timeline " << server.timeline_id << "\n";
                    }
                }
                else // The server does not exists, needs to be set
                {
                    std::cout << "No Server exists for timeline " << server.timeline_id << "\n";
                }
            }
            break;

        default:
            break;
    }
    return 0;
}


/* Sync Service Main Function */
int main(int argc , char *argv[])  
{  
    int opt = 1; // TRUE 
    int retval = 0;
    int master_socket, new_socket, client_socket[MAX_CLIENTS], max_clients = MAX_CLIENTS , activity, i , valread , sd;  
    socklen_t addrlen;
    int max_sd;  
    void **temp_dptr;
    void *temp_ptr;

    // Seed the random number generated with a nanosecond count
	struct timespec t={0,0};
	clock_gettime(CLOCK_REALTIME, &t);
	srand(t.tv_nsec);

	// Parse command line options
	boost::program_options::options_description desc("Allowed options");
	desc.add_options()
		("help,h",       "produce help message")
		("verbose,v",    "print verbose debug messages")
		("iface,i",      boost::program_options::value<std::string>()->default_value("eth0"), "PTP-compliant interface") 
		("name,n",       boost::program_options::value<std::string>()->default_value(RandomString(32)), "name of this node")
		("addr,a",       boost::program_options::value<std::string>()->default_value("192.168.2.33"), "ip address for this node")
        ("peerserver,p",  boost::program_options::value<int>()->default_value(0), "port on which the peer to peer rtt measurement server listens")
        ("natsserver,m",  boost::program_options::value<std::string>()->default_value(NATS_SERVER), "NATS server(s) which to connect to for Peer Sync")
        ("discipline,d",  boost::program_options::value<bool>()->default_value(false), "Flag indicating if the PHC corresponding to the interface should be disciplined")
        ("ntpconfig,c",  boost::program_options::value<std::string>()->default_value("/etc/chrony.conf"), "NTP Chrony Configuration file")
        ("logsyncrate,r",  boost::program_options::value<int>()->default_value(0), "default synchronization rate")
    ;
	boost::program_options::variables_map vm;
	boost::program_options::store(
		boost::program_options::parse_command_line(argc, argv, desc), vm);
	boost::program_options::notify(vm);    

	// Set logging level
	if (vm.count("verbose") > 0)
	{
		boost::log::core::get()->set_filter
	    (
	        boost::log::trivial::severity >= boost::log::trivial::info
	    );
	}
	else
	{
		boost::log::core::get()->set_filter
	    (
	        boost::log::trivial::severity >= boost::log::trivial::warning
	    );
	}

	// Print some help with arguments
	if (vm.count("help") > 0)
	{
		std::cout << desc << "\n";
		return 0;
	}

	// Some friendly debug
	BOOST_LOG_TRIVIAL(info) << "Node unique name is " << vm["name"].as<std::string>();
	BOOST_LOG_TRIVIAL(info) << "Performing synchronization over interface " << vm["iface"].as<std::string>();
	BOOST_LOG_TRIVIAL(info)	<< "IP address is " << vm["addr"].as<std::string>();

    // Spawn thread for the peer-delay server & and receiver
    PeerTSserver *peerserver = NULL;
    PeerTSreceiver *peerreceiver = NULL;
    boost::thread peerserver_mon;
    if (vm["peerserver"].as<int>() != 0)
    {
        BOOST_LOG_TRIVIAL(info) << "Peer Delay option is chosen starting a peer-delay server\n";
        peerserver = new PeerTSserver(vm["peerserver"].as<int>(), vm["iface"].as<std::string>(), 0, 2);
        peerserver->Start(vm["name"].as<std::string>());
        peerserver_mon = boost::thread(PeerServerMon, peerserver, vm["name"].as<std::string>());

        // Setup the receiver to get the offset
        // peerreceiver = new PeerTSreceiver(vm["name"].as<std::string>(), vm["natsserver"].as<std::string>(), vm["iface"].as<std::string>(), vm["discipline"].as<bool>());
        // peerreceiver->Start(2000000000);
    }
    else
    {
        ptp_flag = 1;
    }

    // Consructor status flag
    int status_flag = 0;

    // Deferred request flag
    int def_req_flag = 0;

    // Pointer to deferred request thread
    std::thread *def_req_thread = NULL;

    // Socket Address
    struct sockaddr_un address;
        
    // Set of socket descriptors 
    fd_set readfds; 

    // Timeval for select timeout
    struct timeval timeout = {TIMEOUT,0};

    // Data Structure maintaining timelines for which the sync service exists
    std::map<std::string, tl_sync_t> timeline_syncmap;
    std::map<std::string, tl_sync_t>::iterator it;

    // Data Structure maintaining the peer clients
    std::map<std::string, PeerTSclient*> peer_clientmap;
    std::map<std::string, PeerTSclient*>::iterator peer_clientit;

    // Data Structure maintaining the peer client monitoring threads (hostname -> client map)
    std::map<std::string, boost::thread> peer_threadmap;
    std::map<std::string, int> peer_threadflag;

    // Data Structure maintaining the peer receivers (timeline uuid -> reciever map)
    // std::map<std::string, PeerTSreceiver*> peer_receivermap;
    std::map<std::string, PeerTSreceiver*>::iterator peer_receiverit;

    // Create the boost asio required for clock sync services
    boost::asio::io_service io;
	boost::asio::io_service::work work(io);

	// Sync address and iface
	std::string addr;
	std::string iface;

	// Timeline Sync data structure
	tl_sync_t timeline_syncvar;

    // QoT Sync Message
    qot_sync_msg_t tl_msg; 

    // Node name
    std::string node_uuid = vm["name"].as<std::string>();

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
    snprintf(address.sun_path, sizeof(address.sun_path), SYNC_SOCKET_PATH);

    // Unlink the socket (incase it was not deleted previously on closing the sync service)
    unlink(SYNC_SOCKET_PATH);
        
    // Bind the socket to the address
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)  
    {  
        std::cout << "Failed to bind to the socket " << "\n";
        exit(EXIT_FAILURE);  
    }  

    // Give all users priveleges to access this socket -> Maybe a security risk
    chmod(address.sun_path, 0777);

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

    // Main Loop listening for commands
    while(sync_service_running)  
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

                    // Unlink the socket    
                    unlink(SYNC_SOCKET_PATH);
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
                    deserialize_syncmsg(data, tl_msg);
                   
                    tl_msg.retval = QOT_RETURN_TYPE_OK;
                    std::cout << "Message Received \n";
                    std::cout << "Type           : " << tl_msg.msgtype << "\n";
                    std::cout << "Guest TL ID    : " << tl_msg.info.index << "\n";
                    std::cout << "Guest TL Name  : " << tl_msg.info.name << "\n";

                    // Take action based on the message type
                    switch(tl_msg.msgtype)
                    {
                        case TL_CREATE_UPDATE:
                        	// Check if timeline exists in the data structure
                            it = timeline_syncmap.find(std::string(tl_msg.info.name)); 
						    if (it != timeline_syncmap.end())
						    {
						        /* Note: Need some optimization parameters based on specified QoT 
						        TBD: Please revisit this section, restarting the sync seems wasteful */
						        // Timeline exists, update the sync service
						        if (tl_msg.info.type == QOT_TIMELINE_LOCAL)
                                {
						        	if (timeline_syncmap[std::string(tl_msg.info.name)].sync != NULL)
                                    {
                                        // Update the accuracy requirements
                                        temp_ptr = (void*) &tl_msg;
                                        temp_dptr = &temp_ptr;
                                        timeline_syncmap[std::string(tl_msg.info.name)].sync->ExtControl(temp_dptr, ADD_TL_SYNC_DATA);
                                        //timeline_syncmap[std::string(tl_msg.info.name)].sync->Start(true, 1, 0, tl_msg.info.index, NULL, std::string(tl_msg.info.name), node_uuid, 1);
                                    }
                                }
						        else
						        	GlobalSync->Start(true, 1, 0, tl_msg.info.index, NULL, std::string(tl_msg.info.name), node_uuid, 1);
						    }
						    else
						    {
						    	// Add the timeline to the data structure
						    	std::cout << "Creating timeline " << tl_msg.info.index << "\n";
						    	timeline_syncvar.info = tl_msg.info;
						    	timeline_syncmap[std::string(tl_msg.info.name)] = timeline_syncvar;
                                timeline_syncmap[std::string(tl_msg.info.name)].sync = NULL;
						    	
						    	// Decide on what sort of sync service is needed
                                if (tl_msg.info.type == QOT_TIMELINE_LOCAL && local_tlsync_flag == 0)     
                                {
                                    std::cout << "First Local timeline detected, need to get shm for the local timeline\n";
                                    if (GlobalSync != NULL)
                                    {
                                        def_req_flag = 1;
                                    }
                                    else
                                        tl_msg.retval = QOT_RETURN_TYPE_ERR;
                                }
                                else if (tl_msg.info.type == QOT_TIMELINE_LOCAL && local_tlsync_flag == 1)
                                {
                                    std::cout << "New Local timeline detected\n";
                                    // Do nothing for now, later we need to do stuff about the peer sync
                                    tl_msg.retval = QOT_RETURN_TYPE_OK;
                                }
								else if (tl_msg.info.type == QOT_TIMELINE_GLOBAL && global_tlsync_flag == 0)		
						    	{
						    		// Create a new sync service -> if a global sync instance does not exist
						    		GlobalSync = Sync::Factory(&io, vm["addr"].as<std::string>(), vm["iface"].as<std::string>(), SYNC_NTP);

						    		std::cout << "Global timeline detected, and have to start global sync\n";
							    	// Start the sync thread
							    	if (GlobalSync != NULL)
							    	{
										// Set the NATS server
                                        const char* nats_name_ptr = vm["natsserver"].as<std::string>().c_str();
                                        const char** nats_name_dptr = &nats_name_ptr;
                                        GlobalSync->ExtControl((void**)nats_name_dptr, SET_PUBSUB_SERVER);

                                        // Set the Chrony configuration file
                                        char* ntp_cfg_ptr = const_cast<char*>(vm["ntpconfig"].as<std::string>().c_str());
                                        char** ntp_cfg_dptr = &ntp_cfg_ptr;
                                        GlobalSync->ExtControl((void**)ntp_cfg_dptr, SET_INIT_SYNC_CFG);

                                        GlobalSync->Start(true, 1, 0, tl_msg.info.index, NULL, std::string(tl_msg.info.name), node_uuid, 1);
										global_tlsync_flag = 1;
							    	}
									else
										tl_msg.retval = QOT_RETURN_TYPE_ERR;

                                    timeline_syncmap[std::string(tl_msg.info.name)].sync = GlobalSync;
								}
								else
								{
									// The Global Sync is already running
                                    timeline_syncmap[std::string(tl_msg.info.name)].sync = GlobalSync;
                                    def_req_flag = 1; // Set the deferred request flag to get the server corresponding to the timeline
								}

                                // If the timeline is local start the peer receiver or PTP sync
                                if (tl_msg.info.type == QOT_TIMELINE_LOCAL)
                                {
                                    if (ptp_flag == 1)
                                    {
                                        // PTP flag is enabled -> start PTP (logic has to change as only one PTP instance can run)
                                        LocalSync = Sync::Factory(&io, vm["addr"].as<std::string>(), vm["iface"].as<std::string>(), SYNC_PTP);
                                        if (LocalSync != NULL)
                                        {
                                            // Set the NATS server
                                            const char* nats_name_ptr = vm["natsserver"].as<std::string>().c_str();
                                            const char** nats_name_dptr = &nats_name_ptr;
                                            LocalSync->ExtControl((void**)nats_name_dptr, SET_PUBSUB_SERVER);

                                            // Set the Accuracy requirements
                                            temp_ptr = (void*) &tl_msg;
                                            temp_dptr = &temp_ptr;
                                            LocalSync->ExtControl(temp_dptr, ADD_TL_SYNC_DATA);
                                            
                                            // Start PTP (master_flag, log_sync_interval, ptp_domain ...)
                                            int ptp_domain = std::stoi(std::string(tl_msg.data),nullptr,0);
                                            LocalSync->Start(false, vm["logsyncrate"].as<int>(), ptp_domain, tl_msg.info.index, NULL, std::string(tl_msg.info.name), node_uuid, 1);
                                            timeline_syncmap[std::string(tl_msg.info.name)].sync = LocalSync;

                                        }
                                    }
                                    else
                                    {
                                        peer_receiverit = peer_receivermap.find(std::string(tl_msg.info.name));
                                        if (peer_receiverit != peer_receivermap.end())
                                        {
                                           std::cout << "Peer receiver for timeline " << std::string(tl_msg.info.name) << " exists" << "\n";
                                           tl_msg.retval = QOT_RETURN_TYPE_ERR;
                                        }
                                        else
                                        {
                                           // Start the receiver
                                           def_req_flag = 1;
                                           peer_receivermap[std::string(tl_msg.info.name)] = new PeerTSreceiver(vm["name"].as<std::string>(), vm["natsserver"].as<std::string>(), vm["iface"].as<std::string>(), false);
                                           peer_receivermap[std::string(tl_msg.info.name)]->Start(2000000000);                                    
                                           std::cout << "Peer receiver for timeline " << std::string(tl_msg.info.name) << " started" << "\n";
                                        }
                                    }
                                }

						    }

                            break;
                        case TL_DESTROY:
                        	// Check if timeline exists in the data structure
                            it = timeline_syncmap.find(std::string(tl_msg.info.name));
						    if (it != timeline_syncmap.end())
						    {
						        std::cout << "Destroying timeline sync " << tl_msg.info.index << "\n";
						        // Timeline exists and is local, kill the sync service
						        /* Note: For now the global sync instance is kept running */
                                // Do nothing for now, commenting out the stuff below

                                // If the timeline is local stop the peer receiver
                                if (tl_msg.info.type == QOT_TIMELINE_LOCAL)
                                {
                                    std::cout << "Local timeline sync being stopped\n";
                                    if (ptp_flag == 1)
                                    {
                                        // PTP flag is enabled -> stop PTP (logic has to change as only one PTP instance can run)
                                        timeline_syncmap[std::string(tl_msg.info.name)].sync->Stop();
                                    }
                                    else
                                    {
                                        peer_receiverit = peer_receivermap.find(std::string(tl_msg.info.name));
                                        if (peer_receiverit == peer_receivermap.end())
                                        {
                                           std::cout << "Peer receiver for timeline " << std::string(tl_msg.info.name) << " does not exists" << "\n";
                                           tl_msg.retval = QOT_RETURN_TYPE_ERR;
                                        }
                                        else
                                        {
                                           // Stop the receiver
                                           peer_receivermap[std::string(tl_msg.info.name)]->Stop();
                                           delete peer_receivermap[std::string(tl_msg.info.name)];
                                           peer_receivermap.erase(std::string(tl_msg.info.name));  
                                           std::cout << "Peer receiver for timeline " << std::string(tl_msg.info.name) << " stopped" << "\n";
                                        }
                                    }
                                }
                                else // If the timeline is Global
                                {
                                    // Remove the timeline from the QoT Map
                                    void* temp_ptr = (void*) &tl_msg;
                                    void** temp_dptr = &temp_ptr;
                                    GlobalSync->ExtControl(temp_dptr, DEL_TL_SYNC_DATA);
                                }

						    	// Remove the timeline from the data structure
						        timeline_syncmap.erase(std::string(tl_msg.info.name));
						    }
						    else
						    {
						    	// No Sync service exists
						    	tl_msg.retval = QOT_RETURN_TYPE_ERR;
						    }
                            break;

                        case PEER_START:
                            // Check if the peer client already exists
                            std::cout << "Received Peer Client start message for " << std::string(tl_msg.data) << "\n";
                            peer_clientit = peer_clientmap.find(std::string(tl_msg.data));
                            if (peer_clientit != peer_clientmap.end())
                            {
                               std::cout << "Peer client for " << std::string(tl_msg.data) << " exists" << "\n";
                               tl_msg.retval = QOT_RETURN_TYPE_ERR;
                            }
                            else
                            {
                               // Start the client (IP, port, iface, timestamping flag -> 2 signifies hardware -> try hardware timestamping if it is supported)
                               peer_clientmap[std::string(tl_msg.data)] = new PeerTSclient(std::string(tl_msg.data), vm["peerserver"].as<int>(), vm["iface"].as<std::string>(), vm["natsserver"].as<std::string>(), 2);
                               retval = peer_clientmap[std::string(tl_msg.data)]->Start(vm["name"].as<std::string>(), 10000000);
                               if (retval >= 0)
                               {
                                    std::cout << "Peer client for " << std::string(tl_msg.data) << " started" << "\n";

                                    // Start the thread which monitors and restarts the client thread
                                    peer_threadflag[std::string(tl_msg.data)] = 1;
                                    peer_threadmap[std::string(tl_msg.data)] = boost::thread(PeerClientMon, peer_clientmap[std::string(tl_msg.data)], vm["name"].as<std::string>(), 10000000, &peer_threadflag[std::string(tl_msg.data)]);
                                   
                                    // Return value
                                    tl_msg.retval = QOT_RETURN_TYPE_OK;
                               }
                               else
                               {
                                    std::cout << "Peer client for " << std::string(tl_msg.data) << " had error in starting" << "\n";
                                    delete peer_clientmap[std::string(tl_msg.data)];
                                    peer_clientmap.erase(std::string(tl_msg.data));
                                    tl_msg.retval = QOT_RETURN_TYPE_ERR;
                               }
                            }
                            break;

                        case PEER_STOP:
                            // Check if the peer client already exists
                            std::cout << "Received Peer Client stop message for " << std::string(tl_msg.data) << "\n";
                            peer_clientit = peer_clientmap.find(std::string(tl_msg.data));
                            if (peer_clientit != peer_clientmap.end())
                            {
                               // Stop the peer client monitoring thread
                               peer_threadflag[std::string(tl_msg.data)] = 0;
                               peer_threadmap[std::string(tl_msg.data)].join();

                               // Stopping the peer client
                               peer_clientmap[std::string(tl_msg.data)]->Stop();
                               std::cout << "Peer client for " << std::string(tl_msg.data) << " terminated " << "\n";

                               // Remove the client's traces from the associated data structure
                               delete peer_clientmap[std::string(tl_msg.data)];
                               peer_clientmap.erase(std::string(tl_msg.data));
                               peer_threadmap.erase(std::string(tl_msg.data));
                               peer_threadflag.erase(std::string(tl_msg.data));

                               tl_msg.retval = QOT_RETURN_TYPE_OK;
                            }
                            else
                            {
                               std::cout << "Peer client for " << std::string(tl_msg.data) << " does not exist" << "\n";
                               tl_msg.retval = QOT_RETURN_TYPE_ERR;
                            }
                            break;

                        case GLOB_SYNC_UPDATE: 
                            // Check if timeline exists in the data structure
                            it = timeline_syncmap.find(std::string(tl_msg.info.name));
                            if (it != timeline_syncmap.end() && tl_msg.info.type == QOT_TIMELINE_GLOBAL)
                            {
                                // Timeline sync service found & Global Timeline
                                char *cmd_ptr = const_cast<char*>(tl_msg.data.c_str());
                                if (!GlobalSync->ExtControl((void**)&cmd_ptr, MODIFY_SYNC_PARAMS))
                                {
                                    std::cout << "Global Sync succesfully got update command\n";
                                }
                                else
                                {
                                    tl_msg.retval = QOT_RETURN_TYPE_ERR;
                                }
                            }
                            else
                            {
                                // No Sync service exists
                                tl_msg.retval = QOT_RETURN_TYPE_ERR;
                            }
                            break;

                        case SET_NODE_UUID:
                            node_uuid = std::string(tl_msg.data);
                            std::cout << "Node name is set as " << std::string(tl_msg.data) << "\n";
                            tl_msg.retval = QOT_RETURN_TYPE_ERR;
                            break;
                        
                        default:
                            tl_msg.retval = QOT_RETURN_TYPE_ERR;
                            break;
                    }

                    // Send Populated message struct back to the user
                    std::cout << "Generated Reply\n";
                    std::cout << "Type          : " << tl_msg.msgtype << "\n";
                    std::cout << "Host TL ID    : " << tl_msg.info.index << "\n";
                    std::cout << "Host TL Name  : " << tl_msg.info.name << "\n";
                    std::cout << "Retval        : " << tl_msg.retval << "\n";
                    // int bytes = send(sd, &tl_msg , sizeof(tl_msg), 0); 
                    data = serialize_syncmsg(tl_msg);
                    std::string msg_string = data.dump();
                    int bytes = send(sd, msg_string.c_str() , msg_string.length(), 0); 

                    // Check if the processing for any request has been deferred
                    if (def_req_flag == 1)
                    {
                        def_req_flag = 0;
                        // Dispatch a thread to handle the deferred request
                        if (def_req_thread)
                            delete def_req_thread;
                        def_req_thread = new std::thread(deferred_message_handler, tl_msg);
                        def_req_thread->detach();
                    } 
                }  
            }
        }  
    }  

    std::cout << "Clock Sync service stopping ...\n";

    if (vm["peerserver"].as<int>() != 0)
    {
        BOOST_LOG_TRIVIAL(info) << "Peer Delay Server stopping ..\n";

        // Joining peer server monitoring thread
        peerserver_mon.join();

        // Stop the peer server
        peerserver->Stop();
        delete peerserver;

        BOOST_LOG_TRIVIAL(info) << "Peer Delay Receiver stopping ..\n";

        // Stop the peer receiver
        if (peerreceiver)
        {
            peerreceiver->Stop();
            delete peerreceiver;
        }
    }

    // Unlink the socket    
    unlink(SYNC_SOCKET_PATH);
    return 0;  
}