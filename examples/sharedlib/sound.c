// gcc sound.c -fpic -shared -o libsound.so

#include <stdio.h>

void PlaySoundFromFile(const char* path){
    printf("Play sound: %s\n", path);
}