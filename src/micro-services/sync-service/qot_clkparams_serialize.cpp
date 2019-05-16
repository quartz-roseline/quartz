/*
 * @file qot_clkparams_serialize.cpp
 * @brief Library to serialize the timeline clock parameters to json
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

/* Only build if the timeline service is being built */
#include <iostream>
#include <string>

// Header 
#include "qot_clkparams_serialize.hpp"

// Serialize Timeline Clock Parameters
json serialize_clkparams(tl_translation_t &clk_params)
{
	json j;

	/* Formulate Clock Params as JSON */
	j["last"] = clk_params.last;
	j["mult"] = clk_params.mult;
	j["nsec"] = clk_params.nsec;
	j["u_nsec"] = clk_params.u_nsec;
	j["l_nsec"] = clk_params.l_nsec;
	j["u_mult"] = clk_params.u_mult;
	j["l_mult"] = clk_params.l_mult;

	return j;
}

// Deserialize Timeline Clock Parameters
void deserialize_clkparams(json &data, tl_translation_t &clk_params)
{
	// Get the clock params
	clk_params.last = data["last"].get<int64_t>();
	clk_params.mult = data["mult"].get<int64_t>();
	clk_params.nsec = data["nsec"].get<int64_t>();
	clk_params.u_nsec = data["u_nsec"].get<int64_t>();
	clk_params.l_nsec = data["l_nsec"].get<int64_t>();
	clk_params.u_mult = data["u_mult"].get<int64_t>();
	clk_params.l_mult = data["l_mult"].get<int64_t>();

	return;
}



