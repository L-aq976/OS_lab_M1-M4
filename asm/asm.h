#include <stdint.h>
#include <stddef.h>

// 跳转缓冲区：8 个 64 位格子，依次存
//   env[0..5] = rbx rbp r12 r13 r14 r15   (SysV 的 callee-saved 寄存器)
//   env[6]    = rsp   本函数 ret 之后调用者看到的栈指针
//   env[7]    = rip   返回地址，也就是 longjmp 的跳转目标
typedef long asm_jmp_buf[8];

int64_t asm_add(int64_t a, int64_t b);
int     asm_popcnt(uint64_t x);
void   *asm_memcpy(void *dest, const void *src, size_t n);

// returns_twice 让编译器知道这函数会返回两次，否则调用者的局部变量会被优化坏；
// noreturn 告诉编译器 longjmp 不会正常返回。
__attribute__((returns_twice)) int  asm_setjmp(asm_jmp_buf env);
__attribute__((noreturn))      void asm_longjmp(asm_jmp_buf env, int val);
