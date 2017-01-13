# Introduction of plugins and usage information

|------------------------------------------------------------------ |
## Introduction

The monitoring client is composed of 8 plugins, monitoring all kinds of the system infrastructure-level performance and power metrics. The plugins implemented are based on various libraries, system hardware counters and Linux proc filesystem. In addition to be used by the monitoring client, each plugin can be built as a standalone client, being executed alone with given specific metrics name. 

More details about each plugin, for example, the plugins' usage, prerequisites and supported metrics are all clarified in the following.

### List of plugins

- Board_power
- CPU_perf
- CPU_temperature
- FPGA_IO
- Linux_resources
- Linux_sys_power
- Movidius_power
- NVML

|------------------------------------------------------------------ |
## Board_power Plugin

This plugin is based on the external ACME power measurement kit and the libiio library, which is installed during the monitoring client setup process, done automatically by the setup.sh shell script. In case that the ACME power measurement board is not connected with the monitoring client hosted computer or the libiio library is not found, the plugin will fail and report associated error messages.

It is recommended to try the following command to test the ACME power measurement board connection:

```
$ ping baylibre-acme.local
```

### Usage and metrics

The Board_power plugin can be built and ran alone, outside the monitoring framework. In the directory of the plugin, execute the **Makefile** using

```
$ make clean all
```

, before calling the generated sampling client **mf_Board_power_client**, please add the required libraries to the **LD_LIBRARY_PATH** by:

```
$ source setenv.sh
```

It is advised to run the sampling client **mf_Board_power_client** like the follows:

```
$ ./mf_Board_power_client <LIST_OF_Board_power_METRICS>
```

Replace **<LIST_OF_Board_power_METRICS>** with a space-separated list of the following events:

- device0:current
- device0:vshunt
- device0:vbus
- device0:power

Unit and description for each metric is showed in the following table:

| Metrics    | Units    | Description   |
|----------- |--------  |-------------  |
| device0:current | mA | current go throught the Jack probe |
| device0:vshunt  | mV | voltage over the shunt |
| device0:vbus    | mV | voltage over the bus |
| device0:power   | mW | power measured for the target board |


|------------------------------------------------------------------ |
## CPU_perf Plugin

This plugin is based on the PAPI library and associated system hardware counters (PAPI_FP_INS, PAPI_FP_OPS, PAPI_TOT_INS). The PAPI library is installed during the monitoring client setup process, done by the setup.sh shell script. In case that the PAPI library is not found or the associated PAPI counters are not available, the plugin will fail at the initialization stage.

### Usage and metrics

The CPU_perf plugin can be built and ran alone, outside the monitoring framework. In the directory of the plugin, execute the **Makefile** using

```
$ make all
```

, before calling the generated sampling client **mf_CPU_perf_client**, please add the required libraries to the **LD_LIBRARY_PATH** by:

```
$ source setenv.sh
```

It is advised to run the sampling client **mf_CPU_perf_client** with root permissions, like:

```
$ ./mf_CPU_perf_client <LIST_OF_CPU_perf_METRICS>
```

Replace **<LIST_OF_CPU_perf_METRICS>** with a space-separated list of the following events:

- MFLIPS
- MFLOPS
- MIPS

Unit and description for each metric is showed in the following table:

| Metrics    | Units    | Description   |
|----------- |--------  |-------------  |
| MFLIPS     | Mflip/s  | mega floating-point instructions per second |
| MFLOPS     | Mflop/s  | mega floating-point operations per second |
| MIPS       | Mip/s    | million instructions per second |


|------------------------------------------------------------------ |
## CPU_temperature Plugin

This plugin is based on the lm_sensors library which provides tools to and drivers to monitor CPU temperatures and other properties. The lm_sensors library is installed during the monitoring client setup process, done by the setup.sh shell script. In case that the lm_sensors library is not found, the plugin will fail at the initialization stage.

### Usage and metrics

The CPU_temperature plugin can be built and ran alone, outside the monitoring framework. In the directory of the plugin, execute the **Makefile** using

```
$ make all
```

, before calling the generated sampling client **mf_CPU_temperature_client**, please add the required libraries to the **LD_LIBRARY_PATH** by:

```
$ source setenv.sh
```

It is advised to run the sampling client **mf_CPU_temperature_client** like the follows:

```
$ ./mf_CPU_temperature_client <LIST_OF_CPU_temperature_METRICS>
```

Replace **<LIST_OF_CPU_temperature_METRICS>** with a space-separated list of the following events:

- CPU<i>:core<ii>

, where i represents the CPU socket id, ii identifies which core is the target core of monitoring. The units of all CPU_temperature metrics are degree (°c).


|------------------------------------------------------------------ |
## FPGA_IO Plugin

### Usage and metrics


|------------------------------------------------------------------ |
## Linux_resources Plugin

This plugin is based on the Linux proc filesystem which provides information and statistics about processes and system.

### Usage and metrics

The Linux_resources plugin can be built and ran alone, outside the monitoring framework. In the directory of the plugin, execute the **Makefile** using

