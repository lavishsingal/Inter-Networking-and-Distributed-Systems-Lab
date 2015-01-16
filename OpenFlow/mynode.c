//
//  myclient.c
//  Custom Protocol
//
//  Created by Lavish Singal on 10/19/14.
//  Copyright (c) 2014 Lavish Singal. All rights reserved.
//


#include <stdio.h>
/* for threads */
#include <pthread.h>
/* for L2 layers
 #include <sys/socket.h>
 #include <linux/if_packet.h>
 #include <net/ethernet.h>*/
#include "raw.h"


#define MYPROTOCOL 19
#define MAXPAYLOAD 1500
#define IFSIZE 5

int src_node;
int dest_node[2];
int flag = 0;
int dest_flag = 0;
int recvflag=1 , retrasmitflag = 1;;

struct stat statbuf;
char *data = NULL;
char interface[IFSIZE] = "";
/* one thread for reciving and one for sending naks*/
pthread_t thrd[2];

struct sockaddr_ll *sol;
int iteratingptr = 0;

off_t datasize;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
//pthread_cond_t serverQ = PTHREAD_COND_INITIALIZER;


typedef struct nodeTracker{
    uint16_t node_index;
    char *retras;
    FILE *fp;
    int totalPacketsArrived;
    int recievedComplete;
    int finishRestramit;
    struct nodeTracker *next;
}nodeTracker;

nodeTracker *track_head = NULL;

//char *retrans2 , *retrans3;


off_t num_of_packets(){
    off_t no_pkts;
    no_pkts = datasize/PAYLOAD;
    if(datasize % (PAYLOAD) != 0 )
        no_pkts++;
    
    return no_pkts;
}

nodeTracker *add_node(uint16_t index){
    nodeTracker *newNode = NULL , *current = track_head;
    newNode = (nodeTracker*)malloc(sizeof(nodeTracker));
    if(!newNode){
        printf("memory issue\n");
        exit(1);
    }
    newNode->node_index = index;
    newNode->totalPacketsArrived = 0;
    newNode->recievedComplete = 0;
    newNode->next = NULL;
    newNode->finishRestramit = 0;
    newNode->retras = calloc(num_of_packets(), sizeof(char));
    newNode->fp = fopen("myfile","w");
    if(newNode->fp == NULL){
        printf("some error in file opening\n");
        exit(1);
    }
    
    if(current == NULL){
        track_head = newNode;
    }else {
        newNode->next = current;
        track_head = newNode;
    }
    return newNode;
}

nodeTracker *ifexist(uint16_t index){
    nodeTracker *current = track_head;
    
    while (current != NULL) {
        if(current->node_index == index){
            return current;
        }
        current = current->next;
    }
    return NULL;
}

int if_all_data_recvd(){
    nodeTracker *current = track_head;
    
    if(current == NULL){
        return 0;
    }
    
    while(current != NULL){
        if( current->recievedComplete != 1){
            return 0;
        }
        current = current->next;
    }
    
    return 1;
}

int if_all_retras_done(){
    nodeTracker *current = track_head;
    
    if(current == NULL){
        return 0;
    }
    
    while(current != NULL){
        if( current->finishRestramit != 1){
            return 0;
        }
        current = current->next;
    }
    
    return 1;
}

/* calculate packet size based on seqnum */
off_t calc_packet_size(uint16_t seqnum){
    off_t totalseq , leftbyte , size_p;
    
    totalseq =  datasize / PAYLOAD;
    leftbyte = datasize % PAYLOAD;
    //
    size_p = PAYLOAD;
    if(leftbyte != 0 && totalseq ==  seqnum)
    {
        size_p = leftbyte;
    }
    return size_p + 10;
}

int ifPacketArrived(char **allNodes , int seqNum){
    
    if( seqNum >=0 && seqNum <=num_of_packets()-1){
        if( *((*allNodes)+seqNum) == 0 ){
            *((*allNodes)+seqNum) = (char)1;
         //   printf("seqNum = %d",seqNum);
            return 1;
        } else {
            return 0;
        }
    }
    return 0;
}

