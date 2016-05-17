yun_manager: manager.o log.o network.o
	gcc -o yun_manager manager.o log.o network.o
manager.o: manager.c
	gcc -c manager.c 
log.o: log/log.c log/log.h
	gcc -c log/log.c
network.o: network.c network.h
	gcc -c network.c

clean: 
	rm yun_manager *.o

