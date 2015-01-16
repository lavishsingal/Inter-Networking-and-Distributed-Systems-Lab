#include <sys/socket.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include "client.h"
#include <errno.h>

int errno;

#define UDP_PORT 17
#define SRC_NAME 1
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

char interface[5] = "";

long int filesize, no_pkts,ttlpkt,ptr=0;
pthread_t threadid_1, threadid_2 ;
unsigned char *txbuf;
struct sockaddr_ll sol;
int ifindex;
struct ifreq ifr;
int last_pktsize;
int sockfd,sockfd2;
long int max=0;
int *retrans;

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

int validate_it(packet recvpacket ){
    if(recvpacket.my_protocol == 19 && recvpacket.dest_name == 1 ){
        return 1;
    }
    return 0;
}


long int missed_pkts()
{
    int i = 0, min = 0;
    
    for ( i = min; i < max ; i++)
    {
        if (retrans[i] == 0 )
        {
            if (i==no_pkts-1)
                min=0;
            else
                min=(i+1);
            return i;
        }
    }
    min=0;
    return -1;
    
}


void* nakthr(void * t)
{
    int parser = 0 , sendack = 0 , n = 0;

    nak_pack nak;
    printf("Coming to nak thread\n");
    
    usleep(5);
    while(1)
    {
        usleep(50);
        //pthread_mutex_lock(&mutexlist);
        parser = missed_pkts();
        
        //Change to make - add socket structure //
       	if(parser == -1 && ttlpkt == no_pkts)
       	{
            /* sending the acks */
            for(sendack=0; sendack<20; sendack++)
            {
                ifindex = ifr.ifr_ifindex;
                
                sol.sll_family = PF_PACKET;
                sol.sll_protocol=htons(0x0800);
                sol.sll_ifindex = 2;
                sol.sll_hatype = 1;
                sol.sll_pkttype = PACKET_OTHERHOST;
                
                int i;
                for (i = 0; i<8; i++) {
                    //sol.sll_addr[i] = tosend->dest_mac[i];
                    sol.sll_addr[i] = 0x00;
                }
                
                sol.sll_halen = 6;
                
                memset(&nak,0,sizeof(nak));
                
                nak.dest_node = 2;
                nak.seqno = 111;
                nak.my_protocol = 99;
                
                //sleep(0.5);
                socklen_t sol_size = sizeof(sol);
                n = sendto(sockfd,&nak,sizeof(nak),0,(struct sockaddr*)&sol, sol_size);
                printf("an error: %s\n", strerror(errno));
                //sleep(2);
                printf("All packets are received*******\n");
                printf("sending ack: number of bytes sent %d\n", n);
            }
            
            break;
            
       	}
        
        else
        {
            
            if(parser == -1)
            {
                
            }
            else
            {
                int i =0;
                ifindex = ifr.ifr_ifindex;
                
                sol.sll_family = PF_PACKET;
                sol.sll_protocol=htons(0x0800);
                sol.sll_ifindex = 2;
                sol.sll_hatype = 1;
                sol.sll_pkttype = PACKET_OTHERHOST;
                
                for (i = 0; i< 8; i++) {
                    //sol.sll_addr[i] = tosend->dest_mac[i];
                    sol.sll_addr[i] = 0x00;
                }
                
                sol.sll_halen = 6;
                
                memset(&nak,0,sizeof(nak));
                
                nak.dest_node = 2;
                nak.seqno = parser;
                nak.my_protocol = 99;
                
                //sleep(0.5);
                socklen_t sol_size = sizeof(sol);
                n = sendto(sockfd,&nak,sizeof(nak),0,(struct sockaddr*)&sol, sizeof(sol));
                printf("an error: %s\n", strerror(errno));
                //sleep(2);
                printf("sending nak: number of bytes sent %d\n", n);
            }
        }
        
    }
    
}


