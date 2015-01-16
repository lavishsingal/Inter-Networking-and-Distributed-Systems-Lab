#include <sys/socket.h>
#include <net/ethernet.h>
//#include <netpacket/packet.h>
#include <arpa/inet.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>

int errno;


/* thread utilities */
#define TOTAL_THREADS 3
pthread_t thrd[TOTAL_THREADS];

struct thrd_args{
    int sockfd;
    int if_index;
    char interface[5];
    struct thrd_args *next;
};

struct thrd_args *thread_anchor = NULL;

//u_char dest_mac[6] = { 0x12 , 0x13 , 0x14 ,0x15 , 0x16 , 0x17 };
struct sockaddr_ll sol_send;

typedef struct mypacket{
	int my_protocol;	
	uint16_t src_name;
	uint16_t dest_name;
	uint16_t seqno;
	char payload[1490];
}packet;

void fillsll(struct thrd_args *thr_walker ){

	int ifindex ;
	struct ifreq ifr;

    bzero(&sol_send, sizeof(struct sockaddr_ll));

	strncpy(ifr.ifr_name,thr_walker->interface,IFNAMSIZ);
	if(ioctl(thr_walker->sockfd, SIOCGIFINDEX, &ifr) < 0)
	{
		 perror("SIOCGIFINDEX");
		 exit(1);
	}
	ifindex = ifr.ifr_ifindex;

	sol_send.sll_family = PF_PACKET;
	sol_send.sll_protocol=htons(0x0800);  // what to put here
    sol_send.sll_ifindex = ifindex;
    sol_send.sll_hatype = 1;
    sol_send.sll_pkttype = PACKET_OTHERHOST;
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

int validate_it(packet *recvpacket){
	if(recvpacket->my_protocol == 19){
		return 1;
	}
	return 0;
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


void *manage_traffic(void* args){
    struct thrd_args *recv = args;
    
    int sockfd = recv->sockfd , length = -1 , sent = -1;
    
    packet *recvpacket = (packet *)malloc(sizeof(packet));
    
    if(sockfd < 0)
    {
        perror("socket() error");
        close(sockfd);
        free(args);
        exit(-1);
    }else{
        printf("socket() - Using SOCK_RAW socket and listening on socket number = %d\n",sockfd);
    }
    
    set_promisc( recv->interface , sockfd );
    while(1)
    {
        struct thrd_args *current = thread_anchor;
        length = recvfrom(sockfd, (unsigned char *)recvpacket, 1500, 0, NULL, NULL);
        
        if(length <  0){
            perror("Error while recieving !!\n");
            exit(1);
        }
        
        if(validate_it(recvpacket)){
            //printf("packet recvd; %s\n , now forwarding it", recvpacket->payload );
            //printf("mac address on which i'm receiving %d", recvpacket->dest_name);
            
            while (current->next != NULL ) {
                if(current->if_index == recvpacket->dest_name){
                    break;
                }
                current = current->next;
            }
            
            fillsll(current);
            sent = sendto(sockfd, recvpacket, 1500, 0, (struct sockaddr*)&sol_send, sizeof(sol_send));

        }
    }
    
}


int create_threads(struct thrd_args *myargs , int i){
    int myerrno = -1 ;
    if((myerrno = pthread_create(&thrd[i], 0,manage_traffic, (void*)myargs ))){
        fprintf(stderr, "pthread_create[0] %s\n",strerror(myerrno));
        pthread_exit(0);
    }
}

void join_threads(int n){
	int i =0 , myerrno =0;
    for (i=0; i< n;i++) {
        if((myerrno = pthread_join(thrd[i], 0))){
            fprintf(stderr, "pthread_join[i] %s\n",strerror(myerrno));
            pthread_exit(0);
        }
    }
}

/* we in this function are mapping interface name , its index , its socket and maintaining in the linkedlist */
void fill_interface_map(int if_count , char *argv[]){
    int i =0 ;
    struct thrd_args *args;
    
    for (i = 1 ; i<= if_count; i++) {
        struct thrd_args *current = thread_anchor;
        args = (struct thrd_args*)malloc(sizeof(struct thrd_args));
        if(args == NULL){
            printf("Error while malloc!\n");
            exit(1);
        }
        args->sockfd = create_rawsocket(ETH_P_ALL);
        strncpy(args->interface, argv[i],5);
        args->if_index = i;
        create_threads(args , i);
        args->next = NULL;
        if(thread_anchor == NULL){
            thread_anchor = args;
        } else {
            while (current->next != NULL) {
                current = current->next;
            }
            current->next = args;
        }
    }
}

int main(int argc, char *argv[])
{
    int if_count = 0 , i = 0;
    struct thrd_args *args;

    printf("WARNING: If you entered incorrect/wrong interface this program won't run as expected\n");
    
    if(argc <= 1){
        printf("please enter the interface you want to listen and send...exiting\n");
        exit(1);
    }
    if_count = argc - 1;
    
    printf("Listening on %d interfaces\n",if_count);
    
    fill_interface_map(if_count , argv);
    
	//create_threads(if_count , argv);
	join_threads(if_count);
	return 0;
}

