/**
 * @file ptp_message.cpp
 * @brief Shape a PTP-like message to fool a BBB-like platform to provide a hw timestamp 
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
#include "ptp_message.hpp"

extern "C"
{
  #include "../ptp/linuxptp-1.8/util.h"
  #include <malloc.h>
}

// Private function to bind to a multicast fd
static int mcast_bind(int fd, int index)
{
  int err;
  struct ip_mreqn req;
  memset(&req, 0, sizeof(req));
  req.imr_ifindex = index;
  err = setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &req, sizeof(req));
  if (err) {
    perror("setsockopt IP_MULTICAST_IF failed: %m");
    return -1;
  }
  return 0;
}

// Private function to join to a multicast fd
static int mcast_join(int fd, int index, const struct sockaddr *grp,
          socklen_t grplen)
{
  int err, off = 0;
  struct ip_mreqn req;
  struct sockaddr_in *sa = (struct sockaddr_in *) grp;

  memset(&req, 0, sizeof(req));
  memcpy(&req.imr_multiaddr, &sa->sin_addr, sizeof(struct in_addr));
  req.imr_ifindex = index;
  err = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &req, sizeof(req));
  if (err) {
    perror("setsockopt IP_ADD_MEMBERSHIP failed: %m");
    return -1;
  }
  err = setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &off, sizeof(off));
  if (err) {
    perror("setsockopt IP_MULTICAST_LOOP failed: %m");
    return -1;
  }
  return 0;
}

// Open a PTP Like Socket (with Multicast Bind and Join)
int open_ptp_socket(const char *name, struct in_addr mc_addr, short port, int ttl)
{
  struct sockaddr_in addr;
  int fd, index, on = 1;

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  // addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_addr = mc_addr;
  addr.sin_port = htons(port);

  fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fd < 0) {
    perror("socket failed: %m");
    goto no_socket;
  }
  index = sk_interface_index(fd, name);
  if (index < 0)
    goto no_option;

  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) {
    perror("setsockopt SO_REUSEADDR failed: %m");
    goto no_option;
  }
  if (bind(fd, (struct sockaddr *) &addr, sizeof(addr))) {
    perror("bind failed: %m");
    goto no_option;
  }
  if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, name, strlen(name))) {
    perror("setsockopt SO_BINDTODEVICE failed: %m");
    goto no_option;
  }
  if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl))) {
    perror("setsockopt IP_MULTICAST_TTL failed: %m");
    goto no_option;
  }
  addr.sin_addr = mc_addr;
  if (mcast_join(fd, index, (struct sockaddr *) &addr, sizeof(addr))) {
    perror("mcast_join failed");
    goto no_option;
  }
 
  if (mcast_bind(fd, index)) {
    goto no_option;
  }
  return fd;
no_option:
  close(fd);
no_socket:
  return -1;
}

// Populate a Dummy PTP-like Message
void populate_dummy_ptp_msg(struct ptp_message *msg, uint16_t sequence_id, const char *iface_name)
{
  struct address address;
  struct ClockIdentity clk_id;

  sk_interface_addr(iface_name, AF_INET, &address);
  generate_clock_identity(&clk_id, iface_name);

  msg->hwts.type = TS_HARDWARE;
  msg->header.tsmt               = 0x1 | 0x0;
  msg->header.ver                = 2;
  msg->header.messageLength      = sizeof(struct delay_req_msg);
  msg->header.domainNumber       = 0;
  msg->header.correction         = 0;
  msg->header.sourcePortIdentity.clockIdentity = clk_id;
  msg->header.sourcePortIdentity.portNumber = 1280;
  msg->header.sequenceId         = (UInteger16) sequence_id;
  msg->header.control            = CTL_DELAY_REQ;
  msg->header.logMessageInterval = 0x7f;
  
  msg_pre_send(msg);
  return;
}

// Get the Length of a PTP message
int get_dummy_msg_len(struct ptp_message *msg)
{
  int len = ntohs(msg->header.messageLength);
  return len;
}