void* recvthr(void* t)
{
    
    int length;
    long int fptr;
    packet recvpacket;
    FILE* fp;
    
    fp = fopen("clientfile.txt","w");
    
    last_pktsize = FILESIZE - ((no_pkts-1)*PAYLOAD_SIZE);
    
    retrans=(int *)calloc(no_pkts, sizeof(int));
    
    while(1)
    {
        length = recvfrom(sockfd2, &recvpacket, 1500, 0, NULL, NULL);
        
        if(length <  0){
            perror("Error while recieving !!\n");
            exit(1);
        }
        
        
        if(validate_it(recvpacket))
        {
            
            printf("Length of data received %d\n", length);
            
            if(retrans[recvpacket.seqno]!=1)
            {
                
                if(recvpacket.seqno != (no_pkts-1))
                {
                    
                    fptr = recvpacket.seqno * PAYLOAD_SIZE;
                    fseek(fp,fptr,SEEK_SET);
                    fwrite(recvpacket.payload,PAYLOAD_SIZE,1,fp);
                    fflush(fp);
                    
                    printf("sequence number %u\n",recvpacket.seqno);
                    
                    pthread_mutex_lock(&lock);
                    retrans[recvpacket.seqno] = 1;
                    pthread_mutex_unlock(&lock);
                    
                    if(recvpacket.seqno > max){
                        max = recvpacket.seqno;
                    }
                    ttlpkt++;
                    
                }
                else
                {
                    
                    fseek(fp,(no_pkts-1)*PAYLOAD_SIZE,SEEK_SET);
                    fwrite(recvpacket.payload,last_pktsize,1,fp);
                    fflush(fp);
                    
                    printf("sequence number %u\n",recvpacket.seqno);
                    
                    pthread_mutex_lock(&lock);
                    retrans[recvpacket.seqno] = 1;
                    pthread_mutex_unlock(&lock);
                    
                    if(recvpacket.seqno > max)
                        max = recvpacket.seqno;
                    
                     ttlpkt++;
                    
                }
                
            }
            
            printf("Total packets received %ld\n\n",ttlpkt);
        }
        
        if(ttlpkt==no_pkts)
        {
            printf("\ntotal packets received equalled num packets   2***\n");
            break;
            
        }
    }//end of while
    printf(" exiting while\n");
    fclose(fp);
    printf(" exiting thread\n");
}






void* retrthr(void *t)
{
    
    unsigned char *addr;
    //unsigned int sockfd2 =0 , sockfd3 = 0;
    int n;
    
    packet pack;
    //nak_pack *nak=(nak_pack *)malloc(sizeof(nak_pack));
    nak_pack nak;
    //socklen_t clilen = sizeof(serv_addr);
    last_pktsize = FILESIZE - ((no_pkts-1)*PAYLOAD_SIZE);
    
    printf("retransmission thread started in client\n\n");
    
    
    while(1)
    {
        
        
        n = recvfrom(sockfd2, &nak, sizeof(nak), 0, NULL, NULL);
        printf("an error: %s\n", strerror(errno));
        
        sleep(0.5);
        if(n == 1500)
        {
            printf("number %d\n", n);
            printf("received protocol number %d\n", nak.my_protocol);
            
            break;
        }
        if(nak.my_protocol == 99 && nak.seqno == 111 && nak.dest_node == 1)
        {
            
            printf("I received -1 and all the packets have been received\n\n");
            break;
            
        }
        printf("received number bytes %d\n", n);
        printf("received protocol number %d\n", nak.my_protocol);
        if(nak.my_protocol == 99 && nak.dest_node == 1)
        {
            if(nak.seqno != (no_pkts-1))
            {
                ifindex = ifr.ifr_ifindex;
                
                sol.sll_family = PF_PACKET;
                sol.sll_protocol=htons(0x0800);
                sol.sll_ifindex = 2;
                sol.sll_hatype = 1;
                sol.sll_pkttype = PACKET_OTHERHOST;
                
                int i;
                for (i = 0; i<8; i++) {
                    //sol.sll_addr[i] = tosend->dest_mac[i];
                    sol.sll_addr[i] = 0x00;
                }
                
                sol.sll_halen = 6;
                
                ptr =  nak.seqno * PAYLOAD_SIZE;
                
                //pthread_mutex_lock(&mutexlist);
                memcpy(pack.payload,&txbuf[ptr],PAYLOAD_SIZE);
                //pthread_mutex_unlock(&mutexlist);
                pack.my_protocol=19;
                pack.seqno = nak.seqno;
                pack.src_name = 1;
                pack.dest_name= 2;
                
                printf("retransmission thread is sending %u\n", pack.seqno);
                sendto(sockfd,&pack,1500,0,(struct sockaddr*)&sol,sizeof(sol));
                //usleep(50);
                
            }
            else
            {
                ifindex = ifr.ifr_ifindex;
                
                sol.sll_family = PF_PACKET;
                sol.sll_protocol=htons(0x0800);
                sol.sll_ifindex = 2;
                sol.sll_hatype = 1;
                sol.sll_pkttype = PACKET_OTHERHOST;
                
                int i;
                for (i = 0; i<8; i++) {
                    //sol.sll_addr[i] = tosend->dest_mac[i];
                    sol.sll_addr[i] = 0x00;
                }
                
                sol.sll_halen = 6;
                
                int last_ptr = (no_pkts-1)*PAYLOAD_SIZE;
                
                //pthread_mutex_lock(&mutexlist);
                memcpy(pack.payload,&txbuf[last_ptr],last_pktsize);
                //pthread_mutex_unlock(&mutexlist);
                pack.my_protocol=19;
                pack.seqno = nak.seqno;
                pack.src_name = 1;
                pack.dest_name= 2;
                
                printf("retransmission thread is sending %u\n", pack.seqno);
                sendto(sockfd,&pack,1500,0,(struct sockaddr*)&sol,sizeof(sol));
                //usleep(50);
            }
            
            
            
        }
    }//end of while

}//end of retransmission thread

