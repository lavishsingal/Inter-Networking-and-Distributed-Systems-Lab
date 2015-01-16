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

#define UDP_PORT 17

char interface[5] = "";

int errno;

int sockfd, sockfd2;
int last_pktsize;
long int max=0;
long int min=0;
pthread_attr_t attr;
int *retrans;
long int filesize, no_pkts = 0,ttlpkt,ptr=0;
pthread_t threadid_1, threadid_2,threadid_3;
struct sockaddr_ll sol;
unsigned char *txbuf;
int ifindex;
struct ifreq ifr;


int set_promisc(char *interface, int sock ) {
    //struct ifreq ifr;
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

int validate_it(packet recvpacket ){
	if(recvpacket.my_protocol == 19 && recvpacket.dest_name == 2 ){
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


long int missed_pkts()
{
	int i;
    
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
    char buff[1024] = "Hello" ;
	long int parser;
	int sendack,n;
    
    
    //nak_pack *nak=(nak_pack *)malloc(sizeof(nak_pack));
    nak_pack nak;
    printf("Coming to nak thread\n");

    

	//set_promisc( INTERFACE , sockfd2 );

    usleep(5);
	while(1)
	{
        usleep(50);
      	//pthread_mutex_lock(&mutexlist);
        parser = missed_pkts();


        //Change to make - add socket structure //
       	if(parser == -1 && ttlpkt == no_pkts)
       	{
            
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

                nak.dest_node = 1;
                nak.seqno = 111;
                nak.my_protocol = 99;

                //sleep(0.5);
               socklen_t sol_size = sizeof(sol);
       			n = sendto(sockfd2,&nak,sizeof(nak),0,(struct sockaddr*)&sol, sol_size);
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
                
        		printf("parser's value %ld\n", parser);
                ifindex = ifr.ifr_ifindex;
                
                sol.sll_family = PF_PACKET;
                sol.sll_protocol=htons(0x0800);
                sol.sll_ifindex = 2;
                sol.sll_hatype = 1;
                sol.sll_pkttype = PACKET_OTHERHOST;
                
                int i;
                for (i = 0; i< 8; i++) {
                    //sol.sll_addr[i] = tosend->dest_mac[i];
                    sol.sll_addr[i] = 0x00;
                }
                
                sol.sll_halen = 6;

                memset(&nak,0,sizeof(nak));

        		nak.dest_node = 1;
                nak.seqno = parser;
                nak.my_protocol = 99;

                //sleep(0.5);
                socklen_t sol_size = sizeof(sol);
        		n = sendto(sockfd2,&nak,sizeof(nak),0,(struct sockaddr*)&sol, sizeof(sol));
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
        length = recvfrom(sockfd, &recvpacket, 1500, 0, NULL, NULL);
        
		if(length <  0){
            perror("Error while recieving !!\n");
            exit(1);
        }


		if(validate_it(recvpacket))
		{
        	
        	printf("Length of data received %d\n", length);
        	//printf("Packet received at server \n%s\n", recvpacket.payload );
            
            // if(recvpacket.my_protocol == 19)
            //{
            
            if(retrans[recvpacket.seqno]!=1)
        	{
                
                if(recvpacket.seqno != (no_pkts-1))
  	 	    	{
                    
  			    	fptr = recvpacket.seqno * PAYLOAD_SIZE;
  				    fseek(fp,fptr,SEEK_SET);
                    fwrite(recvpacket.payload,PAYLOAD_SIZE,1,fp);
                    fflush(fp);
                    
    		    	printf("sequence number %u\n",recvpacket.seqno);
                    
                    //pthread_mutex_lock(&mutexlist);
    		    	retrans[recvpacket.seqno]=1;
                    //pthread_mutex_unlock(&mutexlist);
                    
    			    if(recvpacket.seqno > max)
                        max = recvpacket.seqno;
                    
                    
                    ttlpkt++;
                    
		     	}
                else
                {
                    
                    fseek(fp,(no_pkts-1)*PAYLOAD_SIZE,SEEK_SET);
                    fwrite(recvpacket.payload,last_pktsize,1,fp);
                    fflush(fp);
                    
                    printf("sequence number %u\n",recvpacket.seqno);
                    
                    //pthread_mutex_lock(&mutexlist);
                    retrans[recvpacket.seqno]=1;
                    //pthread_mutex_unlock(&mutexlist);
                    
                    if(recvpacket.seqno > max)
                        max = recvpacket.seqno;
                    
                    
                    ttlpkt++;
                    
                }
                
            }
            
           /* else if(recvpacket.my_protocol == 99)
            {
                
                retrthr(recvpacket->seqno, recvpacket->dest_name);
            }*/
            printf("Total packets received %ld\n\n",ttlpkt);
            
            /*if(ttlpkt==no_pkts)
            {
                printf("\ntotal packets received equalled num packets   1***\n");
                break;

            }*/

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

	// close(sockfd);
	//pthread_exit(NULL);
    
	//printf("recvd size %d\n", length);

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


        n = recvfrom(sockfd, &nak, sizeof(nak), 0, NULL, NULL);
                        printf("an error: %s\n", strerror(errno));
   
        sleep(0.5);
        if(n == 1500)
        {   
            printf("number %d\n", n);
            printf("received protocol number %d\n", nak.my_protocol);

            break;
        }
        if(nak.my_protocol == 99 && nak.seqno == 111 && nak.dest_node == 2)
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
                        pack.src_name = 2;
                        pack.dest_name= 1;
                
                        printf("retransmission thread is sending %u\n", pack.seqno);
                        sendto(sockfd2,&pack,1500,0,(struct sockaddr*)&sol,sizeof(sol));
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
                        pack.src_name = 2;
                        pack.dest_name= 1;
                
                        printf("retransmission thread is sending %u\n", pack.seqno);
                        sendto(sockfd2,&pack,1500,0,(struct sockaddr*)&sol,sizeof(sol));
                        //usleep(50);
                }
            
            
            
        }
    }//end of while
    
   // pthread_exit(NULL);
}//end of retransmission thread








int main(int argc, char *argv[])
{

	
	//packet *recvpacket = (packet *)malloc(sizeof(packet)); 
	//packet *tosend = (packet *)malloc(sizeof(packet));
	packet recvpacket,tosend;
	int length;
	int sentsize;
	int chunk = PAYLOAD_SIZE, share;
	unsigned char *mainbuffer;
	long int counter =0;
	int fd,status;
	struct stat statbuf; 
	uint16_t seq=0;
    int last_pktsize; 
    

    if(argc <= 1){
        printf("Please enter the interface you want to communicate..exiting\n");
        exit(1);
    }
    
    strncpy(interface , argv[1] , 5);


    sockfd = create_rawsocket(ETH_P_ALL);
	if(sockfd < 0)
	{
		perror("socket() error");
		// If something wrong just exit
		exit(-1);
	}
	else
		printf("socket() - Using SOCK_RAW socket and UDP protocol is OK.\n");

    set_promisc( interface , sockfd );

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
	
  	no_pkts = (long int)FILESIZE/PAYLOAD_SIZE;
	if(FILESIZE%(PAYLOAD_SIZE)!=0)
			no_pkts++;
	
	strncpy(ifr.ifr_name,interface,IFNAMSIZ);
	if(ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0)
	{
		 perror("SIOCGIFINDEX");
		 exit(1);
	}		
    
    if(pthread_create(&threadid_1,NULL,recvthr,NULL)!=0)
    {
        //printf("creation of thread %d not successful\n",t);
        pthread_exit(NULL);
    }
    
    if(pthread_create(&threadid_2,NULL,nakthr,NULL)!=0)
    {
        //printf("creation of thread %d not successful\n",t);
        pthread_exit(NULL);
    }
    
    pthread_join(threadid_1,NULL);
    fprintf(stdout , "\nExited the Receiver thread\n");
    pthread_join(threadid_2,NULL);
    fprintf(stdout , "\nExited the Nak thread\n");
    

	fd = open("test.txt",O_RDONLY);
    
    if(fd == -1){
        printf("error : no file..exiting\n");
        exit(1);
    }
  
    status = fstat(fd,&statbuf);

    if(status==-1){
            printf("File not found\n");
        }
        
    filesize = statbuf.st_size;
    
            
    mainbuffer = (char *)mmap(0,filesize,PROT_READ,MAP_SHARED,fd,0);
    txbuf = mainbuffer;
    printf("Filesize is %ld and number of packets is %ld\n\n", filesize, no_pkts);
    share = filesize;
    usleep(10);
    while(counter < no_pkts)
    {   
        
        ifindex = ifr.ifr_ifindex;

        sol.sll_family = PF_PACKET;
        sol.sll_protocol=htons(0x0800);
        sol.sll_ifindex = ifindex;
        sol.sll_hatype = 1;
        sol.sll_pkttype = PACKET_OTHERHOST;

        int i;
        for (i = 0; i<6; i++) {
            //sol.sll_addr[i] = tosend->dest_mac[i];
            sol.sll_addr[i] = 0x00;
        }
        /* this is 8 byte ,why they make it 8 ?*/
        sol.sll_addr[6] = 0x00;
        sol.sll_addr[7] = 0x00;
    
        sol.sll_halen = 6;
        memset(&tosend,0,sizeof(tosend));
        //printf("mac address dest %s\n", dst_mac);
        tosend.my_protocol = 19;
        tosend.seqno = seq;
        tosend.dest_name = 1;
        memcpy(tosend.payload , &(mainbuffer[ptr]),chunk);
        //printf("mainbuffer is %d\n", mainbuffer[ptr]);
        //memcpy(buffer,(unsigned char *)tosend,75);

        //printf("buffer is \n%s\n", tosend.payload);
        usleep(20);
        sentsize = sendto(sockfd2, &tosend, 1500, 0, (struct sockaddr*)&sol, sizeof(sol));
        printf("Sent number of bytes %d with sequence number %d\n\n",sentsize, tosend.seqno);
        
        seq++;
        
        share=share-chunk;
            if(share<=0)
            {
                            
                break;
            
            }
                
            if(share>=PAYLOAD_SIZE)

                chunk=PAYLOAD_SIZE;
            else
                chunk=share;  

            ptr=ptr+PAYLOAD_SIZE;
      
        printf("Remaining number of bytes %d\n\n",chunk);
      
        //sleep(1);
        counter++;
    }
    
		
    //printf("*******Came back to main*********\n");
    if(pthread_create(&threadid_3,NULL,retrthr,NULL)!=0)
    {
        //printf("creation of thread %d not successful\n",t);
        pthread_exit(NULL);
    }

    
    pthread_join(threadid_3,NULL);
    fprintf(stdout , "\nExited the Nak thread\n");
    
	close(fd);
    close(sockfd);
	close(sockfd2);
	return 0;

}



