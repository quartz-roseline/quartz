/**
 * @file PeerTSserver.cpp
 * @brief Peer to Peer Timestamping Echo Server to figure out "network-effect" discrepancies
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

extern "C" 
{
  #include <stdio.h>
  #include <unistd.h>
  #include <stdlib.h>
  #include <string.h>
  #include <netdb.h>
  #include <sys/types.h> 
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <linux/net_tstamp.h>
  #include <sys/uio.h>
  #include <time.h> /* for clock_gettime */
  #include <linux/sockios.h>
  #include <linux/ethtool.h>
  #include <net/if.h>
  #include <sys/ioctl.h> 
  #include <sys/signal.h>
  #include <poll.h>
}

#include "PeerTSserver.hpp"
#include "Timestamping.hpp"

// Add header to spoof PTP messages
#include "ptp_message.hpp"

#define BUFSIZE 1024

using namespace qot;

#define DEBUG_FLAG 0

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
}

// Constructor
PeerTSserver::PeerTSserver(int portno, const std::string &iface, int64_t offset, int ts_flag)
  : portno(portno), iface(iface), offset(offset), ts_flag(ts_flag), running(true)
{
  error_flag = 0;
  error_count = 0;
  if (portno == PTP_PORT)
  {
	ptp_msgflag = 1;
	std::cout << "PeerTSserver: choosing PTP message option\n";
  }
  else
	ptp_msgflag = 0;
}

// Constructor -> 2
PeerTSserver::PeerTSserver(int portno, const std::string &iface, int64_t offset, int ts_flag, std::set<std::string> &exclusion_set, std::map<std::string, std::string> &multicast_map)
  : portno(portno), iface(iface), offset(offset), ts_flag(ts_flag), running(true), exclusion_set(exclusion_set), multicast_map(multicast_map)
{
  error_flag = 0;
  error_count = 0;
  if (portno == PTP_PORT)
  {
	ptp_msgflag = 1;
	std::cout << "PeerTSserver: choosing PTP message option\n";
  }
  else
	ptp_msgflag = 0;
}

// Destructor
PeerTSserver::~PeerTSserver()
{

}

// Control functions
int PeerTSserver::Reset()
{
  running = false;
  server_thread.join();
  running = true;
  server_thread = boost::thread(&PeerTSserver::ts_server_loop, this);
  return ts_flag;
}

int PeerTSserver::Start(const std::string &node_name)
{
  int optval; /* flag value for setsockopt */
  struct timeval recv_timeout_tv;

  error_flag = 0;
  error_count = 0;

  struct in_addr mcast_addr;
  
  if (ptp_msgflag)
  {   
	  // We will be listening for messages on this address
	  if (!inet_aton(node_name.c_str(), &mcast_addr))
		return -1;
  }

  /* socket: create the socket */
  if (ptp_msgflag)
  {
	  sockfd = open_ptp_socket(iface.c_str(), mcast_addr, portno, 1);
	  if (sockfd < 0) 
	  {
		  perror("PeerTSClient: ERROR opening PTP-like socket");
		  return -1;
	  }
  }
  else
  {
	/* 
	 * socket: create the parent socket 
	 */
	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd < 0) 
	{
		error("PeerTSserver: ERROR opening socket");
		return -1;
	}
  }

  node_uuid = node_name;

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
		 (const void *)&optval , sizeof(int));

  /* Set the timeout time to 1 second */
  recv_timeout_tv.tv_sec = 1;
  recv_timeout_tv.tv_usec = 0;

  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout_tv, sizeof(recv_timeout_tv)) < 0)
	perror("PeerTSserver: ERROR in setting socket options to SO_RCVTIMEO");
	
  /* Configure hardware timestamping */
  if (ts_flag == 2)
    ts_flag = tstamp_mode_hardware(sockfd, const_cast<char*>(iface.c_str()));
  else   /* Configure software timestamping */
    ts_flag = tstamp_mode_kernel(sockfd);

  // Spawn the server thread
  running = true;
  server_thread = boost::thread(&PeerTSserver::ts_server_loop, this);
  return ts_flag;
}

int PeerTSserver::Stop()
{
  running = false;
  server_thread.join();
  error_flag = 0;
  error_count = 0;
  return 0;
}

// Function to check error status
bool PeerTSserver::GetErrorStatus()
{
  return error_flag;
}

