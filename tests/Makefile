all: test

test: no_math.o test_no_math.c
	gcc no_math.o test_no_math.c -o test_no_math -lm

no_math.o: no_math.c
	gcc -c no_math.c
