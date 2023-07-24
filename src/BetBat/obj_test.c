// #include <stdio.h>
// #include <Windows.h>
// #include <stdlib.h>

int _declspec(dllimport) __stdcall printme();

int main(){
//     // WriteFile(GetStdHandle(-11),"Hello\n",6,NULL,NULL);

//     // HANDLE ptr = GetStdHandle(-11);

//     // printf();
//     // printf("%llu\n",(unsigned long long)stdout);d
//     // fwrite("Woaw\n",2,5,(FILE*)1);
//     // int a = 5;
//     // int b = 3;
//     // const char* a = "he";
//     // *(void*)0;
//     // int a = 1;
//     // int c = a + b - ok + sec;
//     // unsigned long long* p = &global;
//     // return (int)*p;
//     return GetLastError();
    printme();
    return 991;
}