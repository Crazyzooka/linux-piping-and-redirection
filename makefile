all: part1 part2

part1:
	gcc -o smsh2 smsh2.c splitline.c execute.c -Wall -std=c99
part2:
	gcc -o smsh3 smsh3.c splitline.c execute.c -Wall -std=c99
