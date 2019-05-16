/**
 * @file PeerTSclient.cpp
 * @brief Peer to Peer Timestamping Echo Client to figure out "network-effect" discrepancies
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
 */
#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>

#include "PeerTSclient.hpp"
#include "Timestamping.hpp"
#include "SVMprocessor.hpp"
#include "libsvm/svm.h"

// Add header to Modern JSON C++ Library
#include "../../../../../thirdparty/json-modern-cpp/json.hpp"

// Add header to spoof PTP messages
#include "ptp_message.hpp"

#define BUFSIZE 1024

using namespace qot;

#define DEBUG_FLAG 0

#define EPSILON 50000

#define PRIMARY_MCAST_IPADDR "224.0.1.129"

#ifdef NATS_SERVICE
// Get the NATS connection status
int PeerTSclient::getNatsStatus()
{
    return (int)s;
}

// Connect to the NATS server
int PeerTSclient::natsConnect(const char* nats_url)
{
    s = natsConnection_ConnectTo(&conn, nats_url);
    if (s == NATS_OK)
    {
        std::cout << "Connected to NATS service\n";
    }
    else
    {
        std::cout << "Error Connecting to NATS service\n";
    }
    return (int)s;
}
#endif

// Constructor
PeerTSclient::PeerTSclient(const std::string &hostname, int portno, const std::string &iface, const std::string &pub_server, int ts_flag)
  : portno(portno), iface(iface), ts_flag(ts_flag), hostname(hostname), running(true), tx_period_ns(1000000000), 
    ts_buffer(NULL), proc_ts_buffer(NULL), ts_duration_ns(2000000000ULL), ts_buf_len(0),
    data_lock(PTHREAD_MUTEX_INITIALIZER), data_condvar(PTHREAD_COND_INITIALIZER), nats_server(pub_server)
{
    error_flag = 0;
    // If the port is same as PTP, set the flag
    if (portno == PTP_PORT)
    {
      ptp_msgflag = 1;
      std::cout << "PeerTSclient: choosing PTP message option\n";
    }
    else
      ptp_msgflag = 0;

    #ifdef NATS_SERVICE
    // Initialize NATS Parameters
    conn = NULL;
    msg  = NULL;
    s = NATS_ERR;

    #endif
}

// Destructor
PeerTSclient::~PeerTSclient()
{
    #ifdef NATS_SERVICE
    // Destroy the NATS connection
    // natsConnection_Destroy(conn);
    #endif
}

// Control functions
int PeerTSclient::Reset()
{
  running = false;
  // Wake the processor thread from its condition variable
  pthread_mutex_lock(&data_lock);
  pthread_cond_signal(&data_condvar);
  pthread_mutex_unlock(&data_lock);

  client_thread.join();
  processor_thread.join();
  
  // Restart Threads
  running = true;
  client_thread = boost::thread(&PeerTSclient::ts_client_loop, this);
  processor_thread = boost::thread(&PeerTSclient::proc_client_loop, this);
  return ts_flag;
}

int PeerTSclient::Start(const std::string &node_name, uint64_t period_ns)
{
  error_flag = 0;
  struct in_addr mcast_addr;
  
  if (ptp_msgflag)
  {   
      // We will be listening for messages on this address
      if (!inet_aton(hostname.c_str(), &mcast_addr))
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
      sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
      if (sockfd < 0) 
      {
          perror("PeerTSClient: ERROR opening socket");
          return -1;
      }
  }

  /* gethostbyname: get the server's DNS entry */
  server = gethostbyname(hostname.c_str());
  if (server == NULL) {
      fprintf(stderr,"PeerTSClient: ERROR, no such host as %s\n", hostname.c_str());
      return -1;
  }
  else
  {
      std::cout << "PeerTSclient: Succesfully used hostname " << hostname << "\n";
  }

  /* Set the node name */
  node_uuid = node_name;

  /* Set the transmission period */
  tx_period_ns = period_ns;

  /* Configure hardware timestamping */
  if (ts_flag == 2)
    ts_flag = tstamp_mode_hardware(sockfd, const_cast<char*>(iface.c_str()));
  else   /* Configure software timestamping */
    ts_flag = tstamp_mode_kernel(sockfd);

  /* Initialize the Buffer to hold the timestamps */
  ts_buffer = new probe_timestamps[ts_duration_ns/period_ns];
  proc_ts_buffer = new probe_timestamps[ts_duration_ns/period_ns];
  ts_buf_len = ts_duration_ns/period_ns;

  std::cout << "PeerTSclient: Tx Period = " << period_ns << " ns"
            << " Processing Duration = " << ts_duration_ns << " ns\n";

  #ifdef NATS_SERVICE
  // Connect to NATS Service
  std::cout << "PeerTSclient: Connecting to NATS server on " << nats_server << "\n";
  natsConnect(nats_server.c_str());
  #endif

  // Spawn the server thread
  running = true;
  client_thread = boost::thread(&PeerTSclient::ts_client_loop, this);
  processor_thread = boost::thread(&PeerTSclient::proc_client_loop, this);
  return ts_flag;
}

