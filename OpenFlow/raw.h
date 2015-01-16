//
//  rawsock.h
//  Custom Protocol
//
//  Created by Lavish Singal on 10/19/14.
//  Copyright (c) 2014 Lavish Singal. All rights reserved.
//

#ifndef __Custom_Protocol__rawsock__
#define __Custom_Protocol__rawsock__

#include <stdio.h>

#include <stdio.h>
/* for uint16_t */
#include <stdint.h>
/* ifr*/
#include <sys/ioctl.h>
#include <net/if.h>
/* for stat */
#include <sys/stat.h>
/*for PACKET_OTHER macro*/
#include <linux/if_packet.h>
/* for mmap and other macros */
#include <sys/mman.h>
/* for O_RDONLY*/
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
/* for sll */
#include <sys/socket.h>
#include <linux/if_ether.h>
//#include <net/ethernet.h> /* the L2 protocols */
/* for open/close */
#include <fcntl.h>


#define FILENAME ""
#define FILEPATH_C "textfile_back.txt"
#define MAXSEND 20
#define PAYLOAD 1490

typedef struct mypacket{
    int my_protocol;
    uint16_t src_name;
    uint16_t dest_name;
    uint16_t seqno;
    char payload[PAYLOAD];
}packet;

typedef struct nak_packet{
    int my_protocol;
    uint16_t dest_node;
    uint16_t src_node;
    int seqno;
}nak_pack;

void error(const char *msg);
int iterateArrayAgain();
char *openFILE(char *str);
int deleteNodesFromArray(char **arrayNode , int seqNum);
int set_promisc(char *interface, int sock ) ;
int create_rawsocket(int protocol_to_sniff);
int validate_it(void* recvpacket , int src_node);
struct sockaddr_ll *fillsll(char *iface , int sockfd);



#endif /* defined(__Custom_Protocol__rawsock__) */
