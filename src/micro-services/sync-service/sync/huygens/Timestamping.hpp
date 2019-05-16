/**
 * @file Timestamping.hpp
 * @brief Timestamping Module Header
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
#ifndef QOT_PEER_TIMESTAMPING_HPP
#define QOT_PEER_TIMESTAMPING_HPP
 
#include <sys/socket.h>
#include <netinet/in.h>

/**
 * Try to enable kernel timestamping, otherwise fall back to software.
 *
 * Run setsockopt SO_TIMESTAMPING with SOFTWARE on socket 'sock'. 
 * 
 * \param[in] sock  The socket to activate SO_TIMESTAMPING on (the UDP)
 * \warning         Requires CONFIG_NETWORK_PHY_TIMESTAMPING Linux option
 * \warning         Requires drivers fixed with skb_tx_timestamp() 
 */
int tstamp_mode_kernel(int sock);

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
int tstamp_mode_hardware(int sock, char *iface);

/* Fetch the timestamp of a sent packet */
int get_tx_timestamp(int fd, void *buf, int buflen, struct sockaddr_in *addr, int flags, struct timespec *pkt_timestamp, int ts_flag, int debug_print);

/* Extract the timestamp of a sent packet */
int get_rx_timestamp(struct msghdr *msg, int64_t offset, struct timespec *pkt_timestamp, int ts_flag, int debug_print);

#endif