/* this function should check each condition to terminate the program
 * 1. if all clients connected has sent the ACKs or finishRestramit = 1 for all nodes
 * 2. if all clients sending data recievedComplete = 1
 */

int alldone(){
    if (if_all_data_recvd() && if_all_retras_done()) {
        return 1;
    }
    return 0;
}


int iterateArray(char *allNodes){
    int i ;
    off_t totalSeq;
    
    //  printf("Iterator: %d\n",iteratingptr);
    
    //printf("iterate array called\n");
    totalSeq = num_of_packets()-1;
    
    if( allNodes == NULL ){
        return 0;
    }
    for ( i = iteratingptr; i <= totalSeq ; i++)
    {
        if (allNodes[i] == 0 )
        {
            if (i==totalSeq)
                iteratingptr=0;
            else
                iteratingptr=(i+1);
            return i;
        }
    }
    iteratingptr=0;
    return 0;
    
}

void sendACKS(int sockfd){
    nak_pack sendACK;
    int i =0 , sentsize;
    /* send some ACKs so that retrasmission thread breaks */
    for( i = 0 ; i < 20 ; i++){
        //            printf("sending the final ACK that I got the file\n");
        sendACK.seqno = -1;
        sendACK.my_protocol = 99;
        sendACK.src_node = src_node;
        sendACK.dest_node = dest_node[0];
        
        
        sentsize = sendto(sockfd, &sendACK, sizeof(nak_pack), 0, (struct sockaddr*)sol, sizeof(struct sockaddr_ll));
        if(dest_flag){
            sendACK.dest_node = dest_node[1];
            sentsize = sendto(sockfd, &sendACK, sizeof(nak_pack), 0, (struct sockaddr*)sol, sizeof(struct sockaddr_ll));
        }
        if(sentsize < 0){
            printf("error !!! sending ACKs\n");
        }
    }

}
void* recvThread(void* arg){
    int length  = -1 , sockfd = -1 , sentsize = -1;
    off_t no_pkts = 0;
    packet recvpacket , sendPacket;
    nodeTracker *current = track_head;
    int count , protocol = 0;
    nak_pack *n_packet , sendNACK;
    off_t size_p;
    nodeTracker *node;
    
    
    printf("\n****************** Recieving packets ****************** \n");
    
    no_pkts = num_of_packets();
    
    //   printf("num of packets = %lld\n",no_pkts);
    
    sockfd = create_rawsocket(ETH_P_ALL);
    if(sockfd < 0){
        error("Socket Creation");
    }
    
    set_promisc( interface , sockfd );
    
    while (1) {
        
        if(alldone()){
            sendACKS(sockfd);
            printf("\n****************** ALL DONE ******************\n");
            break;
        }
        
        if(if_all_retras_done()){
         //   printf("flag set to 0");
            flag = 0;
        }
        
        length = recvfrom(sockfd, (void*)&recvpacket, 1500, 0, NULL, NULL);
        
        if(length <  0){
            error("Error while recieving !!\n");
        }
        if( (protocol = validate_it((void*)&recvpacket , src_node)) == 19 ){
       //     printf("Got my packet: seqnum = %d, length = %d\n",recvpacket.seqno, length);
            //printf("Got my packet: destname = %d\n",recvpacket.dest_name);
            nodeTracker *current = NULL ;
            
            if(!(current = ifexist(recvpacket.src_name))){
                current = add_node(recvpacket.src_name);
                if(!current){
                    printf("some error in adding the node\n");
                    exit(1);
                }
            }
            /* current , is the node where he is recieving the data */
            if(ifPacketArrived( &(current->retras) , recvpacket.seqno)){
         //       printf("checking and updating the if packet thing\n line 183\n");
                fseek( current->fp , PAYLOAD*recvpacket.seqno , SEEK_SET  );
                //fwrite(&recvpacket.payload , calc_packet_size(recvpacket.seqno) , 1 , current->fp);
                fwrite(&recvpacket.payload , length - 10 , 1 , current->fp);
                //fwrite(&recvpacket.payload , 27 , 1 , current->fp);
                fflush(current->fp);
                current->totalPacketsArrived += 1;
                count++;
            }
            if(current->totalPacketsArrived == no_pkts ){
                if(recvflag){
                    printf("\n****************** All packets recieved ******************\n");
                    recvflag = 0;
                }
                current->recievedComplete = 1;
            }
            if(if_all_data_recvd()){
                sendACKS(sockfd);
                //printf("All data recived\n");
                continue;
            }
//            printf("count = %d\n",count);
            
        } /* when packet for retrasmission */ else if (protocol == 99){
            
            flag = 1;
            //     printf("flag set to 1");
            
            n_packet = (nak_pack*)&recvpacket;
            
            if(n_packet == NULL){
                printf("n_packet is NULL\n");
                
            }
            //   printf("got the nack packet while retramission \n");
            if(n_packet->seqno == -1){
                if(!(node = ifexist(n_packet->src_node))){
                    node = add_node(n_packet->src_node);
                    if(!node){
                        printf("some error in adding the node\n");
                        exit(1);
                    }
                }
                node->finishRestramit = 1;
                if(retrasmitflag){
                    printf("\n****************** Retrasmission complete ******************\n");
                    retrasmitflag = 0;
                }
                //       printf("All packets recived, just continue\n");
                continue;
            }
            /* bound it , otherwise it will crash */
            if( n_packet->seqno > num_of_packets()-1 || n_packet->seqno < 0){
                printf("bound in retramission thread with seqnum=%d\n",n_packet->seqno);
                continue;
            }
            
            
            size_p = calc_packet_size(n_packet->seqno);
            
            // printf("Got the seqnum = %d for retrasmission, with size of = %d\n",n_packet->seqno,size_p);
            
            pthread_mutex_lock(&lock);
            
            memcpy(sendPacket.payload , &data[(n_packet->seqno)*PAYLOAD] , size_p);
            
            pthread_mutex_unlock(&lock);
            
            sendPacket.dest_name = dest_node[0] ;/* any , but its wrong*/
            sendPacket.src_name = src_node;
            sendPacket.my_protocol = MYPROTOCOL;
            sendPacket.seqno = n_packet->seqno;
            
  //          printf("sending the nack\n");
    //        printf(":::::%d",sendPacket.seqno);
            //   printf("%s",sendPacket.payload);
            
            sentsize = sendto(sockfd, &sendPacket, size_p, 0, (struct sockaddr*)sol, sizeof(struct sockaddr_ll));
            if(dest_flag){
                sendNACK.dest_node = dest_node[1];
                sentsize = sendto(sockfd, &sendPacket, size_p, 0, (struct sockaddr*)sol, sizeof(struct sockaddr_ll));
            }
            
            if(sentsize < 0){
                printf("error !!! in retransmission\n");
            }
            
            //     pthread_mutex_unlock(&lock);
        }
    }
}


