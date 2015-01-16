
#include "ntpapis.h"


tspkt *timeanchor = NULL;
ntpinfo *ntp_anchor = NULL;
int prevOffset = INT_MAX;


//char payload[10] = "1234567890";
static const unsigned char payload[] = {
	0x00,
};

/* calculate the T1 timestamping using SO_TI**** APIS*/

struct timeval sendTimeofPacket(int seqnum){
	/* give me seq num and I will give you the tx of that packet */
	tspkt *current = timeanchor;
	struct timeval error;
	if(timeanchor == NULL){
		/* no such value */
		goto error;
	} else {
		do{
			if(current->seqnum == seqnum){
				return current->sendtime;
			}
			current = current->next;
		}while(current != NULL);
	}
	error:
	error.tv_sec = -1;
	error.tv_usec = -1;
	return error;
}

struct timeval recvTimeofPacket(int seqnum){
	/* give me seq num and I will give you the tx of that packet */
	tspkt *current = timeanchor;
	struct timeval error;
	if(timeanchor == NULL){
		/* no such value */
		goto error;
	} else {
		do{
			if(current->seqnum == seqnum){
				return current->recvtime;
			}
			current = current->next;
		}while(current != NULL);
	}
	error:
	error.tv_sec = -1;
	error.tv_usec = -1;
	return error;
}


int bindAddress(int sock , int reuseaddr , struct in_addr *ip ){
	struct sockaddr_in  inInAddr;
	int ret , set_option_on = 1;

    inInAddr.sin_addr.s_addr    =  ip->s_addr; //inet_addr(ip); /*INADDR_ANY;*/
    inInAddr.sin_port           = htons(CNTP_PORT);
    inInAddr.sin_family         = AF_INET;

    if(reuseaddr){
    	ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*) &set_option_on, sizeof(set_option_on));
        if(0 > ret){
        	perror("UDP SO_REUSEADDR\n");
        	//exit(1);
        	return -1;
        }
      	ret = setsockopt(sock, SOL_SOCKET, 15, (char*) &set_option_on, sizeof(set_option_on));
        if(0 > ret){
            	perror("UDP SO_REUSEADDR\n");
            	//exit(1);
            	return -1;
         }
    }

    ret = bind(sock, (struct sockaddr *) &inInAddr, sizeof(struct sockaddr_in));
     if (0 > ret){
         perror("UDP bind() failed:\n");
         return -1;
     }
     return 1;
}

int createUDPSocket(){
	int sock;
	sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0){
		perror("socket");
		return -1;
		//exit(1);
	}
	return sock;
}

int addtstolist(int seqnum , struct timeval *sendts , struct timespec *recvts){
	tspkt *current = timeanchor , *node;

	/* check if node with seqnum exists */
	do{
		if(current == NULL)
			break;
		if(seqnum == current->seqnum){
			break;
		}
		current = current->next;
	}while(current != NULL);

	if(current == NULL){
		current = timeanchor;
		node = (tspkt*)malloc(sizeof(tspkt));
		if(node == NULL){
			printf("SOme memory issue:\n");
			return -1;
			//exit(1);
		}
		/*copy data into node */
		node->next = NULL;
		node->seqnum = seqnum;

		if(sendts == NULL ){
			node->recvtime.tv_sec = recvts->tv_sec;
			node->recvtime.tv_usec = (recvts->tv_nsec)/1000;
		} else if(recvts == NULL){
			node->sendtime.tv_sec = sendts->tv_sec;
			node->sendtime.tv_usec = sendts->tv_usec;
		}

		if(timeanchor == NULL){
			timeanchor = node;
		}else{
			while(current->next != NULL){
				current = current->next;
			}
			current->next = node;
		}
	} else {
		/* node already exists */
		if(sendts == NULL ){
			current->recvtime.tv_sec = recvts->tv_sec;
			current->recvtime.tv_usec = (recvts->tv_nsec)/1000;
		} else if(recvts == NULL){
			current->sendtime.tv_sec = sendts->tv_sec;
			current->sendtime.tv_usec = sendts->tv_usec;
		}
	}

	return 1;
}

