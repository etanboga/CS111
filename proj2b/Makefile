#NAME: Ege Tanboga
#EMAIL: ege72282@gmail.com
#ID: 304735411

.SILENT:

default: build

build:
	gcc -pthread -g -Wall -Wextra SortedList.c lab2_list.c -o lab2_list
graphs:	tests
	gnuplot lab2_list.gp
clean:
	rm -rf *.png *.csv *.o lab2_list
dist: graphs
	tar -zcvf lab2a-304735411.tar.gz *.h *.c README Makefile *.png *.csv *.gp 

tests: clean build