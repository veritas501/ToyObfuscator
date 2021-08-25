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



- demo_obf.ll

```bash
$ llvm-dis demo_obf.bc
```

just function `foo2`:

```
; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @foo2(i32 %0) #0 {
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store i32 %0, i32* %2, align 4
  %4 = alloca i32
  %5 = alloca i32
  %6 = alloca i32
  %7 = alloca i32
  %8 = alloca i32
  %9 = alloca i32
  %10 = alloca i32
  store i32 -880217864, i32* %4
  store i32 -990342826, i32* %5
  store i32 535417669, i32* %6
  br label %Dispatcher

Dispatcher:                                       ; preds = %LoopBack, %1
  %11 = load i32, i32* %4
  switch i32 %11, label %DefaultBranch [
    i32 -1467212085, label %12
    i32 -916412709, label %17
    i32 -1358730345, label %29
    i32 -438744154, label %35
    i32 -421709854, label %42
    i32 -880217864, label %Trans_0
    i32 2034435264, label %Trans_1
    i32 1285005694, label %Trans_2
    i32 -1068251817, label %Trans_3
  ]

12:                                               ; preds = %Dispatcher
  store i32 0, i32* %3, align 4
  %13 = load i32, i32* %6
  %14 = xor i32 %13, -710862801
  store i32 %14, i32* %5
  %15 = load i32, i32* %5
  %16 = xor i32 %15, -1271979713
  store i32 %16, i32* %6
  store i32 -880217864, i32* %4
  br label %LoopBack

17:                                               ; preds = %Dispatcher
  %18 = load i32, i32* %3, align 4
  %19 = load i32, i32* %2, align 4
  %20 = icmp slt i32 %18, %19
  %21 = load i32, i32* %6
  %22 = xor i32 %21, -1917852521
  %23 = xor i32 %21, -520642266
  %24 = select i1 %20, i32 %22, i32 %23
  store i32 %24, i32* %5
  %25 = load i32, i32* %5
  %26 = xor i32 %25, 1552887217
  %27 = xor i32 %25, -1286396546
  %28 = select i1 %20, i32 %26, i32 %27
  store i32 %28, i32* %6
  store i32 -880217864, i32* %4
  br label %LoopBack

29:                                               ; preds = %Dispatcher
  %30 = call i32 @puts(i8* getelementptr inbounds ([10 x i8], [10 x i8]* @.str.1, i64 0, i64 0))
  %31 = load i32, i32* %6
  %32 = xor i32 %31, -313117627
  store i32 %32, i32* %5
  %33 = load i32, i32* %5
  %34 = xor i32 %33, -290390692
  store i32 %34, i32* %6
  store i32 -880217864, i32* %4
  br label %LoopBack

35:                                               ; preds = %Dispatcher
  %36 = load i32, i32* %3, align 4
  %37 = add nsw i32 %36, 1
  store i32 %37, i32* %3, align 4
  %38 = load i32, i32* %6
  %39 = xor i32 %38, 1726554368
  store i32 %39, i32* %5
  %40 = load i32, i32* %5
  %41 = xor i32 %40, -1271979713
  store i32 %41, i32* %6
  store i32 -880217864, i32* %4
  br label %LoopBack

42:                                               ; preds = %Dispatcher
  ret void

Trans_0:                                          ; preds = %Dispatcher
  %43 = load i32, i32* %5
  %44 = add i32 %43, -725173849
  %45 = shl i32 %43, 24
  %46 = lshr i32 %43, 8
  %47 = or i32 %46, %45
  %48 = xor i32 %47, %44
  store i32 %44, i32* %7
  store i32 %48, i32* %8
  store i32 2034435264, i32* %4
  br label %LoopBack

Trans_1:                                          ; preds = %Dispatcher
  %49 = load i32, i32* %7
  %50 = load i32, i32* %8
  %51 = add i32 %50, %49
  %52 = lshr i32 %50, 11
  %53 = shl i32 %50, 21
  %54 = or i32 %53, %52
  %55 = xor i32 %54, %51
  store i32 %51, i32* %9
  store i32 %55, i32* %10
  store i32 1285005694, i32* %4
  br label %LoopBack

Trans_2:                                          ; preds = %Dispatcher
  %56 = load i32, i32* %9
  %57 = load i32, i32* %10
  %58 = add i32 %57, %56
  %59 = shl i32 %57, 28
  %60 = lshr i32 %57, 4
  %61 = or i32 %60, %59
  %62 = xor i32 %61, %58
  store i32 %58, i32* %7
  store i32 %62, i32* %8
  store i32 -1068251817, i32* %4
  br label %LoopBack

Trans_3:                                          ; preds = %Dispatcher
  %63 = load i32, i32* %7
  %64 = load i32, i32* %8
  %65 = add i32 %64, %63
  %66 = lshr i32 %64, 6
  %67 = shl i32 %64, 26
  %68 = or i32 %67, %66
  %69 = xor i32 %68, %65
  store i32 %65, i32* %9
  store i32 %69, i32* %4
  br label %LoopBack

LoopBack:                                         ; preds = %Trans_3, %Trans_2, %Trans_1, %Trans_0, %35, %29, %17, %12
  br label %Dispatcher

DefaultBranch:                                    ; preds = %Dispatcher, %DefaultBranch
  br label %DefaultBranch
}
```