int freetslist(){
	tspkt *current = timeanchor;
	if(timeanchor == NULL){
		return 1;
	} else {
		while(current != NULL){
			free(current);
			current = current->next;
		}
	}
	return 1;
}

/* send packet to specific interface IP */
int ntpsendto(char *ip , int seq , ntpPacket *send_pkt , int fd){

	struct sockaddr_in daddr;
	struct timeval t1;
	int ret;
	//ntpPacket send_pkt;

	memset(&daddr, 0, sizeof(daddr));

	daddr.sin_family	= AF_INET;
	daddr.sin_port		= htons(CNTP_PORT);
	daddr.sin_addr.s_addr	= inet_addr(ip);

	send_pkt->seqnum = seq;

	//gettimeofday(&t1, NULL);
	//printf("time (app):    %lus.%luus\n", t1.tv_sec, t1.tv_usec);


	ret = sendto(fd, send_pkt, sizeof(ntpPacket), 0,
			     (void *) &daddr, sizeof(daddr));
	if (ret != sizeof(ntpPacket)) {
		perror("sendto");
		return -1;
	}
	return 1;
}

int ntprecvfrom(ntpPacket *recv_pkt , int sock , int recvmsg_flags){

	char data[256];
	struct msghdr msg;
	struct iovec entry;
	struct sockaddr_in from_addr;
	struct {
		struct cmsghdr cm;
		char control[512];
	} control;
	int res;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &entry;
	msg.msg_iovlen = 1;
	entry.iov_base = data;
	entry.iov_len = sizeof(data);
	msg.msg_name = (caddr_t)&from_addr;
	msg.msg_namelen = sizeof(from_addr);
	msg.msg_control = &control;
	msg.msg_controllen = sizeof(control);

	res = recvmsg(sock, &msg, recvmsg_flags );
	if (res < 0) {

	/*	printf("%s %s: %s\n",
		       "recvmsg",
		       (recvmsg_flags & MSG_ERRQUEUE) ? "error" : "regular",
		       strerror(errno));*/
	} else {
		if(!(recvmsg_flags & MSG_ERRQUEUE)){
			memcpy(recv_pkt , data , sizeof(ntpPacket));
		//	printf("seq we got:=%d\n",recv_pkt->seqnum  );
		} else {
			memcpy((void*)recv_pkt, data + res - sizeof(ntpPacket), sizeof(ntpPacket));
		}
		printpacket(&msg, res, data,
			    sock , recvmsg_flags , recv_pkt);
	}
	return res;
}

