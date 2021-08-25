# ToyObfuscator

Some simple obfuscator ;)

- `-fla_plus`: control flow graph flatten plus version

## Compile

```bash
git clone https://github.com/veritas501/ToyObfuscator.git
cd ToyObfuscator
mkdir build && cd build
cmake .. -DLLVM_DIR=/usr/lib/llvm-10/lib/cmake/llvm/
make

# lib at "./src/libToyObfuscator.so"
```



## Usage

- demo.c

```c
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void foo1() {
    puts("argc == 1");
}

void foo2(int argc) {
    for(int i=0;i<argc;i++){
        puts("argc != 1");    
    }
}

int main(int argc, char **argv) {
    if (argc == 1) {
        foo1();
    } else {
        foo2(argc);
    }
    return argc;
}
```



```bash
$ clang -emit-llvm -c demo.c -o demo.bc
$ opt -load ./libMyObfuscator.so -lowerswitch -fla_plus demo.bc -o demo_obf.bc
# 👆 DON'T forget -lowerswitch
$ clang demo_obf.bc -o demo_obf
```



