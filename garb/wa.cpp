
// int helly(int okay, int ohno = 9) {
    
//     return okay + ohno;
// }

// #define M(...) helly(2 __VA_OPT__(,) __VA_ARGS__);

#include <typeinfo>
#include <stdio.h>

struct Apple {
    int size;
    int sour;
};
struct Pear {
    int size;
    int sour;
};

int main() {
    
    const std::type_info& info = typeid(Apple*);
    
    printf("name: %s\n",typeid(const Pear*).name());
    
    // M()
    // M(5)
    
    // printf(__cplusplus)
    
    return 9;
}