void printpacket(struct msghdr *msg, int res,
			char *data,
			int sock , int recvmsg_flags , ntpPacket *recv_pkt)
{
	struct cmsghdr *cmsg;
	ntpPacket payload ;
	struct timeval *sendTime = NULL ;
	struct timespec *recvTime = NULL;

	for (cmsg = CMSG_FIRSTHDR(msg);
	     cmsg;
	     cmsg = CMSG_NXTHDR(msg, cmsg)) {
		switch (cmsg->cmsg_level) {
		case SOL_SOCKET:
			switch (cmsg->cmsg_type) {
			case SO_TIMESTAMP: {
				sendTime = (struct timeval *)CMSG_DATA(cmsg);
			/*	printf("Recieving Timestamp(T4) ");
				printf(" %ld.%06ld",
				       (long)sendTime->tv_sec,
				       (long)sendTime->tv_usec);*/
				break;
			}
			case SO_TIMESTAMPING: {
				recvTime = (struct timespec *)CMSG_DATA(cmsg);
			/*	printf("Sending Timestamp(T1) ");
				printf(" %ld.%06ld ",
				       (long)recvTime->tv_sec,
				       ((long)recvTime->tv_nsec/1000));*/
				break;
			}
			default:
				printf("type %d", cmsg->cmsg_type);
				break;
			}
			break;
		case IPPROTO_IP:
			switch (cmsg->cmsg_type) {
			case IP_RECVERR: {
				struct sock_extended_err *err =
					(struct sock_extended_err *)CMSG_DATA(cmsg);
				assert(err);
				if (res < sizeof(ntpPacket))
					printf(" => truncated data?!");
				break;
			}
			default:
				printf("type %d", cmsg->cmsg_type);
				break;
			}
			break;
		default:
			printf("level %d type %d",
				cmsg->cmsg_level,
				cmsg->cmsg_type);
			break;
		}
	//	printf("\n");
	}
	//printf("Recieved Packet : %s",(recvmsg_flags & MSG_ERRQUEUE) ? "errorQPacket\n" : "regularPacket\n");
	//printf("Seqnum = %d\n",recv_pkt->seqnum);
	if(recvmsg_flags & MSG_ERRQUEUE){
		addtstolist(recv_pkt->seqnum , sendTime , NULL);
	} else {
		addtstolist(recv_pkt->seqnum , NULL , recvTime);
	}
}

int setSOCflags(int sockfd , int ifsend){
	int flags;
	if(ifsend){
		flags = SOF_TIMESTAMPING_TX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE;
		if (setsockopt(sockfd, SOL_SOCKET, SO_TIMESTAMPING, &flags, sizeof(flags))) {
			perror("setsockopt SO_TIMESTAMPING");
			return -1;
		}
	}else {
		flags = 1;
		if (setsockopt(sockfd, SOL_SOCKET, SO_TIMESTAMP,
				   &flags, sizeof(flags)) < 0){
			perror("setsockopt SO_TIMESTAMP");
			return -1;
		}
	}
	return 1;
}

int setMulticast(int sock){
	struct ip_mreq imr;
	struct in_addr iaddr;

	inet_aton("224.0.1.130", &iaddr); /* alternate PTP domain 1 */
	//addr.sin_addr = iaddr;
	imr.imr_multiaddr.s_addr = iaddr.s_addr;
	imr.imr_interface.s_addr = INADDR_ANY;
	//	((struct sockaddr_in *)&device.ifr_addr)->sin_addr.s_addr;
	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,
		       &imr.imr_interface.s_addr, sizeof(struct in_addr)) < 0){
		perror("set multicast");
		return -1;//(1);
	}
	/* join multicast group, loop our own packet */
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		       &imr, sizeof(struct ip_mreq)) < 0){
		perror("join multicast group");
		return -1;
	}

	return 1;
}

/* this function will use select syscall and check if the packet is ready to receive*/
int ifPacketReady(int sock){

	fd_set readfs, errorfs;
	int res;
	struct timeval delta;

	/* timer for 5 sec timeout */
	delta.tv_sec = 5;
	delta.tv_usec = 0;

	FD_ZERO(&readfs);
	FD_ZERO(&errorfs);
	FD_SET(sock, &readfs);
	FD_SET(sock, &errorfs);

	res = select(sock + 1, &readfs, 0, &errorfs, &delta);

	if (res > 0) {
		if (FD_ISSET(sock, &readfs)){
			//printf("ready for reading\n");
			return 1;
		}
		if (FD_ISSET(sock, &errorfs)){
			printf("has error\n");
			return 0;
		}
	} else {
		return 0;
	}
	return 0;
}


/* create the number of threads according to interface */
void createThread(pthread_t *threadid , void *(*func) (void *) , ntpinfo *info){
	/* write now we dont need interface */
    int myerrno = -1 ;
    if((myerrno = pthread_create(threadid, 0,func, (void*)info ))){
        fprintf(stderr, "pthread_create[0] %s\n",strerror(myerrno));
        pthread_exit(0);
    }
}


