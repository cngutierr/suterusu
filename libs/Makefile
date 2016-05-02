all: test_no_math test_rand_test

test_no_math: no_math.o test_no_math.c
	gcc no_math.o test_no_math.c -o test_no_math -lm

test_rand_test: no_math.o rand_test.o test_rand_test.c
	gcc no_math.o rand_test.o test_rand_test.c -o test_rand_test -mhard-float

rand_test.o: rand_test.c
	gcc -c rand_test.c -mhard-float

no_math.o: no_math.c
	gcc -c no_math.c -mhard-float
