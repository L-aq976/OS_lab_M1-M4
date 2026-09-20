#include "asm.h"
#include <string.h>

typedef long asm_jmp_buf[8];

//告诉编译器某些函数有特殊行为，不要过度优化
__attribute__((returns_twice)) int  asm_setjmp(asm_jmp_buf env);
__attribute__((noreturn))      void asm_longjmp(asm_jmp_buf env, int val);


int64_t asm_add(int64_t a, int64_t b) {
  // a=a+b;
  asm("addq %1, %0" : "+r"(a) : "r"(b) : "cc");
  return a;
}

int asm_popcnt(uint64_t x) {
  int s = 0;
  for (int i = 0; i < 64; i++) {
    // if ((x >> i) & 1) s++;
     asm("btq %2, %1\n\t"
      "adcq $0, %0"
      : "+r"(s)            // %0: 累加器，读写
      : "r"(i), "r"(x)     // %1: 被测试值, %2: 位号
      : "cc");
      // btq %2, %1 —— Bit Test（quadword，64 位） bt 位号,
      // 数值：把「数值的第 位号 位」取出来放进 CF（进位标志），不改其他任何寄存器。
      // adcq $0, %0 —— Add with Carry adc 0, 
      // s 的语义是 s = s + 0 + CF。加个 0 只是为了把 CF 加进来，效果就是 if (bit) s++;。
  }
  return s;
}

void *asm_memcpy(void *dest, const void *src, size_t n) {
  // return memcpy(dest, src, n);
  void *ret = dest;
  size_t tmp;                       // 中转用的临时寄存器
  asm volatile(
      "testq %2, %2\n\t"            // n == 0 直接跳过（下面循环是 do-while）
      "jz 2f\n"
      "1:\n\t"
      "movb (%1), %b3\n\t"          // 取一字节内存 -> 临时寄存器低 8 位
      "movb %b3, (%0)\n\t"          // 临时寄存器 -> 目标内存
      "incq %1\n\t"                 // src++
      "incq %0\n\t"                 // dest++
      "decq %2\n\t"                 // n--
      "jnz 1b\n"                    // n != 0 就继续
      "2:"
      : "+r"(dest), "+r"(src), "+r"(n), "=&r"(tmp)
      :
      : "memory", "cc");
  return ret;
}

int asm_setjmp(asm_jmp_buf env) {
  return setjmp(env);
}

void asm_longjmp(asm_jmp_buf env, int val) {
  longjmp(env, val);
}