/* join if threads */

void joinThreads(int count , pthread_t *threadIds){
	int i =0 , myerrno =0;
	for (i=0; i< count;i++) {
		if((myerrno = pthread_join(threadIds[i], 0))){
			fprintf(stderr, "pthread_join[i] %s\n",strerror(myerrno));
			pthread_exit(0);
    	}
	}
}

long time_diff(struct timeval x , struct timeval y)
{
  long x_ms, y_ms, msec;
  x_ms = (long)x.tv_sec*1000000 + (long)x.tv_usec;
  y_ms = (long)y.tv_sec*1000000 + (long)y.tv_usec;
  msec = (long)y_ms - (long)x_ms;
  return msec;
}

void removefromlist(int seqnum){
	tspkt *current = timeanchor;

	do{
		if(current->seqnum == seqnum){
	//		return current->sendtime;
		}
		current = current->next;
	}while(current != NULL);

	if(current == NULL){
		printf("SOmeting wrong in logic\n");
		exit(1);
	}

	/* adjust the node */
	free(current);
//	return;

}

/* calculate the network delay */
int calcnetdelay(ntpPacket *packet , int stratum){
	assert(stratum < packet->stratum);
	struct timeval t1 , t2 , t3 , t4 ;
	int delay;
	int offset;
	FILE *fp;

	t1 = sendTimeofPacket(packet->seqnum);
	t2 = packet->t2;
	t3 = packet->t3;
	t4 = recvTimeofPacket(packet->seqnum);

	(packet->t1).tv_sec = t1.tv_sec;
	(packet->t1).tv_usec = t1.tv_usec;

	(packet->t4).tv_sec = t4.tv_sec;
	(packet->t4).tv_usec = t4.tv_usec;


	printf("**************** 	SEQ NUM = %d  	*******************\n",packet->seqnum);
	printf("**************** T1 = %lus.%lus *****************\n", t1.tv_sec , t1.tv_usec);
	printf("**************** T2 = %lus.%lus *****************\n", t2.tv_sec , t2.tv_usec);
	printf("**************** T3 = %lus.%lus *****************\n", t3.tv_sec , t3.tv_usec);
	printf("**************** T4 = %lus.%lus *****************\n", t4.tv_sec , t4.tv_usec );


	delay = time_diff(t1 , t4) - time_diff(t2 , t3);
	offset = (time_diff(t1 , t2) + time_diff(t4 , t3))/2;
	printf("***************   Network Delay = %d ******************** \n",delay);
	printf("**************** 	offset = %d  *******************\n",offset);

	packet->networkdelay = delay;
	packet->offset = offset;

	fp = fopen("Offset_readings.txt","a+");

	fprintf(fp , "%d\n" , offset);

	/* do later memory leak*/
	//removefromlist(packet->seqnum);
	fclose(fp);

	return delay;
}


/* calculate the delay error due to jitter */
int calcdelayerror(ntpPacket *packet){
	int t3jitter;
	struct timeval t3kernel = sendTimeofPacket(packet->oldseqnum);
	t3jitter = packet->networkdelay + time_diff(packet->t4 , packet->t1) + time_diff(packet->t2 , t3kernel);
	return t3jitter;
}

