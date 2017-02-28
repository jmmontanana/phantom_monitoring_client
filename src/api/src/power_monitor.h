#ifndef _POWER_MONITOR_H
#define _POWER_MONITOR_H

#define METRIC_NAME_3 "power"

typedef struct cpu_info_t {
	unsigned long long sys_itv;
	unsigned long long sys_runtime;
	unsigned long long pid_runtime;
} cpu_info;

float max_cpu_power;
float min_cpu_power;

int power_monitor(int pid, char *DataPath, long sampling_interval);
int cpu_info_read(int pid, cpu_info *info);
float cpu_freq_stat(void);

#endif


