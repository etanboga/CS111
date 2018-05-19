##NAME: Ege Tanboga
##EMAIL: ege72282@gmail.com
##ID: 304735411

.SILENT:

default: lab4b

check: lab4b
	./egetest.sh
lab4b:
	 gcc -g -Wall -Wextra lab4b.c -lmraa -lm -o lab4b
clean: 
	rm -f lab4b lab4b-304735411.tar.gz *.0 *.txt

dist:
	tar -czvf lab4b-304735411.tar.gz Makefile lab4b.c README egetest.sh