// Function to start a server which recevies packets from other clients
int PeerTSserver::ts_server_loop()
{
  socklen_t clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct sockaddr_in clientaddr_ucast; /* client addr */

  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char clientname[100];
  struct ptp_message *ptp_msg = msg_allocate(); /* PTP-like message buffer */
  struct in_addr mcast_addr; /* Multi-cast Address */

  char *hostaddrp; /* dotted decimal host addr string */
  int n; /* message byte size */
  char cmsgbuf[BUFSIZE]; /* ancillary info buf */
  struct ip_mreq mreq; /* multicast recv request */
  struct timespec pkt_timestamp; /* packet timestamp */
  struct timespec tx_timestamp, rx_timestamp;
  int ok_flag = 1;

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);
	
  /* 
   * bind: associate the parent socket with a port 
   */
  if (!ptp_msgflag)
  {
	if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) 
	{
		error("PeerTSserver: ERROR on binding");
		return -1;
	}
  }
 
  /* Setup message header */
  clientlen = sizeof(clientaddr);
  struct msghdr msg;
  struct iovec iov[1];
  if (ptp_msgflag)
  {
	iov[0].iov_base=ptp_msg;
	iov[0].iov_len=sizeof(ptp_msg->data);
	// This is the multicast address to which we send -> initially
	if (!inet_aton(node_uuid.c_str(), &mcast_addr))
	  return -1;
  }
  else
  {
	iov[0].iov_base=buf;
	iov[0].iov_len=sizeof(buf);
  }

  msg.msg_name=&clientaddr;
  msg.msg_namelen=sizeof(clientaddr);
  msg.msg_iov=iov;
  msg.msg_iovlen=1;
  msg.msg_control=(caddr_t)cmsgbuf;
  msg.msg_controllen=sizeof(cmsgbuf);
  /* 
   * main loop: wait for a datagram, then echo it
   */
  while (running) {
	/*
	 * recvfrom: receive a UDP datagram from a client
	 */
	ok_flag = 1;
	bzero(buf, BUFSIZE);
	bzero(cmsgbuf, BUFSIZE);
	if (ptp_msgflag)
	{
	  memset(ptp_msg, 0, sizeof(ptp_msg->data));
	  clientaddr.sin_addr = mcast_addr;
	  clientaddr.sin_port = htons((unsigned short)portno);
	}

	n = recvmsg(sockfd, &msg, 0);
	if (n < 0)
	{
	  if (DEBUG_FLAG)
		  printf("PeerTSserver: ERROR in recvmsg %d\n", n);
	  continue;
	}
	if (DEBUG_FLAG)
	   printf("PeerTSserver: Received message from client %s\n", inet_ntop(AF_INET, &clientaddr.sin_addr, clientname, sizeof(clientname)));
	// Implement packet filtering
	// if (ptp_msgflag)
	// {
	// 	// Check if the sent IP address exists in the exclusion set
	// 	if (exclusion_set.find(std::string(inet_ntop(AF_INET, &clientaddr.sin_addr, clientname, sizeof(clientname)))) != exclusion_set.end())
	// 		continue;
	// }

	 /* 
	  * gethostbyaddr: determine who sent the datagram
	  */
	if (DEBUG_FLAG)
	   printf("PeerTSserver: server received %d/%d bytes: %s\n", strlen(buf), n, buf);
	
	/* Get Packet Timestamp */
	n = get_rx_timestamp(&msg, offset, &rx_timestamp, ts_flag, DEBUG_FLAG);
	if (n < 0) 
	{
	  if (DEBUG_FLAG)
		  error("PeerTSserver: ERROR in getting rx timestamp");
	  ok_flag = 0;
	}
	
	/* 
	 * sendto: echo the input back to the client 
	 */
	if (ptp_msgflag)
	{
	  clientaddr_ucast = clientaddr;
	  clientaddr.sin_addr = mcast_addr;
	  clientaddr.sin_port = htons((unsigned short)portno);
	  n = sendto(sockfd, ptp_msg, sizeof(ptp_msg->data), 0, 
		 (struct sockaddr *) &clientaddr, clientlen);
	  if (n < 0) 
		  error("PeerTSserver: ERROR in sendto 1");
	}
	else
	{
		n = sendto(sockfd, buf, strlen(buf)+2, 0, 
			 (struct sockaddr *) &clientaddr, clientlen);
		if (n < 0) 
		  error("PeerTSserver: ERROR in sendto 1");
	}

	n = get_tx_timestamp(sockfd, buf, strlen(buf), NULL, MSG_ERRQUEUE, &tx_timestamp, ts_flag, DEBUG_FLAG);
	if (n < 0) 
	{
	  if (DEBUG_FLAG)
		  error("PeerTSserver: ERROR in getting tx timestamp 1");
	  ok_flag = 0;
	  error_count++;
	}
	else
	{
	  if (error_count > 0)
		 error_count--;
	}

	/*
	 * send the timestamps to the client 
	 */
	bzero(buf, BUFSIZE);
	sprintf(buf, "%ld %ld %ld %ld %d\n", (long)rx_timestamp.tv_sec, (long)rx_timestamp.tv_nsec, (long)tx_timestamp.tv_sec, (long)tx_timestamp.tv_nsec, ok_flag);
	// if (ptp_msgflag)
	// {
	// 	clientaddr = clientaddr_ucast;
	// 	clientaddr.sin_port = htons((unsigned short)portno);
	// }
	n = sendto(sockfd, buf, strlen(buf), 0, 
	(struct sockaddr *) &clientaddr, clientlen);
	if (n < 0) 
		error("PeerTSserver: ERROR in sendto 2");

	// if (!ptp_msgflag)
	// {
	  n = get_tx_timestamp(sockfd, buf, strlen(buf), NULL, MSG_ERRQUEUE, &pkt_timestamp, ts_flag, DEBUG_FLAG);
	  //if (n < 0) 
	  //{
	  //  error("PeerTSServer: ERROR in getting tx timestamp 2");
	  //  printf("%s\n", buf);
	  //}
	// }

	// Check error count and set error flag
	if (error_count > 5)
	  error_flag = 1;

  }
  printf("PeerTSserver: Timestamping thread exiting\n");
  return 0;
}