```
$ make all
```

will build the standalone executable client **mf_Linux_resources_client**. It is advised to run the sampling client like the follows:

```
$ ./mf_Linux_resources_client <LIST_OF_Linux_resources_METRICS>
```

Replace **<LIST_OF_Linux_resources_METRICS>** with a space-separated list of the following events:

- CPU_usage_rate
- RAM_usage_rate
- swap_usage_rate
- net_throughput
- io_throughput

Unit and description for each metric is showed in the following table:

| Metrics         | Units    | Description   |
|---------------- |--------- |-------------  |
| CPU_usage_rate  | %        | percentage of CPU usage time |
| RAM_usage_rate  | %        | percentage of used RAM size |
| swap_usage_rate | %        | percentage of used swap size |
| net_throughput  | bytes/s  | total send and receive bytes via wlan and ethernet per second |
| io_throughput   | bytes/s  | total disk read and write bytes per second |


|------------------------------------------------------------------ |
## Linux_sys_power Plugin

This plugin is based on both the PAPI library (with RAPL component) and the Linux proc filesystem. The PAPI library is installed during the monitoring client setup process, done by the setup.sh shell script. In case that the PAPI library is not found or the RAPL component is not enabled, the plugin will fail at the initialization stage. For accurate power measurement of the system network and disk component, it is advised to adjust the values (E_NET_SND_PER_KB, E_NET_RCV_PER_KB, E_DISK_R_PER_KB, E_DISK_W_PER_KB), as defined in **src/mf_Linux_sys_power_connector.c** according to the network and disk sepcification. To be noticed, the system wired network power consumption is not considered by this plugin.

### Usage and metrics

The Linux_sys_power plugin can be built and ran alone, outside the monitoring framework. In the directory of the plugin, execute the **Makefile** using

```
$ make all
```

, before calling the generated sampling client **mf_Linux_sys_power_client**, please add the required libraries to the **LD_LIBRARY_PATH** by:

```
$ source setenv.sh
```

It is advised to run the sampling client **mf_Linux_sys_power_client** with root permissions, like:

```
$ ./mf_Linux_sys_power_client <LIST_OF_Linux_sys_power_METRICS>
```

Replace **<LIST_OF_Linux_sys_power_METRICS>** with a space-separated list of the following events:

- power_CPU
- power_mem
- power_net
- power_disk
- power_total

Unit and description for each metric is showed in the following table:

| Metrics      | Units      | Description   |
|------------- |----------  |-------------  |
| power_CPU    | milliwatts | CPU package power, which is measured using the RAPL PACKAGE_ENERGY event |
| power_mem    | milliwatts | DRAM power, which is measured using the RAPL DRAM_ENERGY event |
| power_net    | milliwatts | wireless network power, which is calcuated with given variables (E_NET_SND_PER_KB, E_NET_RCV_PER_KB) and monitored send and receive bytes via the wireless card |
| power_disk   | milliwatts | disk power, which is calcuated with given variables (E_DISK_R_PER_KB, E_DISK_W_PER_KB) and monitored read and write bytes via disk |
| power_total  | milliwatts | total system power, calculated by the addition of the abover metrics |


|------------------------------------------------------------------ |
## Movidius_power Plugin

### Usage and metrics


|------------------------------------------------------------------ |
## NVML Plugin

This plugin is based on the nvml (NVIDIA management library), which is a C-based API for monitoring various states of the NVIDIA GPU devices. It is required to use the library **libnvidia-ml.so**, which is installed with the NVIDIA Display Driver, therefore the default library path (/usr/lib64 or /usr/lib) is used to link and build the plugin. During the monitoring setup process, done the setup.sh shell script, the NVIDIA SDK with appropriate header file is installed.

### Usage and metrics

The NVML plugin can be built and ran alone, outside the monitoring framework. In the directory of the plugin, execute the **Makefile** using

```
$ make all
```

will build the standalone executable client **mf_NVML_client**. It is advised to run the sampling client with root permissions like the follows:

```
$ ./mf_NVML_client <LIST_OF_NVML_METRICS>
```

Replace **<LIST_OF_NVML_METRICS>** with a space-separated list of the following events:

- gpu_usage_rate
- mem_usage_rate
- mem_allocated
- PCIe_snd_throughput
- PCIe_rcv_throughput
- temperature
- power


All the metrics above is reported per GPU device. Unit and description for each metric is showed in the following table:

| Metrics             | Units      | Description   |
|-------------------- |----------- |-------------  |
| gpu_usage_rate      | %          | percentage of time for GPU execution |
| mem_usage_rate      | %          | Percent of time for device memory access |
| mem_allocated       | %          | percentage of used device memory |
| PCIe_snd_throughput | bytes/s    | PCIe send bytes per second (for Maxwell or newer architecture) |
| PCIe_rcv_throughput | bytes/s    | PCIe write bytes per second (for Maxwell or newer architecture) |
| temperature         | °c         | GPU device temperature |
| power               | milliwatts | GPU device power consumption |
