/**
 * @file qot_peer_service.cpp
 * @brief Peer Clock Delay/Offset Calculation Service main file
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
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

// C++ Standard Library Headers
#include <iostream>
#include <map>
#include <string>

// Peer timestamping headers
#include "sync/huygens/PeerTSserver.hpp"
#include "sync/huygens/PeerTSclient.hpp"
#include "sync/huygens/PeerTSreceiver.hpp"

using namespace qot;

// Maximum Clients
#define MAX_CLIENTS 1

// Select Timeout (seconds)
#define TIMEOUT 5

// Default NATS Server
#define NATS_SERVER "nats://localhost:4222"

// Running Flag
int peer_service_running = 1;

// Exit Handler to terminate the program on Ctrl+C
static void exit_handler(int s)
{
    std::cout << "Exit requested " << std::endl;
    peer_service_running = 0;
}

// Catch Signal Handler function
static void sigpipe_handler(int signum){

    printf("Caught signal SIGPIPE %d\n",signum);
}

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

/* Timeline Service Main Function */
int main(int argc , char *argv[])  
{  
    // Seed the random number generated with a nanosecond count
	struct timespec t={0,0};
	clock_gettime(CLOCK_REALTIME, &t);
	srand(t.tv_nsec);
	int mode_flag;
	int timestamping_flag = 2; /* Timestamping flag 2 - HWTS, 0 - SWTS */

	// Parse command line options
	boost::program_options::options_description desc("Allowed options");
	desc.add_options()
		("help,h",       "produce help message")
		("verbose,v",    "print verbose debug messages")
		("iface,i",      boost::program_options::value<std::string>()->default_value("eth0"), "PTP-compliant interface") 
		("name,n",       boost::program_options::value<std::string>()->default_value(RandomString(32)), "name of this node")
        ("peerport,p",  boost::program_options::value<int>()->default_value(0), "port on which the peer to peer rtt measurement server listens")
		("timelineid,d", boost::program_options::value<int>()->default_value(0), "timeline id")
        ("addr,a",  boost::program_options::value<std::string>()->default_value("0"), "peer IP address")
        ("tx_period_ns,t",  boost::program_options::value<uint64_t>()->default_value(1000000000ULL), "peer IP")
        ("natsserver,m",  boost::program_options::value<std::string>()->default_value(NATS_SERVER), "NATS server(s) which to connect to for Peer Sync")
        ("discipline,d",  boost::program_options::value<bool>()->default_value(false), "Flag indicating if the PHC corresponding to the interface should be disciplined")
        ("mode,o",  boost::program_options::value<int>()->default_value(0), "Flag indicating which mode to launch in: 0-normal, 1-client only, 2-server only")
		("timestamping,x",  boost::program_options::value<int>()->default_value(2), "Flag indicating which timestamps to use: 0-SWTS, 2-HWTS")
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

	// Get operating mode
	mode_flag = 0;
	if (vm["mode"].as<int>() >= 0 & vm["mode"].as<int>() < 3)
		mode_flag = vm["mode"].as<int>();

	// Get timestamping mode
	if (vm["timestamping"].as<int>() >= 0 & vm["timestamping"].as<int>() < 3)
		timestamping_flag = vm["timestamping"].as<int>();

	// Some friendly debug
	BOOST_LOG_TRIVIAL(info) << "Node unique name is " << vm["name"].as<std::string>();
	BOOST_LOG_TRIVIAL(info) << "Performing synchronization over interface " << vm["iface"].as<std::string>();
	BOOST_LOG_TRIVIAL(info)	<< "Peer IP address is " << vm["addr"].as<std::string>();

	// Exclusion Set and Multicast Map for Peer Service
	std::set<std::string> exclusion_set;
	std::map<std::string, std::string> multicast_map;

	// Clock Parameters
	tl_translation_t clk_params;
	clk_params.u_nsec = 0;
	clk_params.l_nsec = 0;
	clk_params.u_mult = 0;
	clk_params.l_mult = 0;

	// Initialize Multicast Map
	// multicast_map[std::string("192.168.1.110")] = std::string("224.0.1.129");
	// multicast_map[std::string("192.168.1.111")] = std::string("224.0.1.130");
	// multicast_map[std::string("192.168.1.112")] = std::string("224.0.1.131");
	// multicast_map[std::string("192.168.1.113")] = std::string("224.0.1.132");
	// exclusion_set.insert(std::string("192.168.1.111"));

    // Spawn threads for the peer-delay client and server
    PeerTSclient peerclient(vm["addr"].as<std::string>(), vm["peerport"].as<int>(), vm["iface"].as<std::string>(), vm["natsserver"].as<std::string>(), timestamping_flag);
    PeerTSserver peerserver(vm["peerport"].as<int>(), vm["iface"].as<std::string>(), 0, timestamping_flag, exclusion_set, multicast_map);

    // Setup the receiver to get the offset
    PeerTSreceiver peerreceiver(vm["name"].as<std::string>(), vm["natsserver"].as<std::string>(), vm["iface"].as<std::string>(), vm["discipline"].as<bool>());

    if (mode_flag != 1)
    	peerserver.Start(vm["name"].as<std::string>());

    if (mode_flag != 2)
    {
    	peerreceiver.SetClkParamVar(&clk_params);
    	peerclient.Start(vm["name"].as<std::string>(), vm["tx_period_ns"].as<uint64_t>());
    	peerreceiver.Start(2000000000);
    }

    // Install SIGINT Signal Handler for exit
    signal(SIGINT, exit_handler);

    // Catch Signal Handler SIGPIPE 
    signal(SIGPIPE, sigpipe_handler);

    // Main Loop 
    while(peer_service_running)  
    {
        sleep(1);
        // Check error status of peer server
        if (peerserver.GetErrorStatus() && mode_flag != 1)
        {
        	peerserver.Stop();
        	peerserver.Start(vm["name"].as<std::string>());
        }

        // Check error status of peer server
        if (peerclient.GetErrorStatus() && mode_flag != 2)
        {
        	peerclient.Stop();
        	peerclient.Start(vm["name"].as<std::string>(), vm["tx_period_ns"].as<uint64_t>());
        }
    }

    // Stop the server and client threads
    if (mode_flag != 2)
    {
    	peerclient.Stop();
    	// Stop the peer receiver
    	peerreceiver.Stop();
    }
    if (mode_flag != 1)
    	peerserver.Stop();

    

    return 0;  
}