/*
 * 生成与 cachesim 框架兼容的访存序列（memory trace）。
 *
 * 记录格式（4 字节/条，只有位域那一个字，没有 data 字段）：
 *   struct _trace { uint32_t addr : 28; uint8_t len : 3; bool is_write : 1; };
 *   低 28 位 = 地址（字节，按 len 对齐）
 *   第 28-30 位 = 访问长度（1/2/4）
 *   第 31 位 = 写标志
 *
 * 为什么是 4 字节而不是 8 字节：
 *   main.c 里是 fread(&t.t, sizeof(t.t), 1, fp)，即每条记录只读 sizeof(struct _trace) = 4 字节。
 *   如果文件里每条写 8 字节（把 struct trace 的 data 字段也写进去），
 *   框架会把下一条记录的 data 当成本条的 addr/len/is_write 读进来 —— 整个重放错位，
 *   统计结果完全错误。实测：8 字节版本会让 hot/random 这类含写操作的 trace 直接段错误。
 *
 * 用法:
 *   ./gen_trace random 1000000 out.bin && bzip2 -kf out.bin
 *   cd ../cachesim && ./cachesim-64 -r 1 ../tools/out.bin.bz2
 *
 * 注意: trace 文件名/路径不要带非 ASCII 字符。main.c 用 popen("bzcat %s") 拼命令，
 *       在某些 locale 下中文路径会被破坏，bzcat 打不开，fread 立刻返回 0，
 *       于是 t 是未初始化的栈内存，喂进 cache 会读到随机地址 → 段错误。
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MEM_SIZE (1u << 25)          /* 与 common.h 一致：32 MB */

static FILE *out;
static long nrec = 0;

static void emit(uint32_t addr, unsigned len, unsigned is_write) {
  uint32_t w = (addr & 0x0fffffffu)
             | ((uint32_t)(len & 7u) << 28)
             | ((uint32_t)(is_write & 1u) << 31);
  if (fwrite(&w, 1, 4, out) != 4) { perror("fwrite"); exit(1); }
  nrec++;
}

static uint32_t align_down(uint32_t a, unsigned len) {
  return len ? (a & ~(uint32_t)(len - 1)) : a;
}

/* 随机访存：最坏情况，冲突缺失多 */
static void gen_random(unsigned long n) {
  uint32_t lens[3] = {1, 2, 4};
  for (unsigned long i = 0; i < n; i++) {
    unsigned len = lens[rand() % 3];
    uint32_t addr = align_down((uint32_t)((unsigned long long)rand() * MEM_SIZE / RAND_MAX), len);
    emit(addr, len, rand() & 1);
  }
}

/* 顺序扫描：空间局部性极好，缺失几乎只来自块边界 */
static void gen_seq(unsigned long n) {
  for (unsigned long i = 0; i < n; i++)
    emit(align_down((uint32_t)((i * 4) % MEM_SIZE), 4), 4, 0);
}

/* 固定步长跨步：步长超过 64B 块大小时命中率骤降，
   最能体现直接映射 vs 组相联、容量大小的差别 */
static void gen_stride(unsigned long n, uint32_t stride) {
  for (unsigned long i = 0; i < n; i++)
    emit(align_down((uint32_t)((i * stride) % MEM_SIZE), 4), 4, (i % 3 == 0));
}

/* 密集顺序扫描，但工作集逐步扩大：
   前 n/2 条只在 [0, W) 内密集走，之后每次把窗口起点往后挪 W，
   于是工作集随访问不断增长 —— 能真正压出"容量不够"的缺失，
   是区分不同 cache 大小/相联度的有效 workload。 */
static void gen_footprint(unsigned long n, uint32_t window) {
  unsigned long half = n / 2;
  uint32_t phase = 0;
  for (unsigned long i = 0; i < n; i++) {
    if (i >= half && (i - half) % window == 0) phase++;
    uint32_t off = (uint32_t)((i % window) << 2);
    uint32_t base = (uint32_t)(((unsigned long long)phase * window) % (MEM_SIZE - window));
    emit(align_down(base + off, 4), 4, 0);
  }
}

/* 少量热点块反复访问：时间局部性极好，用来观察容量效应 */
static void gen_hot(unsigned long n, uint32_t ngroups) {
  for (unsigned long i = 0; i < n; i++) {
    uint32_t addr = align_down(((uint32_t)(rand() % ngroups) << 6) | (uint32_t)(rand() & 0x3c), 4);
    emit(addr, 4, (i % 5 == 0));
  }
}

int main(int argc, char **argv) {
  const char *mode = (argc > 1) ? argv[1] : "random";
  unsigned long n = (argc > 2) ? strtoul(argv[2], 0, 0) : 1000000UL;
  const char *path = (argc > 3) ? argv[3] : "gen.bin";

  srand(12345);

  out = fopen(path, "wb");
  if (!out) { perror("fopen"); return 1; }

  if (!strcmp(mode, "random"))          gen_random(n);
  else if (!strcmp(mode, "seq"))        gen_seq(n);
  else if (!strcmp(mode, "stride4"))    gen_stride(n, 4);
  else if (!strcmp(mode, "stride128"))  gen_stride(n, 128);
  else if (!strcmp(mode, "stride4096")) gen_stride(n, 4096);
  else if (!strcmp(mode, "footprint"))  gen_footprint(n, 1u << 12);   /* 16 KB 窗口 */
  else if (!strcmp(mode, "hot"))        gen_hot(n, 64);
  else { fprintf(stderr, "unknown mode %s\n", mode); return 1; }

  fclose(out);
  fprintf(stderr, "%s: %ld 条记录, %ld 字节 -> %s\n", mode, nrec, nrec * 4, path);
  return 0;
}
