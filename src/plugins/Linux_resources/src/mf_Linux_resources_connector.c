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
#include "mf_Linux_resources_connector.h"

#define SUCCESS 1
#define FAILURE 0
#define RESOURCES_EVENTS_NUM 4

#define CPU_STAT_FILE "/proc/stat"
#define RAM_STAT_FILE "/proc/meminfo"
#define NET_STAT_FILE "/proc/net/dev"
#define IO_STAT_FILE "/proc/%s/io"

#define HAS_CPU_STAT 0x01 
#define HAS_RAM_STAT 0x02
#define HAS_NET_STAT 0x04
#define HAS_IO_STAT 0x08
/*******************************************************************************
 * Variable Declarations
 ******************************************************************************/
unsigned int flag = 0;
double before_time, after_time;

const char Linux_resources_metrics[RESOURCES_EVENTS_NUM][32] = {
	"CPU_usage_rate", "RAM_usage_rate", 
	"net_throughput", "io_throughput" };


struct net_stats {
	unsigned long long rcv_bytes;
	unsigned long long send_bytes;
};

struct net_stats net_stat_before;
struct net_stats net_stat_after;

/*******************************************************************************
 * Forward Declarations
 ******************************************************************************/
int flag_init(char **events, size_t num_events);
float CPU_usage_rate_read();
float RAM_usage_rate_read();
int NET_stat_read(struct net_stats *nets_info);

/** @brief Initializes the Linux_resources plugin
 *
 *  Check if input events are valid; add valid events to the data->events
 *  acquire the previous value and before timestamp
 *
 *  @return 1 on success; 0 otherwise.
 */
int mf_Linux_resources_init(Plugin_metrics *data, char **events, size_t num_events)
{
	/* failed to initialize flag means that all events are invalid */
	if(flag_init(events, num_events) == 0) {
		return FAILURE;
	}
	
	/* get the before timestamp in second */
	struct timespec timestamp;
	clock_gettime(CLOCK_REALTIME, &timestamp);
    before_time = timestamp.tv_sec * 1.0  + (double)(timestamp.tv_nsec / 1.0e9);
	
	/* initialize Plugin_metrics events' names according to flag */
	int i = 0;
	if(flag & HAS_CPU_STAT) {
		data->events[i] = malloc(MAX_EVENTS_LEN * sizeof(char));	
    	strcpy(data->events[i], "CPU_usage_rate");
    	i++;
	}
	if(flag & HAS_RAM_STAT) {
		data->events[i] = malloc(MAX_EVENTS_LEN * sizeof(char));	
    	strcpy(data->events[i], "RAM_usage_rate");
    	i++;
	}
	if(flag & HAS_NET_STAT) {
		data->events[i] = malloc(MAX_EVENTS_LEN * sizeof(char));	
    	strcpy(data->events[i], "net_throughput");
    	/* read the current network rcv/send bytes */
    	NET_stat_read(&net_stat_before);
    	i++;
	}
	if(flag & HAS_IO_STAT) {
		data->events[i] = malloc(MAX_EVENTS_LEN * sizeof(char));	
    	strcpy(data->events[i], "io_throughput");
    	/* read the current io read/write bytes for all processes */
    	/* ... */
    	i++;
	}
	data->num_events = i;
	return SUCCESS;
}

/** @brief Samples all possible events and stores data into the Plugin_metrics
 *
 *  @return 1 on success; 0 otherwise.
 */
int mf_Linux_resources_sample(Plugin_metrics *data)
{
	/* get current timestamp in second */
	struct timespec timestamp;
	clock_gettime(CLOCK_REALTIME, &timestamp);
    after_time = timestamp.tv_sec * 1.0  + (double)(timestamp.tv_nsec / 1.0e9);
	double time_interval = after_time - before_time;

	int i = 0;
	if(flag & HAS_CPU_STAT) {
		data->values[i] = CPU_usage_rate_read();
		i++;
	}
	if(flag & HAS_RAM_STAT) {
		data->values[i] = RAM_usage_rate_read();
		i++;
	}
	if(flag & HAS_NET_STAT) {
		NET_stat_read(&net_stat_after);
		unsigned long long total_bytes = (net_stat_after.rcv_bytes - net_stat_before.rcv_bytes) 
			+ (net_stat_after.send_bytes - net_stat_before.send_bytes);
		data->values[i] = (float) (total_bytes * 1.0 / time_interval);
		i++;
		
		/* update the net_stat_before values by the current values */
		net_stat_before.rcv_bytes = net_stat_after.rcv_bytes;
		net_stat_before.send_bytes = net_stat_after.send_bytes;
	}
	if(flag & HAS_IO_STAT) {
		data->values[i] = -1.0;
		i++;
	}

	/* update timestamp */
	before_time = after_time;
	return SUCCESS;
}

/** @brief Formats the sampling data into a json string
 *
 *  json string contains: plugin name, timestamps, metrics_name and metrics_value
 *
 */
void mf_Linux_resources_to_json(Plugin_metrics *data, char *json)
{
	struct timespec timestamp;
    char tmp[128] = {'\0'};
    int i;
    /*
     * prepares the json string, including current timestamp, and name of the plugin
     */
    sprintf(json, "{\"plugin\":\"CPU_FF_perf\"");
    clock_gettime(CLOCK_REALTIME, &timestamp);
    double ts = timestamp.tv_sec + (double)(timestamp.tv_nsec / 1.0e9);
    sprintf(tmp, ",\"@timestamp\":\"%f\"", ts);
    strcat(json, tmp);

    /*
     * filters the sampled data with respect to metrics values
     */
	for (i = 0; i < data->num_events; i++) {
		/* if metrics' value > 0.0, append the metrics to the json string */
		if(data->values[i] > 0.0) {
			sprintf(tmp, ",\"%s\":%f", data->events[i], data->values[i]);
			strcat(json, tmp);
		}
	}

	strcat(json, "}");
}


