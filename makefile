all: smsh2

part1:
	gcc -o smsh2 smsh2.c splitline.c execute.c
