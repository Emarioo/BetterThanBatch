
/* Compile with:
g++ -c examples/foreign/tiny_math.cpp -o examples/foreign/tiny_math.o
ar rcs examples/foreign/tiny_math.lib examples/foreign/tiny_math.o

g++ -shared examples/foreign/tiny_math.cpp -o examples/foreign/tiny_math.dll
*/

int sum(int start, int end) {
    int n = 0;
    for(int i=start;i<=end;i++) {
        n += i;
    }
    return n;
}