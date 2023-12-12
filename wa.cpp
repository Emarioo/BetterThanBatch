/*
    File to test UNIX functions
*/

// #include <semaphore.h>
// #include <pthread.h>
// #include <stdio.h>
// #include <dirent.h>
// #include <sys/stat.h>
// #include <vector>
// #include <string>
// #include <string.h>
#include <unistd.h>

// struct Apple {
//     ~Apple() {
//         printf("Apple dest\n");
//     }

//     std::string oi;
// };
// struct Basket {
//     ~Basket() {
//     }
//     Apple apple;
// };

int main() {

    char k = 'A';
    write(STDOUT_FILENO, &k,2);

    // Basket basket;

    // basket.apple.oi = "Haha";

    // basket.apple.oi.~basic_string();
    // basket.~Basket();
    // printf("%s\n",basket.apple.oi.c_str());

    // char* ar[2]{"gcc",nullptr};
    // int err = execv(ar[0],ar);
    // printf("%d, %s\n",err, strerror(errno));
    // int fd = STDOUT_FILENO;
    // const char* str = "\033[1;32mEyo!\n";
    // write(fd,str,strlen(str));
    // printf("%s",str);

    // int res = __atomic_fetch_add(&yoo,1,__ATOMIC_SEQ_CST);
    // printf("ha %d %d\n",res, yoo);

    // printf("%d\n",sizeof(pthread_t));
    // dirent* entry = nullptr;
    // DIR* dir = nullptr;
    // struct stat buf{};
    // std::vector<std::string> dirs;
    // dirs.push_back("src");

    // char filepath[256];

    // while(dirs.size()>0){
    //     std::string root = dirs[0];
    //     dirs.erase(dirs.begin());
    //     dir = opendir(root.c_str());
    //     while((entry = readdir(dir))) {
    //         if(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
    //             continue;
            
    //         int len = snprintf(filepath, sizeof(filepath), "%s/%s", root.c_str(), entry->d_name);

    //         int err = stat(filepath, &buf);
    //         if(err == -1) {
    //             // printf("err: %d\n",errno);
    //         } else {
    //             if(buf.st_mode & S_IFDIR) {
    //                 dirs.push_back(filepath);
    //             } else {
    //                 printf("%s, %d\n",entry->d_name, buf.st_mode);
    //             }
    //         }
    //     }
    //     closedir(dir);
    // }
}