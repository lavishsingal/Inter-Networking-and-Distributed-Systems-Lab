#include "ntpapis.h"


/* global variable stratum */

int stratum = -1; /* slave dont know its stratum until it recieves a packet */
/* total 3 interface max */
pthread_t thread[NUM_OF_IF];
FILE *masterfd = NULL, *slavefd = NULL;



//ntpinfo *ntp_anchor = NULL;
int threadcount = 0;


int synced = 0;
int slavesync = 0 , totalsync = 0;

void *slavesyncproc(void *args){
	/* send pacekt to the slave */
	ntpinfo *recvInfo = args;
	ntpPacket send_pkt , recv_pkt , err_pkt;
	struct timeval t2 , t3;
	int res;


	/* start listening to all the interface*/

	while(recv_pkt.ifsync != NO_OF_SYNC){
//		printf("---------Sync = %d\n",recv_pkt.ifsync);
		memset(&recv_pkt , 0 , sizeof(ntpPacket));
		memset(&send_pkt , 0 , sizeof(ntpPacket));

		if(ifPacketReady(recvInfo->sockfd)){
			res = ntprecvfrom(&recv_pkt , recvInfo->sockfd , 0);
			/* send the packet to the master with t2 and t3 in it*/
			memset(&send_pkt , 0 , sizeof(ntpPacket));
			t2 = recvTimeofPacket(recv_pkt.seqnum );
			send_pkt.t2.tv_sec = t2.tv_sec;
			send_pkt.t2.tv_usec = t2.tv_usec;
		//	printf("time (T2):   %lus.%lus\n", t2.tv_sec , t2.tv_usec);
			stratum = recv_pkt.stratum +1;
			send_pkt.stratum = stratum;
			gettimeofday(&t3 , 0);
		//	printf("time (T3):   %lus.%lus\n", t3.tv_sec , t3.tv_usec);
			send_pkt.t3.tv_sec = t3.tv_sec;
			send_pkt.t3.tv_usec = t3.tv_usec;
			if(recv_pkt.setclock == 1){
				printf("Got full packet with seq num = %d\n",recv_pkt.seqnum);
				updatetime(&recv_pkt);
				ntpsendto(recvInfo->slaveip , recv_pkt.seqnum , &recv_pkt ,  recvInfo->sockfd);
			}else {
				ntpsendto(recvInfo->slaveip , recv_pkt.seqnum , &send_pkt , recvInfo->sockfd);
			}
			ntprecvfrom(&err_pkt ,  recvInfo->sockfd , MSG_ERRQUEUE);

//			printf("Sent back with seq num: %d\n",recv_pkt.seqnum);

		}
	//	sleep(5);
	}
	slavesync = 1;
	return;
}

void *mastersyncproc(void *args){
	/* send pacekt to the slave */
	ntpinfo *sendInfo = args;

	int seqnum = 0 , res = 0;
	ntpPacket send_pkt , recv_pkt , err_pkt;
	int delay;
	struct timeval send;

	memset(&recv_pkt , 0 , sizeof(ntpPacket));

	/* send to this ip*/

	while(1){
		if(slavesync == 1 && totalsync == NO_OF_SYNC+1 )
			break;
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
					/*  update sync number only when the slave itself is synced*/
					if(slavesync)
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
	if(slavefd){
		printf("%s closed\n",MASTERFILE);
		fclose(slavefd);
		slavefd = NULL;
	}

	printf("Safely Terminated\n");


}


int main(int argc, char **argv){
	int  rc , set_resuse_addr = 1;
	int num;
	char slaveip[20] = {0,} , line[50]={0,} , myip[20] = {0,} , iface[20] = {0,};
	ntpinfo *slave;


	sigset(SIGINT, freeresources);

	masterfd = openFile(SLAVEFILE);
	slavefd = openFile(MASTERFILE);

	if(masterfd){
		while(fgets(line , sizeof(line), masterfd)){
			fgets( line, sizeof(line), masterfd );
			/*SlaveIP       Interface     myIP       */
			num = sscanf( line, "%s %s %s\n", slaveip, iface, myip );

			slave = fillntpinfo(slaveip , iface , myip , mastersyncproc , thread ,threadcount);
			if(slave == NULL)
				freeresources(1);
			threadcount++;
		}
	}

	if(slavefd){
		while(fgets(line , sizeof(line), slavefd)){
			struct in_addr ip_inaddr;
			fgets( line, sizeof(line), slavefd );
			/*SlaveIP       Interface     myIP       */
			num = sscanf( line, "%s %s %s\n", slaveip, iface, myip );

			slave = fillntpinfo(slaveip , iface , myip , slavesyncproc ,thread ,threadcount );
			threadcount++;
		}
	}

	joinThreads(threadcount , thread);

	if(masterfd){
		printf("%s closed\n",SLAVEFILE);
		fclose(masterfd);
		masterfd = NULL;
	}
	if(slavefd){
		printf("%s closed\n",MASTERFILE);
		fclose(slavefd);
		slavefd = NULL;
	}

	return 1;
}

