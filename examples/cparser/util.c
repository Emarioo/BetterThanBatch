// gcc -g -c -o util.o util.c
// ar rcs util.lib util.o

// gcc -g -shared -o util.dll util.c

int calculate(int x, int y) {
    return x * 2 + y * 3;
}