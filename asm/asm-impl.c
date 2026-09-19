#include "asm.h"
#include <string.h>

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
  return memcpy(dest, src, n);
}

int asm_setjmp(asm_jmp_buf env) {
  return setjmp(env);
}

void asm_longjmp(asm_jmp_buf env, int val) {
  longjmp(env, val);
}
