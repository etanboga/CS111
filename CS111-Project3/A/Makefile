# Akshara Sundararajan,Ege Tanboga
# 404731846, 304735411
# amedrops@g.ucla.edu, ege72282@gmail.com

CC = gcc
DEBUG = -g
CFLAGS = -Wall -Wextra

default:
	$(CC) $(DEBUG) $(CFLAGS) lab3a.c -o lab3a
clean:
	rm -f lab3a lab3a-404731846.tar.gz

dist:	clean default
	tar -zvcf lab3a-404731846.tar.gz Makefile README lab3a.c ext2_fs.h
