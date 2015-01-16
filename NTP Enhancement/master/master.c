#include "ntpapis.h"

/* global variable stratum */


int stratum = 0;
/* total 3 interface max */
pthread_t thread[NUM_OF_IF];

int totalsync = 0;
int synced = 0;
int threadcount = 0;
FILE *masterfd = NULL;


void *mastersyncproc(void *args){
	/* send pacekt to the slave */
	ntpinfo *sendInfo = args;
	int seqnum = 0 , res = 0;
	ntpPacket send_pkt , recv_pkt , err_pkt;
	int delay;
	struct timeval send;

	memset(&recv_pkt , 0 , sizeof(ntpPacket));

	/* send to this ip*/

	while(totalsync != NO_OF_SYNC+1){
		memset(&send_pkt , 0 , sizeof(ntpPacket));
		send_pkt.seqnum = seqnum;
		send_pkt.stratum = stratum;
		send_pkt.ifsync = totalsync ;
		if(synced){
			synced = 0;
			ntpsendto(sendInfo->slaveip , seqnum , &recv_pkt , sendInfo->sockfd);
		}else {
			ntpsendto(sendInfo->slaveip , seqnum , &send_pkt , sendInfo->sockfd);
		}

		printf("Send seq num= %d",seqnum);
	//	sendTimeofPacket(seqnum);
		if(ifPacketReady(sendInfo->sockfd)){
			res = ntprecvfrom(&err_pkt , sendInfo->sockfd , MSG_ERRQUEUE|MSG_DONTWAIT);
			printf("RES of error:%d\n",res);
			res = ntprecvfrom(&recv_pkt , sendInfo->sockfd , MSG_DONTWAIT);
			printf("RES of regular:%d\n",res);
			printf("Seq num gt from regular: %d\n",recv_pkt.seqnum);
			printf("Seq num gt from error q: %d\n",err_pkt.seqnum);

			if(res > 0){
				if(recv_pkt.stratum > stratum){
					synced = 1;
					/* we got reply from the slave and got t2 and t3, now calculate network delay*/
					delay = calcnetdelay(&recv_pkt , stratum);
					/* copy this delay into the packet and send to the slave with gettimeofday time */
					//recv_pkt.ifsync = 1;
					recv_pkt.stratum = stratum;
					recv_pkt.setclock = 1;
					gettimeofday(&send , 0);
					recv_pkt.mastertime.tv_sec = send.tv_sec;
					recv_pkt.mastertime.tv_usec = send.tv_usec;
					recv_pkt.oldseqnum = recv_pkt.seqnum;
					/*  update sync number */
					 totalsync++;
					printf("Got confirmation, %d\n",recv_pkt.setclock);
				}
			}
		}
		seqnum++;
		/* send packet after 5 sec*/
		sleep(SLEEPTIME);
	}
}

void freeresources(int signo) {
	int errno ;
	int i = 0;
	printf("\nFreeing the resources\n");

	for(i =0 ;i < threadcount ; i++){
		printf("ThreadID%d killed\n",i);
		if((errno = pthread_cancel(thread[i]))){
	        fprintf(stderr, "pthread_cancel[0] %s\n",strerror(errno));
	    }
	}

	printf("Other resources freed!!\n");
	freentpinfolist();
	freetslist();

	if(masterfd){
		printf("%s closed\n",SLAVEFILE);
		fclose(masterfd);
		masterfd = NULL;
	}

	printf("Safely Terminated\n");

}


int main(int argc, char **argv){
	int num , ret;
	struct ifreq device;
	char slaveip[20] = {0,} , line[50]={0,} , myip[20] = {0,} , iface[20] = {0,};
	ntpinfo *master;

	sigset(SIGINT, freeresources);

	/* it can have many slaves */
	masterfd = openFile(SLAVEFILE);

	if(masterfd){
		while(fgets(line , sizeof(line), masterfd)){
			fgets( line, sizeof(line), masterfd );
			/*SlaveIP       Interface     myIP       */
			num = sscanf( line, "%s %s %s\n", slaveip, iface, myip );
			master = fillntpinfo(slaveip , iface , myip , mastersyncproc , thread , threadcount);
			if(master == NULL)
				freeresources(1);
			threadcount++;
		}
	}
	joinThreads(threadcount , thread);

	if(masterfd){
		printf("File closed\n");
		fclose(masterfd);
		masterfd = NULL;
	}

	return 0;
}



