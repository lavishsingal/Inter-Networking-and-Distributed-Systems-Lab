
#ifndef __NTPAPISH__
#define __NTPAPISH__


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <asm/types.h>
#include <linux/net_tstamp.h>
#include <linux/errqueue.h>

#include <assert.h>
#include <signal.h>

#include <limits.h>

#include "myntp.conf"

#ifndef SO_TIMESTAMPING
# define SO_TIMESTAMPING         37
# define SCM_TIMESTAMPING        SO_TIMESTAMPING
#endif

#ifndef SO_TIMESTAMPNS
# define SO_TIMESTAMPNS 35
#endif

#ifndef SIOCGSTAMPNS
# define SIOCGSTAMPNS 0x8907
#endif

#ifndef SIOCSHWTSTAMP
# define SIOCSHWTSTAMP 0x89b0
#endif

/* how many sync iteration you need */

typedef struct ntpPacket{
	struct timeval t1;
	struct timeval t2;
	struct timeval t3;
	struct timeval t4;
	int stratum;
	struct timeval mastertime;
	int networkdelay;
	int seqnum;
	int oldseqnum;
	int ifsync;
	int setclock;
	int offset;
}ntpPacket;

typedef struct tspkt{
	struct timeval sendtime;
	struct timeval recvtime;
	int seqnum;
	struct tspkt *next;
}tspkt;

typedef struct thrd_args{
    int sockfd;
    char interface[MAXBUFF];
    char slaveip[MAXBUFF];
    struct in_addr myip;
    struct thrd_args *next;
}ntpinfo;

void createThread(pthread_t *threadid , void *(*func) (void *) , ntpinfo *info);
int ifPacketReady(int sock);
int setSOCflags(int sockfd , int ifsend);
int ntprecvfrom(ntpPacket *recv_pkt , int sock , int recvmsg_flags);
int ntpsendto(char *ip , int seq , ntpPacket *send_pkt , int fd);
struct timeval sendTimeofPacket(int seqnum);
struct timeval recvTimeofPacket(int seqnum);
int calcdelayerror(ntpPacket *packet);
void printpacket(struct msghdr *msg, int res,char *data,int sock, int recvmsg_flags , ntpPacket *recv_pkt);
void joinThreads(int count , pthread_t *threadIds);
int createUDPSocket();
int bindAddress(int sock , int reuseaddr , struct in_addr *ip );
int addtstolist(int seqnum , struct timeval *sendts , struct timespec *recvts);
int calcnetdelay(ntpPacket *packet , int stratum);
int updatetime(ntpPacket *packet);
int freetslist();

ntpinfo *addntpinfotolist(ntpinfo **ntp_anchor , struct in_addr *ip , int sendsock , char *iface ,char *slaveip );
void freentpinfolist();
int getifaceIP(struct ifreq *device ,char *iface , int sock );
FILE *openFile(char *filename);
ntpinfo *fillntpinfo(char *slaveip , char *iface , char *myip , void *(*func) (void *) , pthread_t *thread , int threadcount);


#endif
