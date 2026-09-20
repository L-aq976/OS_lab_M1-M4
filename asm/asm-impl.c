#include "asm.h"
#include <string.h>

int64_t asm_add(int64_t a, int64_t b) {
  // a=a+b;
  asm("addq %1, %0" : "+r"(a) : "r"(b) : "cc");
  return a;
}

int asm_popcnt(uint64_t x) {
  // btq/adcq 是 64 位指令，操作数必须是 64 位寄存器，所以 s 和 i 都用 int64_t
  int64_t s = 0;
  for (int64_t i = 0; i < 64; i++) {
    // if ((x >> i) & 1) s++;
     asm("btq %1, %2\n\t"
      "adcq $0, %0"
      : "+r"(s)            // %0: 累加器，读写
      : "r"(i), "r"(x)     // %1: 位号, %2: 被测试值
      : "cc");
      // btq %1, %2 —— Bit Test（quadword，64 位）。AT&T 是「bt 位号, 数值」，
      // 把「数值的第 位号 位」取出来放进 CF（进位标志），不改其他任何寄存器。
      // 注意别写反：Intel 手册里是 BT 数值, 位号，AT&T 两个操作数是反过来的。
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

/* ------------------------------------------------------------------
 * asm_setjmp / asm_longjmp
 *
 * 「环境」就是 8 个 64 位值，正好是 env 的 8 个格子：
 *   env[0..5] = rbx rbp r12 r13 r14 r15
 *               这 6 个是 SysV 的 callee-saved 寄存器，调用者指望它们
 *               跨过函数调用之后还是原值，所以必须存。
 *   env[6]    = rsp   本函数 ret 之后调用者看到的栈指针（注意不是当前的
 *               rsp：longjmp 回去时要假装这个函数已经返回了）
 *   env[7]    = rip   返回地址，也就是 longjmp 的跳转目标
 * rax/rcx/rdx/rsi/rdi/r8~r11 和所有 xmm 都是 caller-saved，ABI 本来就
 * 允许被调用者随便毁掉，所以一个都不用存。
 *
 * 这两个函数只能用真正的汇编写：保存环境要拿「函数入口处的 rsp 和返回
 * 地址」，内联汇编拿不到（那时 rsp 已经被 prologue 动过了），而 x86 上
 * GCC 又不支持 __attribute__((naked))。所以用文件作用域的顶层 asm 实现，
 * 仍然放在本文件里，Makefile 不用改。
 * ------------------------------------------------------------------ */
#if defined(__x86_64__)
__asm__(
    ".text\n"
    ".p2align 4\n"
    ".globl asm_setjmp\n"
    ".type  asm_setjmp, @function\n"
    "asm_setjmp:\n"                     /* 按 SysV 调用约定，第 1 个整型/指针参数放 rdi（第 2 个 rsi、第 3 个 rdx…），所以 C 里的 asm_setjmp(buf) 编译出来就是「把 buf 的地址塞进 rdi，然后 call」。 */
    "    movq  %rbx,  0(%rdi)\n"        
    "    movq  %rbp,  8(%rdi)\n"
    "    movq  %r12, 16(%rdi)\n"
    "    movq  %r13, 24(%rdi)\n"
    "    movq  %r14, 32(%rdi)\n"
    "    movq  %r15, 40(%rdi)\n"
    "    leaq  8(%rsp), %rdx\n"         /* ret 之后调用者看到的 rsp */
    "    movq  %rdx, 48(%rdi)\n"
    "    movq  (%rsp), %rdx\n"          /* 返回地址 */
    "    movq  %rdx, 56(%rdi)\n"
    "    xorl  %eax, %eax\n"            /* 首次返回 0 */
    "    ret\n"
    ".size asm_setjmp, .-asm_setjmp\n"

    ".p2align 4\n"
    ".globl asm_longjmp\n"
    ".type  asm_longjmp, @function\n"
    "asm_longjmp:\n"                    /* rdi = env, esi = val */
    "    movl  %esi, %eax\n"
    "    testl %eax, %eax\n"
    "    jne   .Lasm_longjmp_nonzero\n"
    "    movl  $1, %eax\n"              /* val == 0 必须变成 1，否则和首次返回分不清 */
    ".Lasm_longjmp_nonzero:\n"
    "    movq   0(%rdi), %rbx\n"
    "    movq   8(%rdi), %rbp\n"
    "    movq  16(%rdi), %r12\n"
    "    movq  24(%rdi), %r13\n"
    "    movq  32(%rdi), %r14\n"
    "    movq  40(%rdi), %r15\n"
    "    movq  48(%rdi), %rsp\n"        /* 换回当时的栈 */
    "    jmp   *56(%rdi)\n"             /* 跳回返回地址，调用者以为 setjmp 又返回了一次 */
    ".size asm_longjmp, .-asm_longjmp\n"
);
#else
#error "asm_setjmp/asm_longjmp
#endif