int main(int argc, char *argv[])
{
    
    int fd,status;
    long int counter =0;
    int one = 1,t=0;
    uint16_t seq=0;
    int last_pktsize;
    struct stat statbuf;
    
    
    unsigned char *mainbuffer;
    const int *val = &one;
    //packet *tosend = (packet *)malloc(sizeof(packet));
    packet tosend;
    char *inetadd;
    char macStr[50];
    int sentsize;
    int chunk = PAYLOAD_SIZE, share;
    
    unsigned char *buffer=(unsigned char*)malloc(100*sizeof(unsigned char));
    
    if(argc <= 1){
        printf("Please enter the interace you are connected to..exiting\n");
        exit(1);
    }
    strncpy(interface , argv[1] , 5);
    
    /*create raw socket and send UDP port number*/
    sockfd = create_rawsocket(ETH_P_ALL);
    sockfd2 = create_rawsocket(ETH_P_ALL);
    bzero(&sol, sizeof(struct sockaddr_ll));
    
    
    if(sockfd2 < 0)
    {
        perror("socket() error");
        // If something wrong just exit
        exit(-1);
    }
    else
        printf("socket() - Using SOCK_RAW socket and UDP protocol is OK.\n");
    
    set_promisc( interface , sockfd2 );
    
    if(sockfd < 0)
    {
        perror("socket() error");
        // If something wrong just exit
        exit(-1);
    }
    else
        printf("socket() - Using SOCK_RAW socket and listening to socket number %d.\n",sockfd);
    
    strncpy(ifr.ifr_name,interface,IFNAMSIZ);
    if(ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0)
    {
        perror("SIOCGIFINDEX");
        exit(1);
    }
    
    bzero(&sol, sizeof(struct sockaddr_ll));
    
    /* Read from file and find out the number of packets */
    fd = open("test.txt",O_RDONLY);
    
    if(fd == -1){
        printf("Please input the file . exiting\n");
        exit(1);
    }
    
    status = fstat(fd,&statbuf);
    
    if(status == -1){
        printf("File not found\n");
    }
    
    filesize = statbuf.st_size;
    
    no_pkts = (long int)filesize/PAYLOAD_SIZE;
    
    if(filesize%(PAYLOAD_SIZE)!= 0)
        no_pkts++;
    
    mainbuffer = (char *)mmap(0,filesize,PROT_READ,MAP_SHARED,fd,0);
    txbuf = mainbuffer;
    printf("Filesize is %ld and number of packets is %ld\n\n", filesize, no_pkts);
    share = filesize;
    usleep(10);
    while(counter < no_pkts)
    {
        int i;
        ifindex = ifr.ifr_ifindex;
        
        sol.sll_family = PF_PACKET;
        sol.sll_protocol=htons(0x0800);
        sol.sll_ifindex = ifindex;
        sol.sll_hatype = 1;
        sol.sll_pkttype = PACKET_OTHERHOST;
        
        for (i = 0; i<8; i++) {
            sol.sll_addr[i] = 0x00;
        }
        
        sol.sll_halen = 6;
        memset(&tosend,0,sizeof(tosend));
        //printf("mac address dest %s\n", dst_mac);
        tosend.my_protocol = 19;
        tosend.seqno = seq;
        tosend.dest_name = 2;
        tosend.src_name = SRC_NAME;
        memcpy(tosend.payload , &(mainbuffer[ptr]),chunk);
        //printf("mainbuffer is %d\n", mainbuffer[ptr]);
        //memcpy(buffer,(unsigned char *)tosend,75);
        
        //printf("buffer is \n%s\n", tosend.payload);
        usleep(20);
        sentsize = sendto(sockfd, &tosend, 1500, 0, (struct sockaddr*)&sol, sizeof(sol));
        printf("Sent number of bytes %d with sequence number %d\n\n",sentsize, tosend.seqno);
        
        seq++;
        
        share=share-chunk;
        if(share <= 0)
        {
            break;
        }
        
        if(share>=PAYLOAD_SIZE){
          		chunk=PAYLOAD_SIZE;
        }else {
            chunk=share;
        }
            
        printf("Remaining number of bytes %d\n\n",chunk);
        //sleep(1);
        counter++;
    }
    
    if(pthread_create(&threadid_1,NULL,recvthr,NULL)!=0)
    {
        printf("creation of thread %d not successful\n",t);
        pthread_exit(NULL);
    }
    //printf("*******Came back to main*********\n");
    if(pthread_create(&threadid_2,NULL,nakthr,NULL)!=0)
    {
        printf("creation of thread %d not successful\n",t);
        pthread_exit(NULL);
    }
    
    
    pthread_join(threadid_1,NULL);
    pthread_join(threadid_2,NULL);
    close(fd);
    close(sockfd);
    close(sockfd2);
    return 0;
    
}



