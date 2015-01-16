How you can run the code?

First you need to compile the code .
For that you must have the below files mandatory:

master:
	1) master.c
	2) ntpapis.c
	3) ntpapis.h
	4) myntp.conf
	5) Makefile

slave:
	1) slave.c
	2) ntpapis.c
	3) ntpapis.h
	4) myntp.conf
	5) Makefile

Just run “make” and the following executables will be created:

in the master 

There are two folders:

1) slave
    - master.txt
    - myntp
    - ntpapis.o
    - slave.o
    - slave.txt

2) master
    - slave.txt
    - myntp
    - ntpapis.o
    - master.o



Two text files:
1) master.txt
    this file contains following information:
    1) masterIP address
    2) Interface
    3) InterfaceIP addresss of the node

    The order is very important two follow:
    <masterIP>   <Interface>  <Myip>

   You will fill this text file by ifconfig it and check which node's IP will act as master for this client and enter interface and ip accordingly.


   For. eg. If server's IP address is 10.1.1.3 from where you want to sync and 10.1.1.2 with interface eth0 , so in the file it should be:

   10.1.1.3 eth0 10.1.1.2

2) Similarly for slave.txt

    <slaveIP>   <Interface>  <myIP>

if you dont have slave (in case of MASTER node) , then don't create the slave.txt and viceversa.



After this is configured:

1) Run code on the master node using command:
    ./myntp
2) Then run the same command on the intermediate node.
3) Finally run the same command on the client node.



NOTE: Client will have only master.txt file and MASTER will have slave.txt whereas intermediate nodes will have both files as they act as slave for MASTER node and master for SLAVE node.

The node will run for 50 iterations and will give you offset by setting time and calculating each time .

After the 50 iterations are complete a file will be generated:

Offset_readings.txt which will contain the various offset calculated .

Also there is myntp.conf file:

Configuration file:

/* how many samples do you need */
#define NO_OF_SYNC 50
/* what is the interarrival sleep time between your packets */
#define SLEEPTIME 1
/* Port where you want to listen and send */
#define CNTP_PORT 5345
/* Maximum buffer size */
#define MAXBUFF 50
/* Maximum number of interfaces */
#define NUM_OF_IF  3
/* file name you put the detailed ip configuration of slave and master */
#define SLAVEFILE "slave.txt"
#define MASTERFILE "master.txt"


  




