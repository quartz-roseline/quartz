/**
 * @file Timestamping.cpp
 * @brief Peer Messaging Timestamping Module
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

// Define this flag if not defined
#ifndef SO_SELECT_ERR_QUEUE
#define SO_SELECT_ERR_QUEUE 45
#endif

/**
 * Try to enable kernel timestamping, otherwise fall back to software.
 *
 * Run setsockopt SO_TIMESTAMPING with SOFTWARE on socket 'sock'. 
 * 
 * \param[in] sock  The socket to activate SO_TIMESTAMPING on (the UDP)
 * \warning         Requires CONFIG_NETWORK_PHY_TIMESTAMPING Linux option
 * \warning         Requires drivers fixed with skb_tx_timestamp() 
 */
int tstamp_mode_kernel(int sock) {
  int f = 0; /* flags to setsockopt for socket request */
  socklen_t slen;
  
  slen = (socklen_t)sizeof f;
  f |= SOF_TIMESTAMPING_TX_SOFTWARE;
  f |= SOF_TIMESTAMPING_RX_SOFTWARE;
  f |= SOF_TIMESTAMPING_SOFTWARE;
  /* Enable Timestamping */
  if (setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING, &f, slen) < 0) {
    printf("SO_TIMESTAMPING not possible\n");
    return -1;
  }

  /* Enable users to fetch TX timestamps */
  f = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_SELECT_ERR_QUEUE,&f, slen) < 0) {
    printf("SO_SELECT_ERR_QUEUE: %m\n");
    // sk_events = 0;
    // sk_revents = POLLERR;
  }
  printf("Using kernel timestamps\n");
  return 0;
}

/**
 * Try to enable hardware timestamping, otherwise fall back to kernel.
 *
 * Run ioctl() on 'iface' (supports Intel 82580 right now) and run
 * setsockopt SO_TIMESTAMPING with RAW_HARDWARE on socket 'sock'. 
 * 
 * \param[in] sock  The socket to activate SO_TIMESTAMPING on (the UDP)
 * \param[in] iface The interface name to ioctl SIOCSHWTSTAMP on
 * \warning         Requires CONFIG_NETWORK_PHY_TIMESTAMPING Linux option
 * \warning         Supports only Intel 82580 right now
 * \bug             Intel 82580 has bugs such as IPv4 only, and RX issues
 */
int tstamp_mode_hardware(int sock, char *iface) {
  struct ifreq dev; /* request to ioctl */
  struct hwtstamp_config hwcfg, req; /* hw tstamp cfg to ioctl req */
  int f = 0; /* flags to setsockopt for socket request */
  socklen_t slen;
  
  slen = (socklen_t)sizeof f;
  /* STEP 1: ENABLE HW TIMESTAMP ON IFACE IN IOCTL */
  memset(&dev, 0, sizeof dev);
  /*@ -mayaliasunique Trust me, iface and dev doesn't share storage */
  strncpy(dev.ifr_name, iface, sizeof dev.ifr_name);
  /*@ +mayaliasunique */
  /*@ -immediatetrans Yes, we might overwrite ifr_data */
  dev.ifr_data = (char *)&hwcfg;
  memset(&hwcfg, 0, sizeof hwcfg); 
  /*@ +immediatetrans */
  /* enable tx hw tstamp, ptp style, intel 82580 limit */
  hwcfg.tx_type = HWTSTAMP_TX_ON; 
  /* enable rx hw tstamp, all packets, yey! */
  hwcfg.rx_filter = HWTSTAMP_FILTER_ALL;
  req = hwcfg;
    /* Check that one is root */
  if (getuid() != 0)
    printf("Hardware timestamps requires root privileges\n");
  /* apply by sending to ioctl */
  if (ioctl(sock, SIOCSHWTSTAMP, &dev) < 0) {
    printf("ioctl: SIOCSHWTSTAMP: error\n");
    printf("Trying again with HWTSTAMP_FILTER_PTP_V2_EVENT option\n");
    /* Try again with the PTP timestamping flag */
    hwcfg.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT; 
    req = hwcfg;
    if (ioctl(sock, SIOCSHWTSTAMP, &dev) < 0) {
      printf("ioctl: SIOCSHWTSTAMP: error\n");
      printf("Verify that %s supports hardware timestamp\n",iface);
      /* otherwise, try kernel timestamps (socket only) */ 
      printf("Falling back to kernel timestamps\n");
      int retval = tstamp_mode_kernel(sock);
      return retval;
    }
  }

  if (memcmp(&hwcfg, &req, sizeof(hwcfg))) {
    printf("driver changed our HWTSTAMP options\n");
    printf("tx_type   %d not %d\n", hwcfg.tx_type, req.tx_type);
    printf("rx_filter %d not %d\n", hwcfg.rx_filter, req.rx_filter);
    return -1;
  }
  /* STEP 2: ENABLE NANOSEC TIMESTAMPING ON SOCKET */
  f |= SOF_TIMESTAMPING_TX_SOFTWARE;
  f |= SOF_TIMESTAMPING_TX_HARDWARE;
  f |= SOF_TIMESTAMPING_RX_SOFTWARE;
  f |= SOF_TIMESTAMPING_RX_HARDWARE;
  f |= SOF_TIMESTAMPING_RAW_HARDWARE;
  if (setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING, &f, slen) < 0) {
    /* bail to userland timestamps (socket only) */ 
    printf("SO_TIMESTAMPING: error\n");
    return -1;
  }

  /* Enable users to fetch TX timestamps */
  f = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_SELECT_ERR_QUEUE,&f, slen) < 0) {
    printf("SO_SELECT_ERR_QUEUE: %m\n");
    // sk_events = 0;
    // sk_revents = POLLERR;
  }
  printf("Using hardware timestamps\n");
  return 2;
}

