#NAME: Ege Tanboga
#EMAIL: ege72282@gmail.com
#ID: 304735411

.SILENT:

default: lab4c_tcp lab4c_tls

lab4c_tcp:
	 gcc -g -Wall -Wextra lab4c_tcp.c -lmraa -lm -o lab4c_tcp
lab4c_tls:
	gcc -g -Wall -Wextra lab4c_tls.c -lmraa -lm -o lab4c_tls
clean:
	rm -f lab4c_tcp lab4c_tls lab4c-304735411.tar.gz
dist:
	tar -czvf lab4c-304735411.tar.gz Makefile lab4c_tcp.c lab4c_tls.c README
