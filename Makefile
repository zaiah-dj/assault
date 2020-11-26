#Quick test for hypno and basically any other web server that needs to serve many reuests.

NAME=assault
OS = $(sassault uname | sed 's/[_ ].*//')
LDFLAGS = -lcurl -lpthread -lsqlite3
CLANGFLAGS = -g -O0 -Wall -Werror -std=c99 -Wno-unused -Wno-format-security -fsanitize=address -fsanitize-undefined-trap-on-error -DDEBUG_H
CFLAGS = $(CLANGFLAGS)
CC = clang
PREFIX = /usr/local

test:
	$(CC) $(LDFLAGS) $(CFLAGS) main.c -o $(NAME) $(OBJ) 
