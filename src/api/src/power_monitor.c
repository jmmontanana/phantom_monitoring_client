#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include "power_monitor.h"
#include "mf_api.h"


/* Specifications */
/*
 **********************************************************************
 CPU: in my laptop
   - 800MHZ: 6W
   - 2.Ghz: 24.5W
 **********************************************************************
 */
float maxcpupower = 24.5;	// in Watts
float mincpupower = 6.0;	// in Watts

unsigned long long freqs[16];
unsigned long long oldfreqs[16];

int power_monitor(int pid, char *DataPath, long sampling_interval)
{
	/*create and open the file*/
	char FileName[256] = {'\0'};
	sprintf(FileName, "%s/%s", DataPath, METRIC_NAME_3);
	FILE *fp = fopen(FileName, "a"); //append data to the end of the file
	if (fp == NULL) {
		printf("ERROR: Could not create file: %s\n", FileName);
		exit(0);
	}
	struct timespec timestamp_before, timestamp_after;
	double timestamp_ms;
	float sys_energy, sys_power, pid_power;
	float duration;
	cpu_info cpu_before, cpu_after;

	cpu_info_read(pid, &cpu_before);
	sys_energy = cpu_freq_stat();

	/*in a loop do data sampling and write into the file*/
	while(running) {
		/*get before timestamp in ms*/
		clock_gettime(CLOCK_REALTIME, &timestamp_before);
    	
		usleep(sampling_interval * 1000);
		cpu_info_read(pid, &cpu_after);
		sys_energy = cpu_freq_stat();
		
		/*get after timestamp in ms*/
		clock_gettime(CLOCK_REALTIME, &timestamp_after);

		/*calculate the avg power during the interval */
		if(sys_energy > 0) {
			duration = timestamp_after.tv_sec - timestamp_before.tv_sec + ((timestamp_after.tv_nsec - timestamp_before.tv_nsec) / 1.0e9);
			sys_power = sys_energy * (cpu_after.sys_runtime - cpu_before.sys_runtime) * 100 * 1.0e3 / 
				((cpu_after.sys_itv - cpu_before.sys_itv) * duration); //in milliwatt

			pid_power = sys_power * (cpu_after.pid_runtime - cpu_before.pid_runtime) * 100 /
				(cpu_after.sys_runtime - cpu_before.sys_runtime);	//propotional to the process's cpu usage rate

	    	timestamp_ms = timestamp_after.tv_sec * 1000.0  + (double)(timestamp_after.tv_nsec / 1.0e6);
    		fprintf(fp, "\"local_timestamp\":\"%.1f\", \"%s\":%.3f, \"%s\":%.3f\n", timestamp_ms, 
    			"total_CPU_power", sys_power,
    			"process_CPU_power", pid_power);	
			}
		else
			continue;
		}
	/*close the file*/
	fclose(fp);
	return 1;
}

/* read the system total time, system runtime, and process runtime */
int cpu_info_read(int pid, cpu_info *info)
{
	FILE *fp;
	char line[1024];
	unsigned long long tmp;

	/*
	  read the process runtime from /proc/[pid]/stat 
	 */
	char pid_cpu_file[128] = {'\0'};
	unsigned long long pid_utime, pid_stime;

	sprintf(pid_cpu_file, "/proc/%d/stat", pid);
	fp = fopen(pid_cpu_file, "r");
	if(fp == NULL) {
		printf("ERROR: Could not open file %s\n", pid_cpu_file);
		return 0;
	}
	char tmp_str[32];
	char tmp_char;
	if(fgets(line, 1024, fp) != NULL) {
		sscanf(line, "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu",
			(int *)&tmp, tmp_str, &tmp_char, (int *)&tmp, (int *)&tmp, (int *)&tmp, (int *)&tmp, (int *)&tmp, 
			(unsigned int *)&tmp, (unsigned long *)&tmp, (unsigned long *)&tmp, (unsigned long *)&tmp, 
			(unsigned long *)&tmp, (unsigned long *)&pid_utime, (unsigned long *)&pid_stime);
	}
	info->pid_runtime = pid_utime + pid_stime;
	fclose(fp);

	/* 
	  read the system itv and runtime 
	 */
	char cpu_file[128] = "/proc/stat";
	unsigned long long cpu_user, cpu_nice, cpu_sys, cpu_idle, cpu_iowait, cpu_hardirq, cpu_softirq, cpu_steal;

	fp = fopen(cpu_file, "r");
	if(fp == NULL) {
		printf("ERROR: Could not open file %s\n", cpu_file);
		return 0;
	}
	if(fgets(line, 1024, fp) != NULL) {
		sscanf(line+5, "%llu %llu %llu %llu %llu %llu %llu %llu",
			&cpu_user, 
			&cpu_nice, 
			&cpu_sys, 
			&cpu_idle, 
			&cpu_iowait, 
			&cpu_hardirq, 
			&cpu_softirq, 
			&cpu_steal);
	}
	info->sys_itv = cpu_user + cpu_nice + cpu_sys + cpu_idle + cpu_iowait + cpu_hardirq + cpu_softirq + cpu_steal;
	info->sys_runtime = cpu_user + cpu_sys;
	fclose(fp);
	return 1;
}

/* get the cpu freq counting and return the cpu energy since the last call of the function */
float cpu_freq_stat(void) 
{
	/* 
	  read the system cpu energy based on given max- and min- cpu energy, and frequencies statistics 
	 */
	FILE *fp;
	char line[32];
	DIR *dir;
	int i, max_i;
	struct dirent *dirent;
	char cpu_freq_file[128] = {'\0'};
	float power_range = max_cpu_power - min_cpu_power;
	float energy_delta, energy_total;
	unsigned long long tmp;

	/* 
	  check if system support cpu freq counting 
	 */
	fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/stats/time_in_state", "r");
	if(!fp) {
		return 0.0;
	}

	/* 
	  reset the freq counters
	 */
	energy_total = 0.0;
	memcpy(&oldfreqs, &freqs, sizeof(freqs));
	for (i = 0; i<16; i++)
			freqs[i] = 0;

	dir = opendir("/sys/devices/system/cpu");
	if(!dir)
		return 0.0;
	while ((dirent = readdir(dir))) {
		/* for each entry name starting by cpuxx */
		if (strncmp(dirent->d_name,"cpu", 3) != 0)
			continue;
		sprintf(cpu_freq_file, "/sys/devices/system/cpu/%s/cpufreq/stats/time_in_state", dirent->d_name);
		fp = fopen(cpu_freq_file, "r");
		if(!fp)
			continue;
		memset(line, 0, 32);
		int i = 0;
		while (!feof(fp)) {
			if(fgets(line, 32, fp) == NULL)
				break;
			sscanf(line, "%llu %llu", &tmp, &freqs[i]);
			/* each line has a pair like "<frequency> <time>", which means this CPU spent <time> usertime at <frequency>.
			  unit of <time> is 10ms
			 */
			i++;
			if (i>15)
				break;
		}
		max_i = i - 1;
		fclose(fp);
	}

	closedir(dir);
	for (i = 0; i <= max_i; i++) {
		tmp = freqs[i] - oldfreqs[i];
		if(tmp <= 0)
			continue;
		energy_delta = (max_cpu_power - power_range * i / max_i) * tmp / 100.0; // in Joule
		energy_total += energy_delta; 
	}

	return energy_total;
}