/* Fetch the timestamp of a sent packet */
int get_tx_timestamp(int fd, void *buf, int buflen, struct sockaddr_in *addr, int flags, struct timespec *pkt_timestamp, int ts_flag, int debug_print)
{
  char control[256];
  int cnt = 0, res = 0, level, type;
  struct cmsghdr *cm;
  unsigned char junk[1600];
  // struct iovec iov = { buf, buflen };
  struct iovec iov = { junk, sizeof(junk) };
  struct msghdr msg;
  struct timespec *sw, *ts = NULL;

  memset(control, 0, sizeof(control));
  memset(&msg, 0, sizeof(msg));
  if (addr) {
    msg.msg_name = addr;
    msg.msg_namelen = sizeof(*addr);
  }
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control;
  msg.msg_controllen = sizeof(control);

  if (flags == MSG_ERRQUEUE) {
    struct pollfd pfd = { fd, POLLERR, 0}; //POLLPRI, 0 };
    res = poll(&pfd, 1, 1);
    if (res < 1) {
      if (debug_print)
      {
        printf(res ? "poll for tx timestamp failed: %m\n" :
                     "timed out while polling for tx timestamp\n");
        printf("increasing tx_timestamp_timeout may correct "
               "this issue, but it is likely caused by a driver bug\n");
      }
      return res;
    } else if (!(pfd.revents & POLLERR)) {//POLLPRI)) {
      printf("poll for tx timestamp woke up on non ERR event\n");
      return -1;
    }
  }

  cnt = recvmsg(fd, &msg, flags);
  if (cnt < 1)
    printf("recvmsg%sfailed: %m\n",
           flags == MSG_ERRQUEUE ? " tx timestamp " : " ");

  int timestamp_flag = 0;

  for (cm = CMSG_FIRSTHDR(&msg); cm != NULL; cm = CMSG_NXTHDR(&msg, cm)) {
    level = cm->cmsg_level;
    type  = cm->cmsg_type;
    if (SOL_SOCKET == level && SO_TIMESTAMPING == type) {
      if (cm->cmsg_len < sizeof(*ts) * 3) {
        printf("short SO_TIMESTAMPING message\n");
        return -1;
      }
      ts = (struct timespec *) CMSG_DATA(cm);
      timestamp_flag = 1;
    }
    if (SOL_SOCKET == level && SO_TIMESTAMPNS == type) {
      if (cm->cmsg_len < sizeof(*sw)) {
        printf("short SO_TIMESTAMPNS message\n");
        return -1;
      }
      sw = (struct timespec *) CMSG_DATA(cm);
      timestamp_flag = 1;
    }
  }

  // Copy timestamp
  if (timestamp_flag)
  {
    if (ts_flag == 0)
      *pkt_timestamp = ts[0]; // SW Timestamp
    else if (ts_flag == 1)
      *pkt_timestamp = ts[1]; // HW Timestamp translated to system time
    else
      *pkt_timestamp = ts[2]; // HW Timestamp
  }
  else
  {
    memset(pkt_timestamp, 0, sizeof(struct timespec));
    if (debug_print)
      printf("no timestamp found ! \n");
    return -1;
  }

  if (debug_print)
  {
    if (ts_flag == 0)
      printf("TX SW TIMESTAMP     %ld.%09ld\n", (long)ts[0].tv_sec, (long)ts[0].tv_nsec);
    if (ts_flag == 1)
      printf("TX HWX TIMESTAMP     %ld.%09ld\n", (long)ts[1].tv_sec, (long)ts[1].tv_nsec);
    if (ts_flag == 2)
      printf("TX HW TIMESTAMP     %ld.%09ld\n", (long)ts[2].tv_sec, (long)ts[2].tv_nsec);
  }

  return 0;
}

/* Extract the timestamp of a recvd packet */
int get_rx_timestamp(struct msghdr *msg, int64_t offset, struct timespec *pkt_timestamp, int ts_flag, int debug_print)
{
  int level, type;
  struct cmsghdr *cm;
  struct timespec *ts = NULL;
  int timestamp_flag = 0;
  for (cm = CMSG_FIRSTHDR(msg); cm != NULL; cm = CMSG_NXTHDR(msg, cm))
  {
      level = cm->cmsg_level;
      type  = cm->cmsg_type;
      if (SOL_SOCKET == level && SO_TIMESTAMPING == type) {
         ts = (struct timespec *) CMSG_DATA(cm);
    
         timestamp_flag = 1;
         if (debug_print)
         {
           if (ts_flag == 2)
             printf("RX HW TIMESTAMP        %ld.%09ld\n", (long)ts[2].tv_sec, (long)ts[2].tv_nsec);
           if (ts_flag == 1)
             printf("RX HWX TIMESTAMP       %ld.%09ld\n", (long)ts[1].tv_sec, (long)ts[1].tv_nsec);
           if (ts_flag == 0)
             printf("RX SW TIMESTAMP        %ld.%09ld\n", (long)ts[0].tv_sec, (long)ts[0].tv_nsec);
        }
      }
  }

  if (!timestamp_flag)
  {
    memset(pkt_timestamp, 0, sizeof(struct timespec));
    printf("ERROR in getting rx Timestamp\n");
    return -1;
  }

  // Copy timestamp
  if (ts_flag == 0)
    *pkt_timestamp = ts[0]; // SW Timestamp (RX)
  else if (ts_flag == 1)
    *pkt_timestamp = ts[1]; // HW Timestamp translated to system time (RX)
  else
    *pkt_timestamp = ts[2]; // HW Timestamp (RX)

  return 0;
}
