#NAME: Ege Tanboga
#EMAIL: ege72282@gmail.com
#UID: 304735411

.SILENT:

tarred_files= Makefile README lab1b-client.c lab1b-server.c lab1b-304735411.tar.gz

default: lab1b-server lab1b-client

lab1b-server:
	gcc -g -Wall -Wextra lab1b-server.c -o lab1b-server
lab1b-client:
	gcc -g -Wall -Wextra lab1b-client.c -o lab1b-client
clean:
	rm -rf *.tar.gz *.o lab1b-server lab1b-client *.txt
dist:
	tar -zcvf lab1b-304735411.tar.gz lab1b-client.c lab1b-server.c Makefile README