int updatetime(ntpPacket *packet){
	int  recvjitter;
	struct timeval tuser , tkernel , tfinal;
	int sectoadd , usectoadd , t3jitter;


	printf("PrevOffset = %d and New Offset = %d\n",prevOffset , packet->offset );
	if(abs(prevOffset) < abs(packet->offset))
		return 1;
	prevOffset = packet->offset;

	/* faulty */
	t3jitter = calcdelayerror(packet);

	printf("t3jitter = %d\n",t3jitter);
	printf("Delay = %d\n",packet->networkdelay);

	usectoadd = (packet->networkdelay) % 999999;
	sectoadd = (packet->networkdelay) / 999999;


	gettimeofday(&tuser , 0);
	tkernel = recvTimeofPacket(packet->seqnum);
	recvjitter = time_diff(tkernel , tuser);

	tfinal.tv_sec = (packet->mastertime).tv_sec + (sectoadd/2) +SLEEPTIME;
	tfinal.tv_usec = (packet->mastertime).tv_usec + recvjitter + (usectoadd/2) - t3jitter;

	settimeofday(&tfinal , 0);

	printf(" tkernel = %ld.%06ld :: tuser = %ld.%06ld \n",
	       (long)tkernel.tv_sec,
	       (long)tkernel.tv_usec,
	       (long)tuser.tv_sec,
	       (long)tuser.tv_usec
	       );

	return 1;
}

FILE *openFile(char *filename){
	FILE *fd;
	fd = fopen(filename , "r");

	if(fd == NULL){
		printf("No slave or master of this client exist \n");
		return NULL;
	}
	return fd;
}

int getifaceIP(struct ifreq *device ,char *iface , int sock ){
	memset(device, 0, sizeof(device));
	strncpy(device->ifr_name, iface, sizeof(device->ifr_name));
	if (ioctl(sock, SIOCGIFADDR, device) < 0){
		perror("getting interface IP address");
		return -1;
		//exit(1);
	}
	return 1;
}

ntpinfo *addntpinfotolist(ntpinfo **ntp_anchor , struct in_addr *ip , int sendsock , char *iface ,char *slaveip ){
	ntpinfo *add;
	ntpinfo *current = *ntp_anchor;
     add = (ntpinfo*)malloc(sizeof(ntpinfo));
     if(add == NULL){
         printf("Error while malloc!\n");
         exit(1);
     }
     add->sockfd = sendsock;
     strncpy(add->interface  ,iface ,sizeof(iface)+1);
     strncpy(add->slaveip  ,slaveip ,sizeof(slaveip)+1);
     strncpy((char*)&add->myip  ,(char*)ip ,sizeof(ip)+1);
     add->next = NULL;
     if(*ntp_anchor == NULL){
         *ntp_anchor = add;
     } else {
         while (current->next != NULL) {
             current = current->next;
         }
         current->next = add;
     }

     return add;
}

void freentpinfolist(){
	ntpinfo *current = ntp_anchor;
	if(ntp_anchor == NULL){
		printf("Something wrong with list..NO elements\n");
		exit(1);
	} else {
		while(current != NULL){
			free(current);
			current = current->next;
		}
	}
//	return 1;
}



ntpinfo *fillntpinfo(char *slaveip , char *iface , char *myip , void *(*func) (void *) , pthread_t *thread , int threadcount){
	ntpinfo *info;
	struct ifreq device;
	int ret , sendsock;
	struct in_addr ip_inaddr;


	sendsock = createUDPSocket();
	if(sendsock < 0)
		freeresources(1);
		//goto free;

	ret = setSOCflags(sendsock , 1);
	if(ret < 0)
		freeresources(1);
		//goto free;
	ret = setSOCflags(sendsock , 0);
	if(ret < 0)
		freeresources(1);
		//goto free;

	ret = getifaceIP(&device , iface , sendsock);
	if(ret < 0)
		freeresources(1);
		//goto free;
	ip_inaddr = ((struct sockaddr_in *)&device.ifr_addr)->sin_addr;

	/* store ip , sendsock , ifaceIP to ll */
	info = addntpinfotolist(&ntp_anchor , &ip_inaddr , sendsock , iface ,slaveip );
	if(info == NULL){
		printf("Somehting wrong in LL\n");
		return NULL;
		//goto free;
	}

	ret = bindAddress(sendsock , 1 , &ip_inaddr);
	if(ret < 0)
		freeresources(1);
		//goto free;

	createThread(&thread[threadcount] , func , info );
	printf("thread create = %d",threadcount);

	return info;

}