int PeerTSclient::Stop()
{
  running = false;
  // Wake the processor thread from its condition variable
  pthread_mutex_lock(&data_lock);
  pthread_cond_signal(&data_condvar);
  pthread_mutex_unlock(&data_lock);

  client_thread.join();
  processor_thread.join();
  delete ts_buffer;
  delete proc_ts_buffer;
  ts_buffer = NULL;
  error_flag = 0;
  #ifdef NATS_SERVICE
  // Destroy the NATS connection
  std::cout << "PeerTSclient: destroying nats connection\n";
  if (s == NATS_OK)
  {
    natsConnection_Destroy(conn);
  }
  #endif
  return 0;
}

// Function to check error status
bool PeerTSclient::GetErrorStatus()
{
  return error_flag;
}

// Function to start a processing loop which processes the timestamps
int PeerTSclient::proc_client_loop()
{
    int64_t rtt_peerdelay_ns, offset_ns;
    std::vector<int64_t> peer_offset_bounds(ts_buf_len*2); // Alternate upper and lower bounds
    std::vector<int64_t> instant(ts_buf_len);
    int vec_len = 0, vec_ctr = 0, data_ctr = 0;
    double offset, drift;

    while (running) 
    {
      pthread_mutex_lock(&data_lock);

      // Check if a new batch of data has arrived
      pthread_cond_wait(&data_condvar, &data_lock);

      if (!running)
      {
          pthread_mutex_unlock(&data_lock);
          break;
      }

      // Process the batch
      if (DEBUG_FLAG)
        std::cout << "PeerTSclient: New batch of data received\n";

      // Unwind the data
      vec_len = 0;
      vec_ctr = 0;
      data_ctr = 0;
      int64_t start_time = proc_ts_buffer[0].rx[0];
      for (int i=0; i < ts_buf_len-1; i++)
      {
        struct probe_timestamps timestamps = proc_ts_buffer[i];

        // Check coded probes
        if (timestamps.validity_flag == 1)
        {
            uint64_t delta = (uint64_t) abs((timestamps.rx_remote[1] - timestamps.rx_remote[0]) - (timestamps.tx[1] - timestamps.tx[0]));
            uint64_t epsilon = EPSILON;
            data_ctr++;
            // std::cout << "Delta = " << delta << "\n";
            // printf("PeerTSClient: Remote Timestamps: %lld %lld\n", timestamps.rx_remote[1], timestamps.rx_remote[0]);
            // printf("PeerTSClient: Local  Timestamps: %lld %lld\n", timestamps.tx[1], timestamps.tx[0]);
            if (delta < epsilon)
            {
                // Calculate the necessary quantities
                rtt_peerdelay_ns = (timestamps.rx[0] - timestamps.tx[0]) - (timestamps.tx_remote[0] - timestamps.rx_remote[0]);
                offset_ns = ((timestamps.rx_remote[0] - timestamps.tx[0]) + (timestamps.tx_remote[0] - timestamps.rx[0]))/2;
                peer_offset_bounds[vec_ctr] = timestamps.rx_remote[0] - timestamps.tx[0];   // uper bound
                peer_offset_bounds[vec_ctr+1] = timestamps.tx_remote[0] - timestamps.rx[0]; // lower bound
                instant[vec_len] = timestamps.rx[0] - start_time;
                vec_len++;
                vec_ctr = vec_ctr + 2;
            }
        }
      }
      pthread_mutex_unlock(&data_lock);
      if (DEBUG_FLAG)
        std::cout << "PeerTSclient: Valid data received is " << data_ctr << "\n";
      if (vec_len > 0)
      {
          if (DEBUG_FLAG)
            std::cout << "PeerTSclient: Formulating problem with vec_len " << vec_len << "\n";
          formulate_problem(peer_offset_bounds, instant, vec_len);

          if (DEBUG_FLAG)
            std::cout << "PeerTSclient: Running SVM\n";
          run_svm(offset, drift);
          // std::cout << "PeerTSclient: SVM completed\n";
          #ifdef NATS_SERVICE
          // Publish the message to NATS
          if (s == NATS_OK)
          {
            msg = NULL;
            // Convert the params to json
            nlohmann::json params;
            params["client"] = node_uuid;
            params["server"] = hostname;
            params["offset"] = offset;
            params["drift"] = drift;
            params["start_time"] = start_time;
            std::string data = params.dump();

            // Construct the topic name
            std::string nats_subject = "qot.peer.params";
            // nats_subject.append(std::string("params"));

            s = natsMsg_Create(&msg, nats_subject.c_str(), NULL, data.c_str(), data.length());
            if (s == NATS_OK)
            {
                natsConnection_PublishMsg(conn, msg);
                // std::cout << "PeerTSclient: Published Message to NATS on topic " << nats_subject << "\n";
            }
            natsMsg_Destroy(msg);
          }
          #endif
      }
      else
      {
          std::cout << "PeerTSclient: SVM cannot be run as input length is zero\n";
          // Restart the Client -> set error flag
          error_flag = 1;
      }
    }
    std::cout << "PeerTSclient: Processor loop thread exiting\n";
    return 0;
}

