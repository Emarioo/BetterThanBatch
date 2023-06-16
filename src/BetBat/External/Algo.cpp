// #include "BetBat/External/Algo.h"
// #include <stdio.h>

// extern "C" {
//     __declspec(dllexport) void _cdecl BubbleSort(int* arr, int len){
//         if(arr == 0 || len < 2)
//             return;
//         for(int i=0;i<len;i++){
//             bool swap=false;
//             for(int j=i;j<len-1;j++){
//                 if(arr[j] < arr[j+1]){
//                     arr[j] ^= arr[j+1];
//                     arr[j+1] ^= arr[j];
//                     arr[j] ^= arr[j+1];
//                     swap = true;
//                 }
//             }
//             if(!swap) return;
//         }
//     }
// }
// int main(int argc, char** argv){
//     BubbleSort(0,0);
//     printf("e");
//     return 0;
// }