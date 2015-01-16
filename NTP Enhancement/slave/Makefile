ntp: slave.o ntpapis.o
	gcc -o myntp -g slave.o ntpapis.o -lpthread
slave.o: slave.c
	gcc -g -c slave.c
ntpapis.o: ntpapis.c
	gcc -g -c ntpapis.c 
clean:
	rm -f *.o *.gch ntp