/** @brief Adds events to the data->events, if the events are valid
 *
 *  @return 1 on success; 0 if no events are valid.
 */
int flag_init(char **events, size_t num_events) 
{
	int i, ii;
	for (i=0; i < num_events; i++) {
		for (ii = 0; ii < RESOURCES_EVENTS_NUM; ii++) {
			/* if events name matches */
			if(strcmp(events[i], Linux_resources_metrics[ii]) == 0) {
				/* get the flag updated */
				unsigned int current_event_flag = 1 << ii;
				flag = flag | current_event_flag;
			}
		}
	}
	if (flag == 0) {
		printf("Wrong given metrics.\nPlease given metrics ");
		for (ii = 0; ii < RESOURCES_EVENTS_NUM; ii++) {
			printf("%s ", Linux_resources_metrics[ii]);
		}
		printf("\n");
		return FAILURE;
	}
	else {
		return SUCCESS;
	}
}

/* Gets cpu usage rate (unit is %). 
 * return the cpu usgae rate on success; 0.0 otherwise
 */
float CPU_usage_rate_read() {
	FILE *fp;
	char line[1024];
	unsigned long long cpu_user, cpu_nice, cpu_sys, cpu_idle, cpu_iowait, cpu_irq, cpu_softirq, cpu_steal;
	float CPU_usage_rate = 0.0;

	fp = fopen(CPU_STAT_FILE, "r");
	if(fp == NULL) {
		printf("Error: Cannot open %s.\n", CPU_STAT_FILE);
		return 0.0;
	}
	if (fgets(line, 1024, fp) != NULL) {
		sscanf(line + 5, "%llu %llu %llu %llu %llu %llu %llu %llu",
			       &cpu_user,
			       &cpu_nice,
			       &cpu_sys,
			       &cpu_idle,
			       &cpu_iowait,
			       &cpu_irq,
			       &cpu_softirq,
			       &cpu_steal);
		unsigned long long total_cpu_time = 
			cpu_user + cpu_nice + cpu_sys + cpu_idle + cpu_iowait + cpu_irq + cpu_softirq + cpu_steal;
		unsigned long long total_idle_time =
			cpu_idle + cpu_iowait;
		CPU_usage_rate = (total_cpu_time - total_idle_time) * 100.0 / total_cpu_time;
	}
	fclose(fp);
	return CPU_usage_rate;
}

/* Gets ram usage rate (unit is %). 
 * return the ram usgae rate on success; 0.0 otherwise
 */
float RAM_usage_rate_read() {
	FILE *fp;
	char line[1024];
	int MemTotal = 0;
	int MemFree = 0;
	float RAM_usage_rate = 0.0;

	fp = fopen(RAM_STAT_FILE, "r");
	if(fp == NULL) {
		printf("Error: Cannot open %s.\n", RAM_STAT_FILE);
		return 0.0;
	}

	while(fgets(line, 1024, fp) != NULL) {
		if (!strncmp(line, "MemTotal:", 9)) {
			sscanf(line + 9, "%d", &MemTotal);
		}
		if (!strncmp(line, "MemFree:", 8)) {
			sscanf(line + 8, "%d", &MemFree);
		}
		if ((MemTotal * MemFree) != 0) {
			RAM_usage_rate = (MemTotal - MemFree) * 100.0 / MemTotal;
			break;
		}
	}
	fclose(fp);
	return RAM_usage_rate;
}

/* Gets current network stats (send and receive bytes). 
 * return 1 on success; 0 otherwise
 */
int NET_stat_read(struct net_stats *nets_info) {
	FILE *fp;
	char line[1024];
	unsigned int temp;
	unsigned long long temp_rcv_bytes, temp_send_bytes;

	fp = fopen(NET_STAT_FILE, "r");
	if(fp == NULL) {
		printf("Error: Cannot open %s.\n", NET_STAT_FILE);
		return 0;
	}
	/* values reset to zeros */
	nets_info->rcv_bytes = 0;
	nets_info->send_bytes = 0;

	while(fgets(line, 1024, fp) != NULL) {
		char *sub_line_eth = strstr(line, "eth");
		if (sub_line_eth != NULL) {
			sscanf(sub_line_eth + 5, "%llu%u%u%u%u%u%u%u%llu", 
				&temp_rcv_bytes, &temp, &temp, &temp, &temp, &temp, &temp, &temp,
				&temp_send_bytes);

			nets_info->rcv_bytes += temp_rcv_bytes;
			nets_info->send_bytes += temp_send_bytes;
		}
		char *sub_line_wlan = strstr(line, "wlan");
		if (sub_line_wlan != NULL) {
			sscanf(sub_line_wlan + 6, "%llu%u%u%u%u%u%u%u%llu", 
				&temp_rcv_bytes, &temp, &temp, &temp, &temp, &temp, &temp, &temp,
				&temp_send_bytes);

			nets_info->rcv_bytes += temp_rcv_bytes;
			nets_info->send_bytes += temp_send_bytes;
		}
	}
	fclose(fp);
	return 1;
}