/* this thread will send the NACK if there is any packet missing or send seqnum = -1 when all packets are recived*/
void *NACKThread(void *arg){
    nodeTracker *current = track_head;
    nak_pack sendNACK;
    int sentsize = -1;
    off_t requestIndex = 0;
    //  int sockfd = (*((int*)arg)) ;
    int sockfd ;
    
    sockfd = create_rawsocket(ETH_P_ALL);
    if(sockfd < 0){
        error("Socket Creation");
    }
    
    printf("\n****************** Sending NACKs if required ******************\n\n");
    
    while (!(if_all_data_recvd())) {
        //sleep(5);
    //    printf("struct in NACK thread, current = %p\n", current);
        while (flag) ;
        if(current == NULL){
            requestIndex = iterateArray(NULL);
            current = track_head;
        }else {
            requestIndex = iterateArray(current->retras);
            if( requestIndex == num_of_packets()-1){
                sleep(1);
                current = current->next;
                if(current == NULL){
                    current = track_head;
                }
            }
        }
     //   printf("requestIndex =  %d\n", (int)requestIndex);
        /* make the nack*/
        sendNACK.seqno = (int)requestIndex;
        sendNACK.my_protocol = 99;
        sendNACK.dest_node = dest_node[0];
        sendNACK.src_node = src_node;
        
        // printf("Send NACK packet for missing sequence number= %d\n",requestIndex);
        sentsize = sendto(sockfd, &sendNACK, sizeof(nak_pack), 0, (struct sockaddr*)sol, sizeof(struct sockaddr_ll));
        if(dest_flag){
            sendNACK.dest_node = dest_node[1];
            sentsize = sendto(sockfd, &sendNACK, sizeof(nak_pack), 0, (struct sockaddr*)sol, sizeof(struct sockaddr_ll));
        }
        
        if(sentsize < 0){
            printf("error !!! in main\n");
        }
    }
    
}


