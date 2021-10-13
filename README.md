

This repository contains the code for programs that run on end-hosts for Saqr in-network scheduling experiments.
The code is based on [Racksched repo](https://github.com/netx-repo/RackSched) with modifications based on our queue model and network protocols.

------
# Setting up worker machines
These steps need to be done only once for setting up the worker machines
## Downgrading Kernel Version to 4.4
Dune which is a required component for Shinjuku did not compile/run successfully on newer Kernels.
### Download and install old kernel
> General note:  after reboot the 10G interface might get precedence and system could boot using that. In that case we get locked out!
> To fix this, make sure that only the primary interface is up in interface defult setting: /etc/network/interfaces.
Based on these sources: [1](https://serverascode.com/2019/05/17/install-and-boot-older-kernel-ubuntu.html) and [2](https://unix.stackexchange.com/questions/198003/set-default-kernel-in-grub).

Install the old kernel:

```
sudo apt install linux-image-4.4.0-142-generic
```

Hold: Since Ubuntu regularly wants to upgrade you to a newer kernel, you can apt-mark the package so it doesn't get removed again
```
sudo apt-mark hold linux-image-4.4.0-142-generic
```
Based on this [source](https://askubuntu.com/questions/798975/no-network-no-usb-after-kernel-update-in-14-04), need to install headers and extras (had ethernet problem before this, not sure if necessary):

```
sudo apt-get install linux-headers-4.4.0-142-generic
sudo apt-get install linux-image-extra-4.4.0-142-generic
```
### Update GRUB

Comment out the current default grub in `/etc/default/grub` and replace it with the sub-menu's `$menuentry_id_option` from output of this command:
`grep submenu /boot/grub/grub.cfg`
followed by '>', followed by the selected kernel's `$menuentry_id_option`. (output of this command: 
`grep gnulinux /boot/grub/grub.cfg`).

Example GRUB_DEFAULT:
```
"gnulinux-advanced-b9283994-ad47-412a-8662-81957a75ab4d>gnulinux-4.4.0-142-generic-advanced-b9283994-ad47-412a-8662-81957a75ab4d"
```
Update grub to make the changes:
```
$ sudo update-grub
```

> **Reverting to latest version:**
To revert to latest kernel just need to uncomment the line 
#GRUB_DEFAULT=0 in /etc/default/grub again.

Then reboot the system.

### Disable KASLR

Dune does not support kernel address space layout randomization (KASLR). For newer kernels, you must specify the nokaslr parameter in the kernel command line. Check before inserting the module by executing  `cat /proc/cmdline`. If the command line does not include the nokaslr parameter, then you must add it. In order to add it in Ubuntu-based distributions, you must:

-   edit the file  `/etc/default/grub`,
-   append  `nokaslr`  in the  `GRUB_CMDLINE_LINUX_DEFAULT`  option,
-   execute  `sudo grub-mkconfig -o /boot/grub/grub.cfg`, and
-   reboot the system.


### Install the default NIC  drivers
Without this step, the default ethernet did not show up after reboot. 

Follow [these](https://askubuntu.com/questions/1067564/intel-ethernet-i219-lm-driver-in-ubuntu-16-04) instructions to install the driver manually.

It takes a couple of minuets for system to get DNS settings. Manually Add these lines to `/etc/resolv.conf` (with sudo access):
```
nameserver 142.58.233.11
nameserver 142.58.200.2
search cmpt.sfu.ca
```

> Side note: On cs-nsl-55 when keyboard is connected, the system won't boot until you press F1! That is because of the [Dell's Cover openned alert](https://www.dell.com/support/kbdoc/en-ca/000150875/how-to-reset-or-remove-an-alert-cover-was-previously-removed-message-that-appears-when-starting-a-dell-optiplex-computer)! (not resolved)

## Dependencies and Environment
### Build Server Code
The setup.sh script installs the required library and components for running the worker code and RocksDB. 
```
cd ./server_code/shinjuku-rocksdb/
./setup.sh
```
In summary the script does the following: (1) Installs libraries (e.g libconfig-dev), (2) fetches Shinjuku dependency repositories (e.g., DPDK), (3) Applies our patch for Dune to make it run on the lab machines, (4) Builds the fetched projects and (5) inserts the kernel modules.
 
#### After Reboot
The "after_boot.sh" script should be run after each reboot.  It adds the kernel modules, allocates the needed 2MB hugepages for DPDK, and unbinds the 10G NIC from Kernel driver. 
> Note: might need to modify the NIC name for the machine in the script as instructed (e.g., 0000:01:00.0)



# Setting up Clients
> This is only needed for the machine that runs DPDK client.

Setup the DPDK environments variables (or alternatively, add them permanently):
```
export RTE_SDK=/home/<user>/dpdk/dpdk-stable-16.11.1
export RTE_TARGET=x86_64-native-linuxapp-gcc
```

```
cd client_code/client/tools
./tools.sh install_dpdk
./setup.sh
```

### Build 
```
cd ./client_code/client/
make
```

## Worker Configurations
In ```server_code/shinjuku_rocksdb``` the config files for each machine used in our testbed is provided. On each machine rename the corresponding config and name it as "shinjuku.conf".
For example, on machine cs-nsl-55:
```
cp shinjuku.conf.nsl-55 shinjuku.conf
```
### Allocated CPU Cores
The line "***cpu"*** in the shinjuku.conf refers to the ID of cores that will be allocated for Shinjuku. First two work as *"networker"* and *"dispatcher"* and the rest of them will be *"workers"* serving the tasks. 

### NIC Device
The line ***"devices"*** specifies the PCI device ID for the NIC that will be used for shinjuku. 
> We used one port of NICs, as 10Gbps is more than the capacity of a single machine (#tasks/s that a machine can handle can not saturate 10G). Also, note that I tried this setup on Ramses to use two different NICs but Shinjuku did not work properly with two NICs in configurations and had an issue (seems like concurrency bug) for sending reply packets.

### Worker IDs
In our experiments we use the following two setups for worker placement. Each setup requires configuring the Shinjuku using shinjuku.conf file. 
Depending on the intended setup, the line in shinjuku conf that mentions ***"port"*** should be modified with the assigned IDs to the worker cores. The lines are already in the configuration templates and just need to uncomment the line based on the intended setup.
These port IDs used for matching with ```hdr.dst_id``` in packets to queue the task in the corresponding queue for the worker and also makes sure that task will run on the  worker core selected by the switch scheduler.
> Note: dst_id as selected by switches is 0-based (in controller we use 0-n IDs) but in Shinjuku these ids start from 1, our modified code takes this into consideration and port IDs in the shinjuku.conf should be 1-based index.


#### Balanced/Uniform Setup
The figure below shows the uniform (i.e., balanced) placement setup. 
The python controller in switch codes puts one machine per rack (by logically dividn the register space). Note that the IDs assigned to worker cores are based on a hardcoded parameter in the switch which defines the boundaries for array indexes of each rack. E.g., in our case we used 16 as the boundary, so the IDs for racks start at 1-17-33-49 and etc.
> In real-world setup, this boundary should be set to max. expected leaves per cluster for spine switches and max. number of workers for each vcluster per rack.
 
 ![Worker Setup Uniform](./figs/placement_uniform.png)

#### Skewed Setup
The figure below shows the setup and assgined worker IDs  for the Skewed worker placement. The boundaries for arrays are similar to the previous setup. 

![Worker Setup Skewed](./figs/placement_skewed.png)



## Running Experiments
### On Workers
1.  Make sure that shinjuku.conf is correct according to previous part of the instructions. 
2. Run Shinjku (and RocksDB) using ```./build_and_run.sh```. The script will make a fresh copy of the DB, builds shinjuku and runs it. 
3. Wait for the outputs that mention worker is ready. 

### On clients
Run the client based using the following command:
``` 
sudo ./build/dpdk_client -l 0,1 -- <args>
```
**Arguments:** 
The first arg ```-l``` is input for DPDK and tells it to use two cores (0,1). One core will process sending loop and another core will handle the receive loop for reply packets.
The rest of args are handled by our code:
* ```-l```: (is) **L**atency client?; Type: bool. 
Inputs:
 1: runs RX loop and records the response times. 
 0: Just sends the packets (used for multiple clients case)
* ```-d```: **D**istribution; Type: string;
Inputs: 
"db_bimodal": Runs RocksDB requests with 90%GET-10%SCAN.
"db_port_bimodal": Runs RocksDB requests with 50%GET-50%SCAN. 
* ```-q```: Req. rate (**Q**PS). Type: int;
Input: 
An integer speciying the rate per second. The requests will have an exponential inter-arrival time where mean inter-arrival is calculated based on this parameter.
* ```-r```: (is) **R**ocksDB; Type: bool;
Input: 
1: Means using rocksDB (*We only use this setup*)
0: Means using synthetic workloads

* ```-n```: Experiment **N**ame; Type: String;
Input: 
String attached to the result file name to distinguish the different expeirments. E.g. "rs_h" or "saqr".

#### Running multiple clients
For rates less than or equal to 200KRPS, we use one machine (cs-nsl-62) and for higher rates we used two machines (cs-nsl-42 and cs-nsl-62).

> We ran simple experiments to make sure that the bottleneck is not at the request generator. In that experiment, we sent packets from client to switch and  the switch sent back every packet immediatly to the client. We measured the response time as we increased the request  rate. The results  showed that around ~240KRPS the client gets saturated and the delays start to increase and before this point the delays were consistant and low (few microseconds).  Therefore, we avoid generating higher rates than 200K using *one* machine. 

For clients, we use ID 110 for nsl-62 and ID 111 for nsl-42. Also, we used spine scheduler ID 100 in our experiment. This ID is assigned to every switch in the network (in spine p4 code we have the same ID). These are  defined as constants in dpdk_client.c. 

To generate the loads we used this setup:
- For load <= 200K: Use cs-nsl-62 only. 
- For 200K < load <= 300K: Generate 100K on nsl-42 and the rest on nsl-62.
 - For load > 300K: Generate 200K on nsl-42 and the rest on nsl-62.
To do so, run the client on the desired machines. Example:

```
sudo ./build/dpdk_client -l 0,1 -- -l 1 -d db_bimodal -q 30000 -r 1 -n saqr
```


## Known Issues
### Dune hardware compatibility issue
Sumamry of the issue and root cause (as far as we know) are below. TD;LR To run our nsl-5* machines Intel(R) Xeon(R) E-2186G CPU, we need to disable Dune's APIC Interrupt calls. Also, we disable the preemption mechanism in Shinjuku (it's by design;  as we did not intend to evaluate the server scheduler). In our experiments we do not rely on preemption feature of Shinjuku as worker cores have dedicated task queues. 
When running Shinjuku it freezes when calling the init function of Dune.  
It freezes just after  [this line](https://github.com/kkaffes/dune/blob/78c6679a993b9e014d0f7deb030dc5bbd0abe0b8/libdune/entry.c#L481). CPU gets stock after creating vCPU.

>For Dune, [this repo](https://github.com/ix-project/dune) works fine on our machine but the one Shinjuku uses is from this repo [this repo](https://github.com/kkaffes/dune). The problem is that the second repo adds some new APIs and functionalities to Dune that are necessary for Shinjuku so it is not possible to simply use the first Dune. 

>The issue was related to the usage of Advanced Programmable Interrupt Controllers (APIC) in Dune. It seems like there are multiple operations on the "hardware-specific register" and the address of these registers and values were hardcoded in Dune codes. I think there might be changes in these hardware-specific registers in the next-gen Intel CPUs and that's why it worked on Ramses but didn't work on nsl-55 (the CPUs are from different year/generations).
