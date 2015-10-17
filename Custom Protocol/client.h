typedef struct mypacket{
	uint8_t my_protocol;
	unsigned char src_mac[6];
	unsigned char dest_mac[6];
	struct in_addr src_ip;
	struct in_addr dest_ip;
	char payload[50];
}packet;
