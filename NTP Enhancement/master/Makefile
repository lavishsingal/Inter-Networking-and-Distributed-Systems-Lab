ntp: master.o ntpapis.o
	gcc -o myntp -g master.o ntpapis.o -lpthread
master.o: master.c
	gcc -g -c master.c
ntpapis.o: ntpapis.c
	gcc -g -c ntpapis.c 
clean:
	rm -f *.o *.gch ntp
