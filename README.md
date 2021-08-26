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
$ opt -load ./libToyObfuscator.so -lowerswitch -fla_plus demo.bc -o demo_obf.bc
# 👆 DON'T forget -lowerswitch
$ clang demo_obf.bc -o demo_obf
```



- demo_obf.ll

```bash
$ llvm-dis demo_obf.bc
```

just function `foo2`:

```llvm
; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @foo2(i32 %0) #0 {
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store i32 %0, i32* %2, align 4
  %label = alloca i32
  %x = alloca i32
  %y = alloca i32
  %4 = alloca i32
  %5 = alloca i32
  store i32 1446627025, i32* %label
  store i32 2087920239, i32* %x
  store i32 1888510660, i32* %y
  br label %Dispatcher

6:                                                ; preds = %Dispatcher
  store i32 0, i32* %3, align 4
  %7 = load i32, i32* %y
  %8 = xor i32 %7, 1065883156
  store i32 %8, i32* %x
  %9 = load i32, i32* %x
  %10 = xor i32 %9, -961949487
  store i32 %10, i32* %y
  store i32 1446627025, i32* %label
  br label %LoopBack

11:                                               ; preds = %Dispatcher
  %12 = load i32, i32* %3, align 4
  %13 = add nsw i32 %12, 1
  store i32 %13, i32* %3, align 4
  %14 = load i32, i32* %y
  %15 = xor i32 %14, -1783730366
  store i32 %15, i32* %x
  %16 = load i32, i32* %x
  %17 = xor i32 %16, -961949487
  store i32 %17, i32* %y
  store i32 1446627025, i32* %label
  br label %LoopBack

18:                                               ; preds = %Dispatcher
  %19 = load i32, i32* %3, align 4
  %20 = load i32, i32* %2, align 4
  %21 = icmp slt i32 %19, %20
  %22 = load i32, i32* %y
  %23 = xor i32 %22, -868768504
  %24 = xor i32 %22, 167612878
  %25 = select i1 %21, i32 %23, i32 %24
  store i32 %25, i32* %x
  %26 = load i32, i32* %x
  %27 = xor i32 %26, 1762341169
  %28 = xor i32 %26, -380429728
  %29 = select i1 %21, i32 %27, i32 %28
  store i32 %29, i32* %y
  store i32 1446627025, i32* %label
  br label %LoopBack

Trans_1:                                          ; preds = %Dispatcher
  %30 = load i32, i32* %x
  %31 = load i32, i32* %y
  %32 = add i32 %31, %30
  %33 = shl i32 %31, 23
  %34 = lshr i32 %31, 9
  %35 = or i32 %34, %33
  %36 = xor i32 %35, %32
  store i32 %32, i32* %x
  store i32 %36, i32* %y
  store i32 -1620596700, i32* %label
  br label %LoopBack

37:                                               ; preds = %Dispatcher
  ret void

Dispatcher:                                       ; preds = %LoopBack, %1
  %38 = load i32, i32* %label
  switch i32 %38, label %DefaultBranch [
    i32 -1031163472, label %6
    i32 -1965205282, label %18
    i32 -1686928439, label %39
    i32 2113491536, label %11
    i32 647307479, label %37
    i32 1446627025, label %Trans_0
    i32 387048046, label %Trans_1
    i32 -1620596700, label %Trans_2
    i32 -64152308, label %Trans_3
    i32 180335547, label %Trans_4
    i32 771300597, label %Trans_5
  ]

LoopBack:                                         ; preds = %Trans_5, %Trans_4, %Trans_3, %Trans_2, %Trans_1, %Trans_0, %11, %39, %18, %6
  br label %Dispatcher

DefaultBranch:                                    ; preds = %Dispatcher, %DefaultBranch
  br label %DefaultBranch

39:                                               ; preds = %Dispatcher
  %40 = call i32 @puts(i8* getelementptr inbounds ([10 x i8], [10 x i8]* @.str.1, i64 0, i64 0))
  %41 = load i32, i32* %y
  %42 = xor i32 %41, -680627869
  store i32 %42, i32* %x
  %43 = load i32, i32* %x
  %44 = xor i32 %43, 559241929
  store i32 %44, i32* %y
  store i32 1446627025, i32* %label
  br label %LoopBack

Trans_0:                                          ; preds = %Dispatcher
  %45 = load i32, i32* %x
  store i32 %45, i32* %4
  %46 = load i32, i32* %y
  store i32 %46, i32* %5
  %47 = load i32, i32* %x
  store i32 %47, i32* %y
  store i32 -1415305785, i32* %x
  %48 = load i32, i32* %x
  %49 = load i32, i32* %y
  %50 = add i32 %49, %48
  %51 = shl i32 %49, 8
  %52 = lshr i32 %49, 24
  %53 = or i32 %52, %51
  %54 = xor i32 %53, %50
  store i32 %50, i32* %x
  store i32 %54, i32* %y
  store i32 387048046, i32* %label
  br label %LoopBack

Trans_4:                                          ; preds = %Dispatcher
  %55 = load i32, i32* %x
  %56 = load i32, i32* %y
  %57 = add i32 %56, %55
  %58 = shl i32 %56, 8
  %59 = lshr i32 %56, 24
  %60 = or i32 %59, %58
  %61 = xor i32 %60, %57
  store i32 %57, i32* %x
  store i32 %61, i32* %y
  store i32 771300597, i32* %label
  br label %LoopBack

Trans_3:                                          ; preds = %Dispatcher
  %62 = load i32, i32* %x
  %63 = load i32, i32* %y
  %64 = add i32 %63, %62
  %65 = shl i32 %63, 16
  %66 = lshr i32 %63, 16
  %67 = or i32 %66, %65
  %68 = xor i32 %67, %64
  store i32 %64, i32* %x
  store i32 %68, i32* %y
  store i32 180335547, i32* %label
  br label %LoopBack

Trans_2:                                          ; preds = %Dispatcher
  %69 = load i32, i32* %x
  %70 = load i32, i32* %y
  %71 = add i32 %70, %69
  %72 = shl i32 %70, 20
  %73 = lshr i32 %70, 12
  %74 = or i32 %73, %72
  %75 = xor i32 %74, %71
  store i32 %71, i32* %x
  store i32 %75, i32* %y
  store i32 -64152308, i32* %label
  br label %LoopBack

Trans_5:                                          ; preds = %Dispatcher
  %76 = load i32, i32* %x
  %77 = load i32, i32* %y
  %78 = add i32 %77, %76
  %79 = shl i32 %77, 26
  %80 = lshr i32 %77, 6
  %81 = or i32 %80, %79
  %82 = xor i32 %81, %78
  store i32 %78, i32* %x
  store i32 %82, i32* %y
  %83 = load i32, i32* %4
  store i32 %83, i32* %x
  %84 = load i32, i32* %5
  store i32 %84, i32* %y
  store i32 %82, i32* %label
  br label %LoopBack
}
```



