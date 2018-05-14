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
	rm -rf *.o lab2_list *.csv *.png lab2b-304735411.tar.gz
dist: graphs
	tar -zcvf lab2b-304735411.tar.gz *.h *.c README Makefile *.png *.csv *.gp profile.out
tests: clean build
	for t in 1 2 4 8 12 16 24; do \
		./lab2_list --iterations=1000 --threads=$$t --sync=m >> lab2b_list.csv; \
		./lab2_list --iterations=1000 --threads=$$t --sync=s >> lab2b_list.csv ; \
	done

	for t in 1 4 8 12 16 ; do \
		for i in 1 2 4 8 16 ; do \
			./lab2_list --yield=id --lists=4 --threads=$$t --iterations=$$i >> lab2b_list.csv || true ; \
		done ; \
	done

	for t in 1 4 8 12 16 ; do \
		for i in 10 20 40 80 ; do \
			./lab2_list --yield=id --lists=4 --threads=$$t --iterations=$$i --sync=m >> lab2b_list.csv ; \
			./lab2_list --yield=id --lists=4 --threads=$$t --iterations=$$i --sync=s >> lab2b_list.csv ; \
		done ; \
	done
	for t in 1 2 4 8 12 ; do \
		for l in 4 8 16 ; do \
			./lab2_list --sync=m --iterations=1000 --threads=$$t --lists=$$l >> lab2b_list.csv ; \
			./lab2_list --sync=s --iterations=1000 --threads=$$t --lists=$$l >> lab2b_list.csv ; \
		done ; \
	done

profile: clean build
	rm -f ./raw.gperf
	LD_PRELOAD=/usr/lib64/libprofiler.so.0 CPUPROFILE=./raw.gperf ./lab2_list --threads=12 --iterations=1000 --sync=s
	pprof --text ./lab2_list ./raw.gperf > profile.out
	pprof --list=lock_spin_function ./lab2_list ./raw.gperf >> profile.out 
	rm -f ./raw.gperf
