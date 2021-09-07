# ToyObfuscator

Some simple obfuscator ;)

- `-fla_plus`: control flow graph flatten plus version

## Compile

```bash
git clone https://github.com/veritas501/ToyObfuscator.git
cd ToyObfuscator
mkdir build && cd build
cmake .. -DLLVM_DIR=/usr/lib/llvm-10/lib/cmake/llvm/
make -j`nproc`

# lib at "./src/libToyObfuscator.so"
```

build custom clang with obfuscator:

```bash
# clone llvm-10.0.1
git clone https://github.com/llvm/llvm-project.git --depth 1 -b llvmorg-10.0.1
# apply custom patch
./build_clang.sh <DIR_TO_llvm-project>

# normal build clang and llvm
cd <DIR_TO_llvm-project>
mkdir build && cd build
cmake -DLLVM_ENABLE_PROJECTS=clang -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" ../llvm
make -j`nproc` # or 'make clang -j`nproc`' for just compile clang
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
    for (int i = 0; i < argc; i++) {
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
$ opt -load ./libToyObfuscator.so -fla_plus demo.bc -o demo_obf.bc
$ clang demo_obf.bc -o demo_obf
```

- demo_obf.ll

```bash
$ llvm-dis demo_obf.bc
```

function `foo2`:

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
  store i32 298058585, i32* %label
  store i32 -803332310, i32* %x
  store i32 -1204918219, i32* %y
  br label %Dispatcher

Trans_0:                                          ; preds = %Dispatcher
  %6 = load i32, i32* %x
  store i32 %6, i32* %4
  %7 = load i32, i32* %y
  store i32 %7, i32* %5
  %8 = load i32, i32* %x
  store i32 %8, i32* %y
  store i32 460730405, i32* %x
  %9 = load i32, i32* %x
  %10 = load i32, i32* %y
  %11 = add i32 %10, %9
  %12 = shl i32 %10, 8
  %13 = lshr i32 %10, 24
  %14 = or i32 %13, %12
  %15 = xor i32 %14, %11
  store i32 %11, i32* %x
  store i32 %15, i32* %y
  store i32 1815737693, i32* %label
  br label %Dispatcher

Trans_3:                                          ; preds = %Dispatcher
  %16 = load i32, i32* %x
  %17 = load i32, i32* %y
  %18 = add i32 %17, %16
  %19 = shl i32 %17, 22
  %20 = lshr i32 %17, 10
  %21 = or i32 %20, %19
  %22 = xor i32 %21, %18
  store i32 %18, i32* %x
  store i32 %22, i32* %y
  %23 = load i32, i32* %4
  store i32 %23, i32* %x
  %24 = load i32, i32* %5
  store i32 %24, i32* %y
  store i32 %22, i32* %label
  br label %Dispatcher

Trans_1:                                          ; preds = %Dispatcher
  %25 = load i32, i32* %x
  %26 = load i32, i32* %y
  %27 = add i32 %26, %25
  %28 = shl i32 %26, 28
  %29 = lshr i32 %26, 4
  %30 = or i32 %29, %28
  %31 = xor i32 %30, %27
  store i32 %27, i32* %x
  store i32 %31, i32* %y
  store i32 1197840973, i32* %label
  br label %Dispatcher

32:                                               ; preds = %Dispatcher
  %33 = call i32 @puts(i8* getelementptr inbounds ([10 x i8], [10 x i8]* @.str.1, i64 0, i64 0))
  %34 = load i32, i32* %y
  %35 = xor i32 %34, -824075049
  store i32 %35, i32* %x
  %36 = load i32, i32* %x
  %37 = xor i32 %36, 198614652
  store i32 %37, i32* %y
  store i32 298058585, i32* %label
  br label %Dispatcher

38:                                               ; preds = %Dispatcher
  %39 = load i32, i32* %3, align 4
  %40 = load i32, i32* %2, align 4
  %41 = icmp slt i32 %39, %40
  %42 = load i32, i32* %y
  %43 = xor i32 %42, -799371851
  %44 = xor i32 %42, -1637481724
  %45 = select i1 %41, i32 %43, i32 %44
  store i32 %45, i32* %x
  %46 = load i32, i32* %x
  %47 = xor i32 %46, 1651061640
  %48 = xor i32 %46, 1262310110
  %49 = select i1 %41, i32 %47, i32 %48
  store i32 %49, i32* %y
  store i32 298058585, i32* %label
  br label %Dispatcher

50:                                               ; preds = %Dispatcher
  %51 = load i32, i32* %3, align 4
  %52 = add nsw i32 %51, 1
  store i32 %52, i32* %3, align 4
  %53 = load i32, i32* %y
  %54 = xor i32 %53, -203712883
  store i32 %54, i32* %x
  %55 = load i32, i32* %x
  %56 = xor i32 %55, -2065749477
  store i32 %56, i32* %y
  store i32 298058585, i32* %label
  br label %Dispatcher

57:                                               ; preds = %Dispatcher
  ret void

Dispatcher:                                       ; preds = %Trans_3, %Trans_2, %Trans_1, %Trans_0, %50, %32, %38, %59, %DefaultBranch, %1
  %58 = load i32, i32* %label
  switch i32 %58, label %DefaultBranch [
    i32 -1925297453, label %59
    i32 2014756364, label %38
    i32 947951719, label %32
    i32 830688616, label %50
    i32 -1172152496, label %57
    i32 298058585, label %Trans_0
    i32 1815737693, label %Trans_1
    i32 1197840973, label %Trans_2
    i32 773620844, label %Trans_3
  ]

DefaultBranch:                                    ; preds = %Dispatcher
  br label %Dispatcher

59:                                               ; preds = %Dispatcher
  store i32 0, i32* %3, align 4
  %60 = load i32, i32* %y
  %61 = xor i32 %60, 1175808718
  store i32 %61, i32* %x
  %62 = load i32, i32* %x
  %63 = xor i32 %62, -2065749477
  store i32 %63, i32* %y
  store i32 298058585, i32* %label
  br label %Dispatcher

Trans_2:                                          ; preds = %Dispatcher
  %64 = load i32, i32* %x
  %65 = load i32, i32* %y
  %66 = add i32 %65, %64
  %67 = shl i32 %65, 19
  %68 = lshr i32 %65, 13
  %69 = or i32 %68, %67
  %70 = xor i32 %69, %66
  store i32 %66, i32* %x
  store i32 %70, i32* %y
  store i32 773620844, i32* %label
  br label %Dispatcher
}
```
