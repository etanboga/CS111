#Name: Ege Tanboga
#email: ege72282@gmail.com
#ID: 304735411

.SILENT:

tarred_files = Makefile lab0.c backtrace.png breakpoint.png README

default:
	gcc -g -Wall -Wextra lab0.c -o lab0
clean:
	rm -f *.o *.txt *.o *.tar.gz
dist: default
	tar -czf lab0-304735411.tar.gz $(tarred_files)

check: default testInvalidArg segFaultTest inputErrorTest outputErrorTest successfulCopyTest

testInvalidArg:
	echo | ./lab0 --invalidarg &> /dev/null; \
	if [[ $$? -ne 1 ]]; then \
	echo "test for invalid argument failed"; \
	else \
	echo "passed test for invalid argument"; \
	fi

segFaultTest:
	echo | ./lab0 --catch --segfault &> /dev/null; \
	if [[ $$? -ne 4 ]]; then \
	echo "test for segfault failed"; \
	else \
	echo "passed test for segfault"; \
	fi

inputErrorTest:
	./lab0 --input nofile.txt &> /dev/null; \
	if [[ $$? -ne 2 ]]; then \
	echo "test failed for no input file"; \
	else \
	echo "test passed for no input file"; \
	fi

outputErrorTest:
	echo "input file" > input.txt; \
	touch output.txt; \
	chmod u-w output.txt; \
	./lab0 --input input.txt --output output.txt &> /dev/null; \
	if [[ $$? -ne 3 ]]; then \
	echo "test failed for unable to open/write to output file"; \
	else \
	echo "test passed for unable to open/write to output file"; \
	fi
	chmod u+w output.txt
	rm input.txt output.txt

successfulCopyTest:
	echo "input file" > input.txt; \
	./lab0 --input input.txt --output output.txt &> /dev/null; \
	if [[ $$? -ne 0 ]]; then \
	echo "test failed for proper exit code when copied"; \
	else \
	echo "test passed for proper exit code when copied"; \
	fi; \
	cmp input.txt output.txt; \
        if [[ $$? -ne 0 ]]; then \
        echo "test failed for input file matches output file"; \
        else \
        echo "test passed for input matches output"; \
        fi
	rm input.txt output.txt
