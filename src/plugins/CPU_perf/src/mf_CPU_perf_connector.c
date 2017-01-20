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
#include "mf_CPU_perf_connector.h"

#define SUCCESS 1
#define FAILURE 0
#define PAPI_EVENTS_NUM 3

/*******************************************************************************
 * Variable Declarations
 ******************************************************************************/
const int PAPI_EVENTS[PAPI_EVENTS_NUM] = {PAPI_FP_INS, PAPI_FP_OPS, PAPI_TOT_INS};
const char CPU_perf_metrics[PAPI_EVENTS_NUM][16] = {"MFLIPS", "MFLOPS", "MIPS"};
int EventSet = PAPI_NULL;
long long before_time, after_time;

/*******************************************************************************
 * Forward Declarations
 ******************************************************************************/
int events_are_all_not_valid(char **events, size_t num_events);
static int load_papi_library();

/** @brief Initializes the CPU_perf plugin
 *
 *  Load papi library; create a EventSet; add available events to the EventSet;
 *  get the start timestamp; and start the counters specified in the generated EventSet.
 *
 *  @return 1 on success; 0 otherwise.
 */
int mf_CPU_perf_init(Plugin_metrics *data, char **events, size_t num_events)
{
	/* if all given events are not valid, return directly */
	if (events_are_all_not_valid(events, num_events)) {
		return FAILURE;
	}

	if (!load_papi_library()) {
        return FAILURE;
    }
	/* check if the papi events available, creat an EventSet for all available events */
	int i,j;
	if (PAPI_create_eventset(&EventSet) != PAPI_OK) {
		fprintf(stderr, "PAPI_create_eventset failed.\n");
		return FAILURE;
	}
	for (i = 0, j = 0; i < PAPI_EVENTS_NUM; i++ ) {
		if ( PAPI_query_event(PAPI_EVENTS[i]) == PAPI_OK ) {
			PAPI_add_event(EventSet, PAPI_EVENTS[i]);
			data->events[j] = malloc(MAX_EVENTS_LEN * sizeof(char));
			strcpy(data->events[j], CPU_perf_metrics[i]);
			j++;
		}
	}
	data->num_events = j;

	/* Start counting events */
	before_time = PAPI_get_real_nsec();
	
	if (PAPI_start(EventSet) != PAPI_OK) {
		fprintf(stderr, "PAPI_start failed.\n");
		return FAILURE;
	}

	return SUCCESS;
}

/** @brief Samples all possible events and stores data into the Plugin_metrics
 *
 *  @return 1 on success; 0 otherwise.
 */
int mf_CPU_perf_sample(Plugin_metrics *data)
{
	int ret, i;
	long long values[PAPI_EVENTS_NUM];
	long long duration;

	after_time = PAPI_get_real_nsec();
	
	ret = PAPI_read(EventSet, values);
	if(ret != PAPI_OK) {
		char *error = PAPI_strerror(ret);
		fprintf(stderr, "Error while reading the PAPI counters: %s", error);
        return FAILURE;
	}

	duration = after_time - before_time; /* in nanoseconds */
	for(i = 0; i < data->num_events; i++) {
		data->values[i] = (float) (values[i] * 1.0e3) / duration; /*units are Mflips, Mflops, and Mips */
	}

	/* update timestamp and reset counters to zero */
	before_time = after_time;
	PAPI_reset(EventSet);

	return SUCCESS;
}

/** @brief Formats the sampling data into a json string
 *
 *  json string contains: plugin name, timestamps, metrics_name and metrics_value
 *
 */
void mf_CPU_perf_to_json(Plugin_metrics *data, char **events, size_t num_events, char *json)
{
	struct timespec timestamp;
    char tmp[128] = {'\0'};
    int i, ii;
    /*
     * prepares the json string, including current timestamp, and name of the plugin
     */
    sprintf(json, "\"plugin\":\"CPU_perf\"");
    clock_gettime(CLOCK_REALTIME, &timestamp);
    double ts = timestamp.tv_sec + (double)(timestamp.tv_nsec / 1.0e9);
    sprintf(tmp, ",\"@timestamp\":\"%.4f\"", ts);
    strcat(json, tmp);

    /*
     * filters the sampled data with respect to metrics given
     */
	for (i = 0; i < num_events; i++) {
		for(ii = 0; ii < data->num_events; ii++) {
			/* if metrics' name matches, append the metrics to the json string */
			if(strcmp(events[i], data->events[ii]) == 0) {
				sprintf(tmp, ",\"%s\":%.3f", data->events[ii], data->values[ii]);
				strcat(json, tmp);
			}
		}
	}
}

/** @brief Stops the plugin
 *
 *  This methods stops papi counters gracefully;
 *
 */
void mf_CPU_perf_shutdown()
{
	int ret = PAPI_stop(EventSet, NULL);
    if (ret != PAPI_OK) {
        char *error = PAPI_strerror(ret);
        fprintf(stderr, "Couldn't stop PAPI EventSet: %s", error);
    }

    ret = PAPI_cleanup_eventset(EventSet);
    if (ret != PAPI_OK) {
        char *error = PAPI_strerror(ret);
        fprintf(stderr, "Couldn't cleanup PAPI EventSet: %s", error);
    }

    ret = PAPI_destroy_eventset(&EventSet);
    if (ret != PAPI_OK) {
        char *error = PAPI_strerror(ret);
        fprintf(stderr, "Couldn't destroy PAPI EventSet: %s", error);
    }
	PAPI_shutdown();
}

/* Load the PAPI library */
static int load_papi_library()
{
    if (PAPI_is_initialized()) {
        return SUCCESS;
    }

    int ret = PAPI_library_init(PAPI_VER_CURRENT);
    if (ret != PAPI_VER_CURRENT) {
        char *error = PAPI_strerror(ret);
        fprintf(stderr, "Error while loading the PAPI library: %s", error);
        return FAILURE;
    }

    return SUCCESS;
}
/** @brief Checks if all events are not valid
 *
 *  @return 1 when all events are not valid; 0 otherwise.
 */
int events_are_all_not_valid(char **events, size_t num_events) 
{
	int i, ii, counter;
	counter = 0; 
	for (i=0; i < num_events; i++) {
		for (ii = 0; ii < PAPI_EVENTS_NUM; ii++) {
			/* if events name matches, counter is incremented by 1 */
			if(strcmp(events[i], CPU_perf_metrics[ii]) == 0) {
				counter++;
			}
		}
	}
	if (counter == 0) {
		fprintf(stderr, "Wrong given metrics.\nPlease given metrics MFLIPS, MFLOPS, or MIPS\n");
		return 1;
	}
	else {
		return 0;
	}
}
