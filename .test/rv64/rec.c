//#include <stdio.h>
//#include <stdint.h>

int rec(int n) {
	if (n <= 0)
		return 1;
	return n * rec(n - 1);
}

int main(int argc, char *argv[]) {
	int n = 5;
//	printf("rec(%d) = %d\n", n, rec(n));
	return rec(n);
}