void join_threads(){
    int i =0;
    for (i = 0; i<2; i++) {
        pthread_join(thrd[i], 0);
    }
}

int main(int argc , char *argv[]){
    
    int fp = -1;
    int sockfd , seq = 0 , sentsize = -1 , status = -1 , errno = -1;
    off_t chunk = 0, share = 0 , filesize = 0;
    
    if(argc <= 1 ){
        printf("USAGE: ifname src_name dest_name ..exiting\n");
        exit(1);
    }
    
    strncpy(interface, argv[1], IFSIZE);
    src_node = atoi(argv[2]);
    
    dest_node[0] = atoi(argv[3]);
    
    if(argc == 5){
        dest_flag = 1;
        dest_node[1] = atoi(argv[4]);
    }
    
    /* create a socket */
    sockfd = create_rawsocket(ETH_P_ALL);
    if(sockfd < 0){
        error("Socket Creation");
    }
    
    /* fill the sockaddr_ll for sending the raw socket */
    sol = (struct sockaddr_ll*)malloc(sizeof(struct sockaddr_ll));
    sol = fillsll(interface , sockfd) ;
    
    /* read the file to be send */
    if ((fp = open(FILENAME, O_RDONLY)) == -1)
    {
        error("fopen:");
    }
    status = fstat(fp,&statbuf);
    if(status==-1){
        printf("File not found\n");
    }
    
    pthread_mutex_lock(&lock);
    
    data = openFILE(FILENAME);
    datasize = statbuf.st_size;
    
    filesize = datasize;
    
    pthread_mutex_unlock(&lock);
    
    sleep(5);
    /* create a thread so that it can start reciving the data as well */
    
    if((errno = pthread_create(&thrd[0], 0,recvThread, (void*)0 ))){
        fprintf(stderr, "pthread_create[0] %s\n",strerror(errno));
        pthread_exit(0);
    }
    
    
    while (filesize > 0) {
        packet sendPacket;
        memset(&sendPacket , 0 , sizeof(packet));
        
        share = filesize;
        chunk = PAYLOAD;
        
        if(share - chunk < 0){
            chunk = share;
        } else {
            share = share - chunk;
        }
        
        pthread_mutex_lock(&lock);
        
        memcpy(sendPacket.payload , &data[seq*PAYLOAD] , chunk);
        
        pthread_mutex_unlock(&lock);
        
        sendPacket.dest_name = dest_node[0] ;/* any */
        sendPacket.src_name = src_node;
        sendPacket.my_protocol = MYPROTOCOL;
        sendPacket.seqno = seq;
        
        int total_size = 10 + chunk;
        
        
        usleep(100);
        //sentsize = sendto(sockfd, &sendPacket, MAXPAYLOAD, 0, (struct sockaddr*)sol, sizeof(struct sockaddr_ll));
        sentsize = sendto(sockfd, &sendPacket, total_size, 0, (struct sockaddr*)sol, sizeof(struct sockaddr_ll));
        if(dest_flag){
            sendPacket.dest_name = dest_node[1];
            sentsize = sendto(sockfd, &sendPacket, total_size, 0, (struct sockaddr*)sol, sizeof(struct sockaddr_ll));
        }
        
        if(sentsize < 0){
            printf("error !!! in main\n");
        }
        seq += 1;
        filesize -= chunk;
        //        printf("sending seq num: %d\n",seq);
    }
    
    if((errno = pthread_create(&thrd[1], 0,NACKThread, (void*)0 ))){
        fprintf(stderr, "pthread_create[0] %s\n",strerror(errno));
        pthread_exit(0);
    }
    
    //printf("total rcvd :%d",track_head->totalPacketsArrived);
    join_threads();
    close(fp);
    close(sockfd);
    return 0;
    
}

