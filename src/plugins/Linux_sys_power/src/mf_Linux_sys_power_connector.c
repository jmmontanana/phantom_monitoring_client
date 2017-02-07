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
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <hwloc.h>
#include <papi.h>

#include "mf_Linux_sys_power_connector.h"

/*
 **********************************************************************
 Energy, in milliJoul, when read a bytes
   - Read: 0.02 * 2.78
   - Write: 0.02 * 2.19
 **********************************************************************
 */
#define E_DISK_R_PER_KB (0.02 * 2.78)
#define E_DISK_W_PER_KB  (0.02 * 2.19)

/*
 **********************************************************************
 My Laptop Intel 2200 BG wireless network card:
   - Transmit: 1800 mW
   - Receive: 1400 mW
   - Real upload bandwidth: 12.330M/s
   - Real download bandwidth 5.665M/s
 **********************************************************************
 */
#define E_NET_SND_PER_KB (1800 / (1024 * 12.330))
#define E_NET_RCV_PER_KB (1400 / (1024 * 5.665))


#define SUCCESS 1
#define FAILURE 0
#define POWER_EVENTS_NUM 5

#define NET_STAT_FILE "/proc/net/dev"
#define IO_STAT_FILE "/proc/%d/io"

#define HAS_CPU_STAT 0x01 
#define HAS_RAM_STAT 0x02
#define HAS_NET_STAT 0x04
#define HAS_IO_STAT 0x08
#define HAS_ALL 0x10

/*******************************************************************************
 * Variable Declarations
 ******************************************************************************/

/* flag indicates which events are given as input */
unsigned int flag = 0;
/* time in seconds */
double before_time, after_time; 

const char Linux_sys_power_metrics[POWER_EVENTS_NUM][32] = {
	"power_CPU", "power_mem",
	"power_net", "power_disk", "power_total" };

int EventSet = PAPI_NULL;
int num_sockets = 0;
double denominator = 1.0 ; /*according to different CPU models, DRAM energy scalings are different */
int rapl_is_available = 0;
float ecpu_before, emem_before, ecpu_after, emem_after;

struct net_stats {
	unsigned long long rcv_bytes;
	unsigned long long send_bytes;
};
struct net_stats net_stat_before;
struct net_stats net_stat_after;

struct io_stats {
	unsigned long long read_bytes;
	unsigned long long write_bytes;	
};
struct io_stats io_stat_before;
struct io_stats io_stat_after;

/*******************************************************************************
 * Forward Declarations
 ******************************************************************************/
int flag_init(char **events, size_t num_events);
int NET_stat_read(struct net_stats *nets_info);
int sys_IO_stat_read(struct io_stats *total_io_stat);
int process_IO_stat_read(int pid, struct io_stats *io_info);
float sys_net_energy(struct net_stats *stats_before, struct net_stats *stats_after);
float sys_disk_energy(struct io_stats *stats_before, struct io_stats *stats_after);

int rapl_init(void);
int rapl_stat_read(float *ecpu, float *emem);
int load_papi_library(void);
int check_rapl_component(void);
int hardware_sockets_count(void);
double rapl_get_denominator(void);
void native_cpuid(unsigned int *eax, unsigned int *ebx, unsigned int *ecx, unsigned int *edx);


/** @brief Initializes the Linux_sys_power plugin
 *
 *  Check if input events are valid; add valid events to the data->events
 *  acquire the previous value and before timestamp
 *
 *  @return 1 on success; 0 otherwise.
 */
