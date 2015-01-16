#include "raw.h"

struct sockaddr_ll *fillsll(char *iface , int sockfd){
    
    int ifindex ;
    struct ifreq ifr;
    struct sockaddr_ll *sol_send;
    
    bzero(sol_send, sizeof(struct sockaddr_ll));
    
    strncpy(ifr.ifr_name ,iface,IFNAMSIZ);
    if(ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0)
    {
        perror("SIOCGIFINDEX");
        exit(1);
    }
    ifindex = ifr.ifr_ifindex;
    
    sol_send->sll_family = PF_PACKET;
    sol_send->sll_protocol=htons(0x0800);  // what to put here
    sol_send->sll_ifindex = ifindex;
    sol_send->sll_hatype = 1;
    sol_send->sll_pkttype = PACKET_OTHERHOST;
    return sol_send;
}

int create_rawsocket(int protocol_to_sniff)
{
    int rawsock;
    if((rawsock = socket(AF_PACKET, SOCK_RAW, htons(protocol_to_sniff)))== -1)
    {
        perror("Error creating raw socket: ");
        exit(-1);
    }
    return rawsock;
}

int validate_it(void *recvpacket , int src_node){
    packet *my_packet = (packet*)recvpacket;
    nak_pack *nak_packet = (nak_pack*)recvpacket;
    
    if( my_packet->my_protocol == 19 && my_packet->dest_name == src_node ){
        return 19;
    } else if(nak_packet->my_protocol == 99 && nak_packet->dest_node == src_node ) {
        return 99;
        
    } else {
        return 0;
    }
    
  /*  if(recvpacket.my_protocol == 19 && recvpacket.dest_name == src_node ){
        return 1;
    }
    return 0;*/
}


int set_promisc(char *interface, int sock ) {
    struct ifreq ifr;
    strncpy(ifr.ifr_name, interface,strlen(interface)+1);
    if((ioctl(sock, SIOCGIFFLAGS, &ifr) == -1)) {
        perror("Could not retrive flags for the interface");
        exit(0);
    }
    
    ifr.ifr_flags |= IFF_PROMISC;
    if (ioctl (sock, SIOCSIFFLAGS, &ifr) == -1 ) {
        perror("Could not set the PROMISC flag:");
        exit(0);
    }
    return(0);
}


char *openFILE(char *str){
    int fp;
    int pagesize;
    char *data;
    struct stat statbuf;

    
    fp = open(str, O_RDONLY);
    if(fp < 0 ){
        error("Error: while opening the file\n");
    }
    
    /* get the status of the file */
    if(fstat(fp,&statbuf) < 0){
        error("ERROR: while file status\n");
    }
    
    /* mmap it , data is a pointer which is mapping the whole file*/
    data = mmap((caddr_t)0, statbuf.st_size , PROT_READ , MAP_SHARED , fp , 0 ) ;
    
    if(data == MAP_FAILED ){
        error("ERROR : mmap \n");
    }
    
    return data;
    
}

void error(const char *msg)
{
    perror(msg);
    exit(1);
}
