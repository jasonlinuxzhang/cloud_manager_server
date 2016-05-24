yun_manager: manager.o log.o network.o common.o cJSON.o operation.o host.o vm_monitor.o
	gcc -o yun_manager manager.o log.o network.o common.o cJSON.o operation.o host.o vm_monitor.o -lpthread -lm -lvirt -g
manager.o: manager.c
	gcc -c manager.c
log.o: log/log.c log/log.h
	gcc -c log/log.c 
network.o: network/network.c network/network.h
	gcc -c network/network.c
common.o: common.c common.h
	gcc -c common.c
cJSON.o: cJSON/cJSON.c cJSON/cJSON.h
	gcc -c cJSON/cJSON.c
operation.o: operation/operation.c operation/operation.h
	gcc -c operation/operation.c
host.o: host/host.c host/host.h
	gcc -c host/host.c
vm_monitor.o: vm_monitor/vm_monitor.c vm_monitor/vm_monitor.h
	gcc -c vm_monitor/vm_monitor.c

clean: 
	rm yun_manager *.o

