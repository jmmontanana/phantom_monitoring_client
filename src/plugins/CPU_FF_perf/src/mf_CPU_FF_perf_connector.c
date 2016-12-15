/*
 * Copyright (C) 2015-2017 University of Stuttgart
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <papi.h>
#include "mf_CPU_FF_perf_connector.h"

#define SUCCESS 1
#define FAILURE 0

int mf_CPU_FF_perf_init(Plugin_metrics *data)
{
	float real_time_flips, real_time_flops, proc_time_flips, proc_time_flops;
	long long flpins_1, flpins_2;
	float mflips, mflops;

	if (PAPI_flips(&real_time_flips, &proc_time_flips, &flpins_1, &mflips) < PAPI_OK) {
		printf("PAPI_flips failed.\n");
		return FAILURE;
	}
	if (PAPI_flops(&real_time_flops, &proc_time_flops, &flpins_2, &mflops) < PAPI_OK) {
		printf("PAPI_flops failed.\n");
		return FAILURE;	
	}
	data->events[0] = malloc(MAX_EVENTS_LEN * sizeof(char));
	strcpy(data->events[0], "FLIPS");
	data->events[1] = malloc(MAX_EVENTS_LEN * sizeof(char));
	strcpy(data->events[1], "FLOPS");
	data->num_events = 2;
	return SUCCESS;
}


int mf_CPU_FF_perf_sample(Plugin_metrics *data)
{
	float real_time_flips, real_time_flops, proc_time_flips, proc_time_flops;
	long long flpins_1, flpins_2;
	float mflips, mflops;

	if (PAPI_flips(&real_time_flips, &proc_time_flips, &flpins_1, &mflips) < PAPI_OK) {
		printf("PAPI_flips failed.\n");
		return FAILURE;
	}
	if (PAPI_flops(&real_time_flops, &proc_time_flops, &flpins_2, &mflops) < PAPI_OK) {
		printf("PAPI_flops failed.\n");
		return FAILURE;	
	}
	data->values[0] = mflips;
	data->values[1] = mflops;
	return SUCCESS;
}

void mf_CPU_FF_perf_to_json(Plugin_metrics *data, char **events, size_t num_events, char *json)
{
	struct timespec timestamp;
    char tmp[128] = {'\0'};
    int i, ii;
    /*
     * prepares the json string, including current timestamp, and name of the plugin
     */
    sprintf(json, "{\"plugin\":\"CPU_FF_perf\"");
    clock_gettime(CLOCK_REALTIME, &timestamp);
    double ts = timestamp.tv_sec + (double)(timestamp.tv_nsec / 1.0e9);
    sprintf(tmp, ",\"@timestamp\":\"%f\"", ts);
    strcat(json, tmp);

    /*
     * filters the sampled data with respect to metrics given
     */
	for (i = 0; i < num_events; i++) {
		for(ii = 0; ii < data->num_events; ii++) {
			/* if metrics' name matches, append the metrics to the json string */
			if(strcmp(events[i], data->events[ii]) == 0) {
				sprintf(tmp, ",\"%s\":%f", data->events[ii], data->values[ii]);
				strcat(json, tmp);
			}
		}
	}
	strcat(json, "}");
}

void mf_CPU_FF_perf_shutdown()
{
	PAPI_shutdown();
}