/**
 * @file NTP18.hpp
 * @brief Provides header for ntp instance based on Chrony to the sync interface
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND f
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
 * Reference: Based on the chrony implementation of NTP
 * 1. chrony: https://chrony.tuxfamily.org/
 */

#ifndef NTP18_HPP
#define NTP18_HPP

// Boost includes
#include <boost/asio.hpp>
#include <boost/thread.hpp> 
#include <boost/log/trivial.hpp>

#include "../Sync.hpp"
#include "../SyncUncertainty.hpp"
#include "../qot_tlcomm.hpp"

/* Linuxptp includes */
extern "C"
{
	// Standard includes
	#include <limits.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <unistd.h>
	#include <linux/net_tstamp.h>
	#include <sys/mman.h>

	// chrony includes
	#include "chrony-3.2/config.h"
	#include "chrony-3.2/sysincl.h"
	#include "chrony-3.2/main.h"
	#include "chrony-3.2/sched.h"
	#include "chrony-3.2/local.h"
	#include "chrony-3.2/sys.h"
	#include "chrony-3.2/ntp_io.h"
	#include "chrony-3.2/ntp_signd.h"
	#include "chrony-3.2/ntp_sources.h"
	#include "chrony-3.2/ntp_core.h"
	#include "chrony-3.2/sources.h"
	#include "chrony-3.2/sourcestats.h"
	#include "chrony-3.2/reference.h"
	#include "chrony-3.2/logging.h"
	#include "chrony-3.2/conf.h"
	#include "chrony-3.2/cmdmon.h"
	#include "chrony-3.2/keys.h"
	#include "chrony-3.2/manual.h"
	#include "chrony-3.2/rtc.h"
	#include "chrony-3.2/refclock.h"
	#include "chrony-3.2/clientlog.h"
	#include "chrony-3.2/nameserv.h"
	#include "chrony-3.2/privops.h"
	#include "chrony-3.2/smooth.h"
	#include "chrony-3.2/tempcomp.h"
	#include "chrony-3.2/util.h"

	// Include to share data from ntp sync to uncertainty calculation
	#include "uncertainty_data.h"
}

namespace qot
{
	class NTP18 : public Sync
	{

		// Constructor and destructor
		public: NTP18(boost::asio::io_service *io, const std::string &iface, struct uncertainty_params config);
		public: ~NTP18();

		// Control functions
		public: void Reset();
		public: void Start(bool master, int log_sync_interval, uint32_t sync_session, int timelineid, int *timelinesfd, const std::string &tl_name, std::string &node_name, uint16_t timelines_size);
		public: void Stop();

		// Multi-function function to do stuff based on type input
		public: int ExtControl(void** pointer, ExtCtrlOptions type);

		// This thread performs the actual syncrhonization
		private: int SyncThread(int timelineid, int *timelinesfd, uint16_t timelines_size);

		// This thread computes the synchronization uncertainty
		private: int UncertaintyThread(int timelineid, int *timelinesfd, uint16_t timelines_size);

		// This thread computes the synchronization uncertainty between the PHC and CLK_REALTIME
		private: int LocalUncertaintyThread(int timelineid, int *timelinesfd, uint16_t timelines_size);

		// Boost ASIO
		private: boost::asio::io_service *asio;
		private: boost::thread sync_thread;
		private: boost::thread uncertainty_thread;
		private: boost::thread loc_uncertainty_thread;
		private: std::string baseiface;
		private: bool kill;
		private: bool status_flag; // Indicates if the sync is running or not 

		// Sync Uncertainty Calculation Class
		private: SyncUncertainty sync_uncertainty;

		// Local Timeline (CLKRT->PHC) Sync Uncertainty Class
		private: SyncUncertainty loc_sync_uncertainty;

		// Last Received Clock-Sync Skew Statistic Data Point
        private: qot_stat_t last_clocksync_data_point;
    	private: qot_stat_t last_clkrtphc_data_point;

        // Timeline Name
    	private: std::string timeline_uuid; 

        #ifdef QOT_TIMELINE_SERVICE
        // Communicator class with the timeline service
    	private: TLCommunicator comm;
    	// Pointer to the timeline clock shared memory (global)
    	private: tl_translation_t* tl_clk_params;

    	// Pointer to the timeline clock shared memory (local)
    	private: tl_translation_t* local_tl_clk_params;

    	// NATS Service Server
    	private: std::string nats_server;

        #endif
	};
}

#endif