int mf_Linux_sys_power_init(Plugin_metrics *data, char **events, size_t num_events)
{
	/* failed to initialize flag means that all events are invalid */
	if(flag_init(events, num_events) == 0) {
		return FAILURE;
	}
	
	/* initialize Plugin_metrics events' names according to flag */
	int i = 0;

	if(flag & HAS_ALL) {
		data->events[i] = malloc(MAX_EVENTS_LEN * sizeof(char));	
    	strcpy(data->events[i], "power_total");
    	/* initialize RAPL counters for CPU and DRAM energy measurement;
    	   create eventsets */
    	rapl_is_available = rapl_init();
    	if(rapl_is_available) {
    		rapl_stat_read(&ecpu_before, &emem_before);
    	}

    	/* read the current network rcv/send bytes */
    	NET_stat_read(&net_stat_before);
    	/* read the current io read/write bytes for all processes */
    	sys_IO_stat_read(&io_stat_before);
    	i++;

    	/* data->events create if other metrics are required */
    	if((flag & HAS_CPU_STAT) || (flag & HAS_RAM_STAT)) {
			data->events[i] = malloc(MAX_EVENTS_LEN * sizeof(char));	
    		strcpy(data->events[i], "power_CPU");
    		i++;
    		data->events[i] = malloc(MAX_EVENTS_LEN * sizeof(char));	
    		strcpy(data->events[i], "power_mem");
    		i++;
		}
		if(flag & HAS_NET_STAT) {
			data->events[i] = malloc(MAX_EVENTS_LEN * sizeof(char));	
	    	strcpy(data->events[i], "power_net");
    		i++;
		}
		if(flag & HAS_IO_STAT) {
			data->events[i] = malloc(MAX_EVENTS_LEN * sizeof(char));	
    		strcpy(data->events[i], "power_disk");
    		i++;
		}

	}
	else {
		if((flag & HAS_CPU_STAT) || (flag & HAS_RAM_STAT)) {
			data->events[i] = malloc(MAX_EVENTS_LEN * sizeof(char));	
    		strcpy(data->events[i], "power_CPU");
    		i++;
    		data->events[i] = malloc(MAX_EVENTS_LEN * sizeof(char));	
    		strcpy(data->events[i], "power_mem");
    		i++;
    		/* initialize RAPL counters if available */
			rapl_is_available = rapl_init();
			if(rapl_is_available) {
    			rapl_stat_read(&ecpu_before, &emem_before);
    		}
		}
		if(flag & HAS_NET_STAT) {
			data->events[i] = malloc(MAX_EVENTS_LEN * sizeof(char));	
	    	strcpy(data->events[i], "power_net");
	    	i++;
    		/* read the current network rcv/send bytes */
    		NET_stat_read(&net_stat_before);
		}
		if(flag & HAS_IO_STAT) {
			data->events[i] = malloc(MAX_EVENTS_LEN * sizeof(char));	
    		strcpy(data->events[i], "power_disk");
    		i++;
	    	/* read the current io read/write bytes for all processes */
    		sys_IO_stat_read(&io_stat_before);
		}
	}

	data->num_events = i;
	
	/* get the before timestamp in second */
	struct timespec timestamp;
	clock_gettime(CLOCK_REALTIME, &timestamp);
    before_time = timestamp.tv_sec * 1.0  + (double)(timestamp.tv_nsec / 1.0e9);

	return SUCCESS;
}

/** @brief Samples all possible events and stores data into the Plugin_metrics
 *
 *  @return 1 on success; 0 otherwise.
 */
