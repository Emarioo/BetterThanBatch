
/* Compile with:
g++ -c examples/foreign/tiny_math.cpp -o examples/foreign/tiny_math.o
ar rcs examples/foreign/tiny_math.lib examples/foreign/tiny_math.o

g++ -shared examples/foreign/tiny_math.cpp -o examples/foreign/tiny_math.dll

link with: g++ examples/foreign/main.cpp -Lexamples/foreign -ltiny_math.dll
I believe tiny_math needs to be in the same directory as the final excutable or
you will get STATUS_DLL_NOT_FOUND (0xC0000135 or -1073741515)
*/

int sum(int start, int end) {
    int n = 0;
    for(int i=start;i<=end;i++) {
        n += i;
    }
    return n;
    // return start + end + 10;
}