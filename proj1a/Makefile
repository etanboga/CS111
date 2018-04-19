#Name: Ege Tanboga
#email: ege72282@gmail.com
#ID: 304735411

.SILENT:

tarred_files = Makefile lab1a.c README

default: 
	gcc -g -Wall -Wextra lab1a.c -o lab1a
clean:
	rm -f *.o *.tar.gz lab1a
dist: default
	tar -czf lab1a-304735411.tar.gz $(tarred_files)