// Function to start a client which sends packets to other servers
int PeerTSclient::ts_client_loop()
{
    int n;
    int64_t rtt_peerdelay_ns, offset_ns;
    socklen_t serverlen;
    struct sockaddr_in serveraddr;
    char buf[BUFSIZE];
    char cmsgbuf[BUFSIZE]; /* ancillary info buf */
    struct timespec pkt_timestamp; /* packet timestamp */
    struct timespec recv_timeout; /* receive message timeout */
    struct timespec tx_period; /* period */
    struct timeval recv_timeout_tv;
    struct timespec now, next_wakeup;
    uint64_t now_ns, next_wakeup_ns;
    int debug_flag = DEBUG_FLAG;
    struct ptp_message *ptp_msg = msg_allocate();
    int buflen;

    /* 4 Timestamps to calculate offset and round-trip time */
    struct timespec tx_timestamp, rx_timestamp, rx_timestamp_remote, tx_timestamp_remote;
    struct probe_timestamps timestamps;
    int64_t peer_offset_up, peer_offset_low;

    struct in_addr mcast_addr;
    if (ptp_msgflag)
    {
        // This is the multicast address to which we send
        if (!inet_aton(hostname.c_str(), &mcast_addr))
          return -1;
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    // Use a multicast address for PTP like messaging
    if (ptp_msgflag)
      serveraddr.sin_addr = mcast_addr;
    else
      bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* Test message to send */
    bzero(buf, BUFSIZE);
    sprintf(buf, "test");

    /* Set the timeout time to 1 second */
    recv_timeout.tv_sec = 1;
    recv_timeout.tv_nsec = 0;
    recv_timeout_tv.tv_sec = 1;
    recv_timeout_tv.tv_usec = 0;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout_tv, sizeof(recv_timeout_tv)) < 0)
        perror("PeerTSClient: ERROR in setting socket options to SO_RCVTIMEO");

    /* Set the transmission period */
    tx_period.tv_sec = tx_period_ns/1000000000ULL;
    tx_period.tv_nsec = tx_period_ns % 1000000000ULL;

    /* Setup message header */
    struct mmsghdr msg_vec;
    struct msghdr msg;
    struct iovec iov[1];
    iov[0].iov_base=buf;
    iov[0].iov_len=sizeof(buf);

    msg.msg_name=&serveraddr;
    msg.msg_namelen=sizeof(serveraddr);
    msg.msg_iov=iov;
    msg.msg_iovlen=1;
    msg.msg_control=(caddr_t)cmsgbuf;
    msg.msg_controllen=sizeof(cmsgbuf);
    msg_vec.msg_hdr = msg;

    /* Override debug flag if the period is too small */
    if (tx_period_ns < 500000000)
        debug_flag = 0;
    else
        debug_flag = 1;

    /* Output debug file */
    std::ofstream outfile;
    outfile.open ("example.txt");

    /* 
     * main loop: keep periodically sending messages, the wait for the echo
     */
    int ok_flag = 1; // Indicates everything is ok
    int remote_ok_flag = 1;
    int counter = 0; // counter to identify messages and handle message drops
    int recv_counter = 0;
    int buffer_counter = 0;
    while (running) { 
        /* Periodic wakeup to send */
        clock_gettime(CLOCK_REALTIME, &now);
        now_ns = now.tv_sec*1000000000ULL + now.tv_nsec;
        next_wakeup_ns = ((now_ns/tx_period_ns) + 1)*tx_period_ns;
        next_wakeup.tv_sec = next_wakeup_ns/1000000000ULL;
        next_wakeup.tv_nsec = next_wakeup_ns % 1000000000ULL;
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_wakeup, NULL);

        /* Implement coded probes */
        ok_flag = 1;
        for (int i=0; i < 2; i ++)
        {
            /* Increment message counter */
            counter = (counter + 1) % 255;
            if (ptp_msgflag)
            {
              serveraddr.sin_addr = mcast_addr;
              serveraddr.sin_port = htons(portno);
              populate_dummy_ptp_msg(ptp_msg, (uint16_t) counter, iface.c_str());
            }

            /* send the message to the server */
            sprintf(buf, "%d", counter);
            serverlen = sizeof(serveraddr);
            
            // Send a PTP message if the flag is enabled
            if (ptp_msgflag)
            {
              n = sendto(sockfd, ptp_msg, get_dummy_msg_len(ptp_msg), 0, (struct sockaddr*)&serveraddr, serverlen);
              buflen  = get_dummy_msg_len(ptp_msg);
            }
            else // Send a normal unicast message
            {
              n = sendto(sockfd, buf, strlen(buf)+2, 0, (struct sockaddr*)&serveraddr, serverlen);
              buflen = strlen(buf);
            }
            if (n < 0) 
            {
              perror("PeerTSClient: ERROR in sendto");
              ok_flag = 0;
              break;
            }

            // n = get_tx_timestamp(sockfd, buf, strlen(buf), NULL, MSG_ERRQUEUE, &tx_timestamp, ts_flag, DEBUG_FLAG);
            n = get_tx_timestamp(sockfd, buf, buflen, NULL, MSG_ERRQUEUE, &tx_timestamp, ts_flag, DEBUG_FLAG);
            if (n < 0) 
            {
              if (DEBUG_FLAG)
                printf("PeerTSClient: ERROR getting tx packet timestamp\n");
              ok_flag = 0;
            }

            /* print the server's reply */
            bzero(buf, BUFSIZE);
            bzero(cmsgbuf, BUFSIZE);
            n = recvmsg(sockfd, &msg_vec.msg_hdr, 0);
            if (n < 0)
            {
              if (DEBUG_FLAG)
                printf("PeerTSClient: ERROR in recvmsg %d\n", n);
              ok_flag = 0;
              break;
            }
            else
            {
              /* Get Packet Timestamp */
              n = get_rx_timestamp(&msg_vec.msg_hdr, 0, &rx_timestamp, ts_flag, DEBUG_FLAG);
              if (n < 0) 
              {
                  printf("PeerTSClient: ERROR getting rx packet timestamp\n");
                  ok_flag = 0;
              }

              // This needs to be changed, checks need to be added for PTP-like messages
              if (!ptp_msgflag)
              {
                sscanf (buf, "%d", &recv_counter);
                if (recv_counter != counter)
                {
                  printf("PeerTSClient: Received Incorrect Packet ctr: %d, recv_ctr: %d\n", counter, recv_counter);
                }
              }
            }

            /* Receive remote timestamp data */
            bzero(buf, BUFSIZE);
            bzero(cmsgbuf, BUFSIZE);
            n = recvmsg(sockfd, &msg, 0);
            if (n < 0)
            {
              perror("PeerTSClient: ERROR in receiving remote timestamps");
              ok_flag = 0;
              break;
            }
            sscanf (buf, "%ld %ld %ld %ld %d", &rx_timestamp_remote.tv_sec, &rx_timestamp_remote.tv_nsec, &tx_timestamp_remote.tv_sec, &tx_timestamp_remote.tv_nsec, &remote_ok_flag);
            
            if (remote_ok_flag == 0)
            {
                if (DEBUG_FLAG)
                    printf("PeerTSClient: ERROR in packet timestamping on server side\n");
                ok_flag = remote_ok_flag;
                break;
            }

            /* Extract the timestamps */
            timestamps.rx[i] = rx_timestamp.tv_sec*1000000000LL + rx_timestamp.tv_nsec;
            timestamps.tx[i] = tx_timestamp.tv_sec*1000000000LL + tx_timestamp.tv_nsec;
            timestamps.rx_remote[i] = rx_timestamp_remote.tv_sec*1000000000LL + rx_timestamp_remote.tv_nsec;
            timestamps.tx_remote[i] = tx_timestamp_remote.tv_sec*1000000000LL + tx_timestamp_remote.tv_nsec;
        }

        if (DEBUG_FLAG)
        {
            printf("PeerTSClient: Remote Timestamps: %lld %lld\n", timestamps.rx_remote[0], timestamps.tx_remote[0]);
            printf("PeerTSClient: Local  Timestamps: %lld %lld\n", timestamps.rx[0], timestamps.tx[0]);
        }

        if (DEBUG_FLAG)
        {
            outfile << timestamps.rx[0] << "," << timestamps.tx[0] << "," << timestamps.rx_remote[0] << "," << timestamps.tx_remote[0] << "\n";
            outfile << timestamps.rx[1] << "," << timestamps.tx[1] << "," << timestamps.rx_remote[1] << "," << timestamps.tx_remote[1] << "\n";
        }

        /* Calculate Round-Trip Time and Offset */
        rtt_peerdelay_ns = (timestamps.rx[0] - timestamps.tx[0]) - (timestamps.tx_remote[0] - timestamps.rx_remote[0]);
        offset_ns = ((timestamps.rx_remote[0] - timestamps.tx[0]) + (timestamps.tx_remote[0] - timestamps.rx[0]))/2;
        peer_offset_up = timestamps.rx_remote[0] - timestamps.tx[0];
        peer_offset_low = timestamps.tx_remote[0] - timestamps.rx[0];

        /* Enter the value into the buffer */
        timestamps.validity_flag = ok_flag;
        ts_buffer[buffer_counter] = timestamps;
        buffer_counter = (buffer_counter + 1) % ts_buf_len;

        /* Copy data from buffer */
        if (buffer_counter == ts_buf_len - 1)
        {
            pthread_mutex_lock(&data_lock);
            memcpy((void*) proc_ts_buffer, (void*) ts_buffer, ts_buf_len*sizeof(probe_timestamps));
            
            // Signal the processing thread that a new batch has been added
            pthread_cond_signal(&data_condvar);
            pthread_mutex_unlock(&data_lock);
        }

        if (debug_flag)
        {
            printf("[%d] PeerTSClient: RTT and Offset in nanoseconds  : %lld %lld\n", counter, rtt_peerdelay_ns, offset_ns);
            printf("[%d] PeerTSClient: OffUp and OffLow in nanoseconds: %lld %lld\n", counter, peer_offset_up, peer_offset_low);
        }
    }

    std::cout << "PeerTSclient: Timestamping loop thread exiting\n";

    outfile.close();
    return 0;
}