int mf_Linux_sys_power_sample(Plugin_metrics *data)
{
	/* get current timestamp in second */
	struct timespec timestamp;
	clock_gettime(CLOCK_REALTIME, &timestamp);
    after_time = timestamp.tv_sec * 1.0  + (double)(timestamp.tv_nsec / 1.0e9);

	double time_interval = after_time - before_time; /* get time interval */
	 
	float ecpu, emem, enet, edisk;

	int i = 0;
	if(flag & HAS_ALL) {
		/* get CPU and DRAM energy by rapl read (unit in milliJoule) */
		if(rapl_is_available) {
			rapl_stat_read(&ecpu_after, &emem_after);	
		}
		ecpu = ecpu_after - ecpu_before;
		emem = emem_after - emem_before;
		ecpu_before = ecpu_after;
		emem_before = emem_after;
		
		/* get net energy (unit in milliJoule) */
		NET_stat_read(&net_stat_after);
		enet = sys_net_energy(&net_stat_before, &net_stat_after);

		/* get disk energy (unit in milliJoule) */
		sys_IO_stat_read(&io_stat_after);
		edisk = sys_disk_energy(&io_stat_before, &io_stat_after);

		/* get total energy via addition; calculate average power during the time interval */
		data->values[i] = (ecpu + emem + enet + edisk) / time_interval;
		i++;
		/* assign values to the data->values according to the flag (unit in mW)*/
		if((flag & HAS_CPU_STAT) || (flag & HAS_RAM_STAT)) {
			data->values[i] = ecpu / time_interval;
			i++;
    		data->values[i] = emem / time_interval;
    		i++;
		}
		if(flag & HAS_NET_STAT) {
			data->values[i] = enet / time_interval;
			i++;
		}
		if(flag & HAS_IO_STAT) {
			data->values[i] = edisk / time_interval;
			i++;
		}
	}
	else {
		if((flag & HAS_CPU_STAT) || (flag & HAS_RAM_STAT)) {
			/* get CPU and DRAM energy by rapl read (unit in milliJoule) */
			if(rapl_is_available) {
				rapl_stat_read(&ecpu_after, &emem_after);	
			}
			ecpu = ecpu_after - ecpu_before;
			emem = emem_after - emem_before;
			ecpu_before = ecpu_after;
			emem_before = emem_after;

			data->values[i] = ecpu / time_interval;
			i++;
			data->values[i] = emem / time_interval;
			i++;
		}
		if(flag & HAS_NET_STAT) {
			/* get net energy (unit in milliJoule) */
			NET_stat_read(&net_stat_after);
			enet = sys_net_energy(&net_stat_before, &net_stat_after);			
			data->values[i] = enet / time_interval;
			i++;
		}
		if(flag & HAS_IO_STAT) {
			/* get disk energy (unit in milliJoule) */
			sys_IO_stat_read(&io_stat_after);
			edisk = sys_disk_energy(&io_stat_before, &io_stat_after);
			data->values[i] = edisk / time_interval;
			i++;
		}
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
void mf_Linux_sys_power_to_json(Plugin_metrics *data, char *json)
{
    char tmp[128] = {'\0'};
    int i;
    /*
     * prepares the json string, including current timestamp, and name of the plugin
     */
    sprintf(json, "\"plugin\":\"Linux_sys_power\"");
    sprintf(tmp, ",\"local_timestamp\":\"%.1f\"", after_time * 1.0e3);
    strcat(json, tmp);

    /*
     * filters the sampled data with respect to metrics values
     */
	for (i = 0; i < data->num_events; i++) {
		/* if metrics' name is power_CPU, but flag do not HAS_CPU_STAT, ignore the value*/
		if(strcmp(data->events[i], "power_CPU") == 0 && !(flag & HAS_CPU_STAT))
			continue;

		/* if metrics' name is power_mem, but flag do not HAS_RAM_STAT, ignore the value*/
		if(strcmp(data->events[i], "power_mem") == 0 && !(flag & HAS_RAM_STAT))
			continue;

		/* if metrics' value >= 0.0, append the metrics to the json string */
		if(data->values[i] >= 0.0) {
			sprintf(tmp, ",\"%s\":%.3f", data->events[i], data->values[i]);
			strcat(json, tmp);
		}
	}
}


/** @brief Adds events to the data->events, if the events are valid
 *
 *  @return 1 on success; 0 if no events are valid.
 */
int flag_init(char **events, size_t num_events) 
{
	int i, ii;
	for (i=0; i < num_events; i++) {
		for (ii = 0; ii < POWER_EVENTS_NUM; ii++) {
			/* if events name matches */
			if(strcmp(events[i], Linux_sys_power_metrics[ii]) == 0) {
				/* get the flag updated */
				unsigned int current_event_flag = 1 << ii;
				flag = flag | current_event_flag;
			}
		}
	}
	if (flag == 0) {
		fprintf(stderr, "Wrong given metrics.\nPlease given metrics ");
		for (ii = 0; ii < POWER_EVENTS_NUM; ii++) {
			fprintf(stderr, "%s ", Linux_sys_power_metrics[ii]);
		}
		fprintf(stderr, "\n");
		return FAILURE;
	}
	else {
		return SUCCESS;
	}
}

/* Gets current network stats (send and receive bytes via wireless card). 
 * return 1 on success; 0 otherwise
 */
int NET_stat_read(struct net_stats *nets_info) {
	FILE *fp;
	char line[1024];
	unsigned int temp;
	unsigned long long temp_rcv_bytes, temp_send_bytes;

	fp = fopen(NET_STAT_FILE, "r");
	if(fp == NULL) {
		fprintf(stderr, "Error: Cannot open %s.\n", NET_STAT_FILE);
		return 0;
	}
	/* values reset to zeros */
	nets_info->rcv_bytes = 0;
	nets_info->send_bytes = 0;

	while(fgets(line, 1024, fp) != NULL) {
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

/* Gets the IO stats of the whole system.
 * read IO stats for all processes and make an addition  
 * return 1 on success; 0 otherwise
 */
int sys_IO_stat_read(struct io_stats *total_io_stat) {
	DIR *dir;
	struct dirent *drp;
	int pid;

	/* open /proc directory */
	dir = opendir("/proc");
	if (dir == NULL) {
		fprintf(stderr, "Error: Cannot open /proc.\n");
		return 0;
	}

	/* declare data structure which stores the io stattistics of each process */
	struct io_stats pid_io_stat;

	/* reset total_io_stat into zeros */
	total_io_stat->read_bytes = 0;
	total_io_stat->write_bytes = 0;

	/* get the entries in the /proc directory */
	drp = readdir(dir);
	while (drp != NULL) {
		/* if entry's name starts with digit,
		   get the process pid and read the IO stats of the process */
		if (isdigit(drp->d_name[0])) {
			pid = atoi(drp->d_name);
			if (process_IO_stat_read(pid, &pid_io_stat)) {
				total_io_stat->read_bytes += pid_io_stat.read_bytes;
				total_io_stat->write_bytes += pid_io_stat.write_bytes;
			}
		}
		/* read the next entry in the /proc directory */
		drp = readdir(dir);
	}

	/* close /proc directory */
	closedir(dir);
	return 1; 
}

/* Gets the IO stats of a specified process. 
 * parameters are pid and io_info
 * return 1 on success; 0 otherwise
 */
int process_IO_stat_read(int pid, struct io_stats *io_info) {
	FILE *fp;
	char filename[128], line[256];

	sprintf(filename, IO_STAT_FILE, pid);
	if ((fp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "Error: Cannot open %s.\n", filename);
		return 0;
	}
	io_info->read_bytes = 0;
	io_info->write_bytes = 0;

	while (fgets(line, 256, fp) != NULL) {
		if (!strncmp(line, "read_bytes:", 11)) {
			sscanf(line + 12, "%llu", &io_info->read_bytes);
		}
		if (!strncmp(line, "write_bytes:", 12)) {
			sscanf(line + 13, "%llu", &io_info->write_bytes);
		}
		if ((io_info->read_bytes * io_info->write_bytes) != 0) {
			break;
		}
	}
	fclose(fp);
	return 1;
}

/* Calculates the system network energy consumption;
 * updates net statistics values   
 * return the network energy consumption (in milliJoule)
 */
float sys_net_energy(struct net_stats *stats_before, struct net_stats *stats_after) 
{
	unsigned long long rcv_bytes, send_bytes;

	rcv_bytes = stats_after->rcv_bytes - stats_before->rcv_bytes;
	send_bytes = stats_after->send_bytes - stats_before->send_bytes;

	float enet = ((rcv_bytes * E_NET_RCV_PER_KB) + (send_bytes * E_NET_SND_PER_KB)) / 1024;
	
	/* update the net_stat_before values by the current values */
	stats_before->rcv_bytes = stats_after->rcv_bytes;
	stats_before->send_bytes = stats_after->send_bytes;

	return enet;
}

/* Calculates the system disk energy consumption;
 * updates io statistics values   
 * return the disk energy consumption (in milliJoule)
 */
float sys_disk_energy(struct io_stats *stats_before, struct io_stats *stats_after) 
{
	unsigned long long read_bytes, write_bytes;

	read_bytes = stats_after->read_bytes - stats_before->read_bytes;
	write_bytes = stats_after->write_bytes - stats_before->write_bytes;

	float edisk = ((read_bytes * E_DISK_R_PER_KB) + (write_bytes * E_DISK_W_PER_KB)) / 1024;

	/* update the io_stat_before values by the current values */
	stats_before->read_bytes = stats_after->read_bytes;
	stats_before->write_bytes = stats_after->write_bytes;

	return edisk;
}

/* initialize RAPL counters prepare eventset and start counters */
int rapl_init(void) {

	/* Load PAPI library */
	if (!load_papi_library()) {
        return FAILURE;
    }

    /* check if rapl component is enabled */
    if (!check_rapl_component()) {
        return FAILURE;
    }

    /* get the number of sockets */
    num_sockets = hardware_sockets_count();
    if(num_sockets <= 0) {
    	return FAILURE;
    }

	/* creat an PAPI EventSet */
	if (PAPI_create_eventset(&EventSet) != PAPI_OK) {
		fprintf(stderr, "Error: PAPI_create_eventset failed.\n");
		return FAILURE;
	}

	/* add for each socket the package energy and dram energy events */
	int i, ret;
	char event_name[32] = {'\0'};

	for (i = 0; i < num_sockets; i++) {
		memset(event_name, '\0', 32 * sizeof(char));
		sprintf(event_name, "PACKAGE_ENERGY:PACKAGE%d", i);
		ret = PAPI_add_named_event(EventSet, event_name);
		if (ret != PAPI_OK) {
			fprintf(stderr, "Error: Couldn't add event: %s\n", event_name);
		}
		memset(event_name, '\0', 32 * sizeof(char));
		sprintf(event_name, "DRAM_ENERGY:PACKAGE%d", i);
		ret = PAPI_add_named_event(EventSet, event_name);
		if (ret != PAPI_OK) {
			fprintf(stderr, "Error: Couldn't add event: %s\n", event_name);
		}
	}
	/* set dominator for DRAM energy values based on different CPU model */
	denominator = rapl_get_denominator();

	if (PAPI_start(EventSet) != PAPI_OK) {
		fprintf(stderr, "PAPI_start failed.\n");
		return FAILURE;
	}

	return SUCCESS;
}

/* Load the PAPI library */
int load_papi_library(void)
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

/* Check if rapl component is enabled */
int check_rapl_component(void)
{
	int numcmp, cid; /* number of component and component id variables declare */
	const PAPI_component_info_t *cmpinfo = NULL;

	numcmp = PAPI_num_components();
    for (cid = 0; cid < numcmp; cid++) {
        cmpinfo = PAPI_get_component_info(cid);
        if (strstr(cmpinfo->name, "rapl")) {
            if (cmpinfo->disabled) {
                fprintf(stderr, "Component RAPL is DISABLED");
                return FAILURE;
            } else {
                return SUCCESS;
            }
        }
    }
    return FAILURE;
}

/* Count the number of available sockets by hwloc library. 
 * return the number of sockets on success; 0 otherwise
 */
int hardware_sockets_count(void)
{
	int depth;
	int skts_num = 0;
	hwloc_topology_t topology;
	hwloc_topology_init(&topology);
	hwloc_topology_load(topology);
	depth = hwloc_get_type_depth(topology, HWLOC_OBJ_SOCKET);
	if (depth == HWLOC_TYPE_DEPTH_UNKNOWN) {
		fprintf(stderr, "Error: The number of sockets is unknown.\n");
		return 0;
	}
	skts_num = hwloc_get_nbobjs_by_depth(topology, depth);
	return skts_num;
}

/*get the coefficient of current CPU model */
double rapl_get_denominator(void)
{
	/* get cpu model */
    unsigned int eax, ebx, ecx, edx;
    eax = 1;
    native_cpuid(&eax, &ebx, &ecx, &edx);
    int cpu_model = (eax >> 4) & 0xF;

    if (cpu_model == 15) {
        return 15.3;
    } else {
        return 1.0;
    }

}

/* Get native cpuid */
void native_cpuid(unsigned int *eax, unsigned int *ebx, unsigned int *ecx, unsigned int *edx)
{
    asm volatile("cpuid"
        : "=a" (*eax),
          "=b" (*ebx),
          "=c" (*ecx),
          "=d" (*edx)
        : "0" (*eax), "2" (*ecx)
    );
}

/* Read rapl counters values, computer the energy values for CPU and DRAM; (in milliJoule)
   counters are reset after read */
int rapl_stat_read(float *ecpu, float *emem) 
{
	int i, ret;
	long long *values = malloc(2 * num_sockets * sizeof(long long));
	float e_package_temp = 0.0, e_dram_temp = 0.0;

	ret = PAPI_read(EventSet, values);
	if(ret != PAPI_OK) {
		char *error = PAPI_strerror(ret);
		fprintf(stderr, "Error while reading the PAPI counters: %s", error);
        return FAILURE;
	}
	
	for(i = 0; i < num_sockets * 2; i++) {
		e_package_temp += (float) (values[i] * 1.0e-6);
		i++;
		e_dram_temp += (float) (values[i] * 1.0e-6 ) / denominator;
	}
	PAPI_reset(EventSet);
	*ecpu = e_package_temp;
	*emem = e_dram_temp;
	return SUCCESS;
}