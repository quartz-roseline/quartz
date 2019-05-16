/**
 * @file SVMprocessor.cpp
 * @brief SVM based Timestamp Processor to calculate offset and drift
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
#include <vector>
#include <fstream>
#include <cmath>
#include <iostream>
#include "SVMprocessor.hpp"

extern "C" 
{
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <ctype.h>
	#include <errno.h>
	#include "libsvm/svm.h"
}
#define DEBUG_FLAG 0
#define Malloc(type,n) (type *)malloc((n)*sizeof(type))

void print_null(const char *s) {}


// Global Variables
struct svm_parameter param;		// set by parse_command_line
struct svm_problem prob;		// set by read_problem
struct svm_model *model;

// Data scaling parameters
int64_t peer_offset_bounds_mean, instant_mean;
double peer_offset_bounds_sd, instant_sd;

// Private function to compute the primal (hyperplane equation)
int compute_primal(struct svm_model *model, std::vector<double> &primal, double &b)
{
	const double * const *sv_coef = model->sv_coef;	// Suppoet vector coefficients
	const svm_node * const *SV = model->SV;			// Support vectors
	int nr_class = model->nr_class; 				// number of classes
	int l = model->l; 								// number of support vectors

	// Primal Initialization
	for (int j = 0; j < primal.size(); j++)
	{
		primal[j] = 0;
	}
		
	for (int j = 0; j < primal.size(); j++)
	{
		for(int i = 0; i<l; i++)
		{
			primal[j] += SV[i][j].value*sv_coef[0][i];
		}
	}
	b = -model->rho[0];

	return 0;	
}

// Function which runs the SVM on the data -> the problem should be formulated before this step
int run_svm(double &offset, double &drift)
{
	const char *error_msg;

	std::vector<double> primal(2); // w hyperplane parmeters
	double b; // hyperplane constant wx -b = 0

	// Parameters -> Need to be checked
	param.svm_type = C_SVC;
	param.kernel_type = LINEAR;//RBF;//LINEAR;
	param.degree = 3;
	param.gamma = 0;	// 1/num_features
	param.coef0 = 0;
	param.nu = 0.5;
	param.cache_size = 100;
	param.C = 0.1;
	param.eps = 1e-3;
	param.p = 0.1;
	param.shrinking = 1;
	param.probability = 0;
	param.nr_weight = 0;
	param.weight_label = NULL;
	param.weight = NULL;

	error_msg = svm_check_parameter(&prob,&param);

	if(error_msg)
	{
		fprintf(stderr,"ERROR: %s\n",error_msg);
		return -1;
	}

	model = svm_train(&prob,&param);

	compute_primal(model, primal, b);
	if (DEBUG_FLAG)
		printf("w = %f %f, b = %f\n", primal[0], primal[1], b);

	// Compute the offset and drift
	drift = -(primal[0]*peer_offset_bounds_sd)/(primal[1]*instant_sd);
	offset = peer_offset_bounds_mean + (peer_offset_bounds_sd*(instant_mean*primal[0] + 2*instant_sd*b))/(instant_sd*primal[1]);
	if (DEBUG_FLAG)
		printf("drift = %f, offset = %f\n", drift, offset);


	svm_destroy_param(&param);

	// Free memory
	free(prob.y);
	for (int i = 0; i < prob.l; i++)
		free(prob.x[i]);
	free(prob.x);

	return 0;
}

// Private function to calculate the mean and standard deviation
double calculateSD(std::vector<int64_t> &data, int64_t &mean, int vec_size)
{
    int64_t sum = 0;
    double standardDeviation = 0.0;

    int i;

    for(i = 0; i < vec_size; ++i)
    {
        sum += data[i];
    }

    mean = sum/vec_size;

    for(i = 0; i < vec_size; ++i)
        standardDeviation += pow((double)(data[i] - mean), 2);

    return sqrt(standardDeviation/vec_size);
}

// read in a problem (in svmlight format)
int formulate_problem(std::vector<int64_t> &peer_offset_bounds, std::vector<int64_t> &instant, int vec_len)
{
	int max_index, inst_max_index, i;
	size_t elements, j;

	prob.l = vec_len*2;

	// std::ofstream myfile;
	// myfile.open ("svm.txt");

	// Allocate Memory
	prob.y = Malloc(double,prob.l);
	prob.x = Malloc(struct svm_node *,prob.l);
	
	for (int i = 0; i < prob.l; i++)
		prob.x[i] = Malloc(struct svm_node, 3);

	// Data Scaling Parameters
	peer_offset_bounds_sd = calculateSD(peer_offset_bounds, peer_offset_bounds_mean, 2*vec_len);
	instant_sd = calculateSD(instant, instant_mean, vec_len);

	// std::cout << "Bounds  mean = " << peer_offset_bounds_mean << " sd = " << peer_offset_bounds_sd << "\n";
	// std::cout << "Instant mean = " << instant_mean << " sd = " << instant_sd << "\n";
	
	// Populate problem
	for (int i = 0; i < prob.l; i = i + 2)
	{
		// Populate the upper bound on the offset
		prob.y[i] = 1;
		prob.x[i][0].index = 1;
		prob.x[i][0].value = double(instant[i/2] - instant_mean)/(2*instant_sd);
		prob.x[i][1].index = 2;
		prob.x[i][1].value = double(peer_offset_bounds[i] - peer_offset_bounds_mean)/(2*peer_offset_bounds_sd);
		prob.x[i][2].index = -1;

		// Populate the lower bound on the offset
		prob.y[i+1] = -1;
		prob.x[i+1][0].index = 1;
		prob.x[i+1][0].value = double(instant[i/2] - instant_mean)/(2*instant_sd);
		prob.x[i+1][1].index = 2;
		prob.x[i+1][1].value = double(peer_offset_bounds[i+1] - peer_offset_bounds_mean)/(2*peer_offset_bounds_sd);
		prob.x[i+1][2].index = -1;

		// myfile << prob.y[i] << "," << prob.x[i][0].value << "," << prob.x[i][1].value << "\n";
		// myfile << prob.y[i+1] << "," << prob.x[i+1][0].value << "," << prob.x[i+1][1].value << "\n";
        //myfile << prob.y[i] << " 1:" << prob.x[i][0].value << " 2:" << prob.x[i][1].value << "\n";
		//myfile << prob.y[i+1] << " 1:" << prob.x[i+1][0].value << " 2:" << prob.x[i+1][1].value << "\n";
	}

	// myfile.close();

	max_index = 0;

	param.gamma = 1.0/2.0;

	if(param.kernel_type == PRECOMPUTED)
	{
		for(i=0;i<prob.l;i++)
		{
			if (prob.x[i][0].index != 0)
			{
				fprintf(stderr,"Wrong input format: first column must be 0:sample_serial_number\n");
				exit(1);
			}
			if ((int)prob.x[i][0].value <= 0 || (int)prob.x[i][0].value > max_index)
			{
				fprintf(stderr,"Wrong input format: sample_serial_number out of range\n");
				exit(1);
			}
		}
	}
	return 0;
}
