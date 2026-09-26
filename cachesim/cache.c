#include "common.h"
#include <inttypes.h>

void mem_read(uintptr_t block_num, uint8_t *buf);
void mem_write(uintptr_t block_num, const uint8_t *buf);

uint8_t* cache_arr;

static uint64_t cache_block_num = 0; //记录cache总块号
static uint64_t cache_associativity = 0; //记录cache关联度
static uint64_t cache_group_num = 0; //记录cache总组号
bool* cache_dirty_arr; //记录每个块是否脏，只有一位，在写操作的时候有用
bool* cache_valid_arr; //记录每个块是否有效，只有一位
uint32_t* cache_tag_arr; //记录每个块的tag 实际上只有(19减log(cache_group_num))位，即空出BLOCK_WIDTH位和组号位
static uint64_t cycle_cnt = 0;

/* ---- 统计计数（仅供 display_statistic 使用，不影响 cache 逻辑）---- */
static int      stat_total_size_width = 0;  // 记录配置，供最终打印
static int      stat_assoc_width = 0;
static uint64_t stat_read    = 0;  // cache 读操作次数
static uint64_t stat_write   = 0;  // cache 写操作次数
static uint64_t stat_miss    = 0;  // 缺失次数（读缺失 + 写缺失）
static uint64_t stat_wb      = 0;  // 写回主存的次数
static uint64_t stat_mem_cyc = 0;  // 花在访问主存上的周期数（mem_read/mem_write 累计）
static uint64_t stat_mem_rd  = 0;  // 主存读次数
static uint64_t stat_mem_wr  = 0;  // 主存写次数

void cycle_increase(int n) {
  cycle_cnt += n;
  if (n == 25) { stat_mem_cyc += n; stat_mem_rd++; }   // mem.c 里只有这两种访存代价
  else if (n == 6) { stat_mem_cyc += n; stat_mem_wr++; }
}

// TODO: implement the following functions

uint32_t cache_read(uintptr_t addr) {
  cycle_increase(1); //无论命中与否，读操作都需要增加一个周期
  stat_read++;

  int offset = addr & (BLOCK_SIZE - 1); //计算偏移量
  int block_num = addr >> BLOCK_WIDTH; //计算主存块号
  int group_num = block_num % cache_group_num; //计算对应组号
  bool hit = false;
  uint32_t ret = -1;
  //遍历组查看是否命中，命中则返回数据，否则从主存中读取数据到cache中，并且随机选择一个块替换掉原来的数据
  for(int i = cache_associativity * group_num; i < cache_associativity * (group_num + 1); i++) {
    if(cache_valid_arr[i] && cache_tag_arr[i] == (block_num / cache_group_num)) {
      hit = true;
      ret = *(uint32_t*)(cache_arr + (i << BLOCK_WIDTH) + offset);
    }
  }
  if(hit) {
    return ret;
  }
  stat_miss++;
  //如果没有命中，则从主存中读取数据到cache中，并且随机选择一个块替换掉原来的数据
  int replace_block = cache_associativity * group_num + (rand() % cache_associativity);
  //从主存中读取数据到cache中
  mem_read(block_num, cache_arr + (replace_block << BLOCK_WIDTH));
  //更新cache的tag和有效位
  cache_tag_arr[replace_block] = block_num / cache_group_num;
  cache_valid_arr[replace_block] = true;
  cache_dirty_arr[replace_block] = false;
  return *(uint32_t*)(cache_arr + (replace_block << BLOCK_WIDTH) + offset);
}
// 往 cache 中 addr 地址所属的块写入数据 data，写掩码为 wmask
// 例如当 wmask 为 0xff 时，只写入低8比特
// 若缺失，需要先从内存中读入数据
void cache_write(uintptr_t addr, uint32_t data, uint32_t wmask) {
   cycle_increase(1); //无论命中与否，写操作都需要增加一个周期
  stat_write++;

  int offset = addr & (BLOCK_SIZE - 1); //计算偏移量
  int block_num = addr >> BLOCK_WIDTH; //计算主存块号
  int group_num = block_num % cache_group_num; //计算对应组号
  bool hit = false;
  for(int i = cache_associativity * group_num; i < cache_associativity * (group_num + 1); i++) {
    if(cache_valid_arr[i] && cache_tag_arr[i] == (block_num / cache_group_num)) {
      hit = true;
      *(uint32_t*)(cache_arr + (i << BLOCK_WIDTH) + offset) = (*(uint32_t*)(cache_arr + (i << BLOCK_WIDTH) + offset) & ~wmask) | (data & wmask);
      cache_dirty_arr[i] = true;
    }
  }
  if(hit) {
    return;
  }
  stat_miss++;

  //如果没有命中，则从主存中读取数据到cache中，并且随机选择一个块替换掉原来的数据
  int replace_block = cache_associativity * group_num + (rand() % cache_associativity);
  //如果替换的块是脏的，则需要写回主存
  if(cache_dirty_arr[replace_block]) {
    int old_block_num = cache_tag_arr[replace_block] * cache_group_num + group_num;
    mem_write(old_block_num, cache_arr + (replace_block << BLOCK_WIDTH));
    stat_wb++;
    cache_dirty_arr[replace_block] = false;
  }
    //从主存中读取数据到cache中
    mem_read(block_num, cache_arr + (replace_block << BLOCK_WIDTH));
    //更新cache的tag和有效位
    cache_tag_arr[replace_block] = block_num / cache_group_num;
    cache_valid_arr[replace_block] = true;
    *(uint32_t*)(cache_arr + (replace_block << BLOCK_WIDTH) + offset) = (*(uint32_t*)(cache_arr + (replace_block << BLOCK_WIDTH) + offset) & ~wmask) | (data & wmask);
    cache_dirty_arr[replace_block] = true;
}

//初始化一个数据大小为2^total_size_width B，关联度为 2^associativity_width 的 cache
void init_cache(int total_size_width, int associativity_width) {
  cache_associativity = exp2(associativity_width);
  cache_arr = (uint8_t*)malloc(exp2(total_size_width) * sizeof(uint8_t)); //分配cache内存
  cache_block_num = exp2(total_size_width - BLOCK_WIDTH); //总块数
  cache_group_num = exp2(total_size_width - BLOCK_WIDTH - associativity_width); //总组数
  cache_dirty_arr = (bool*)malloc(cache_block_num * sizeof(bool)); //分配cache脏位内存
  cache_valid_arr = (bool*)malloc(cache_block_num * sizeof(bool)); //分配cache有效位内存
  cache_tag_arr = (uint32_t*)malloc(cache_block_num * sizeof(uint32_t)); //分配cache tag内存
  //初始化cache有效位和tag
  for (int i = 0; i < cache_block_num; i++) {
    cache_valid_arr[i] = false;
    cache_tag_arr[i] = -1;
    cache_dirty_arr[i] = false;
  }
  stat_total_size_width = total_size_width;
  stat_assoc_width = associativity_width;
}

/* 打印 cache 统计信息：
 *  - 访问构成：读/写次数
 *  - 缺失率：stat_miss / 总访问次数
 *  - 写回次数：write-back 策略的真实代价
 *  - 运行时间模型：cycle_cnt = cache 访问周期 + 主存访问周期
 *      每次 cache 访问 +1 周期（cache_read/cache_write 开头各 +1）
 *      mem_read +25、mem_write +6（见 mem.c）
 *  - 平均每次访问周期数（AMAT），用于横向比较不同 cache 配置 */
void display_statistic(void) {
  uint64_t total  = stat_read + stat_write;
  uint64_t hit    = total - stat_miss;
  uint64_t acccyc = cycle_cnt - stat_mem_cyc;   // 访问 cache 本身花掉的周期

  printf("==================== cache statistic ====================\n");
  printf("cache 配置   : %llu B, %llu 路组相联, %llu 组, 块大小 %d B\n",
         (unsigned long long)exp2(stat_total_size_width),
         (unsigned long long)cache_associativity,
         (unsigned long long)cache_group_num, BLOCK_SIZE);
  printf("总访问次数   : %" PRIu64 "  (读 %" PRIu64 " / 写 %" PRIu64 ")\n",
         total, stat_read, stat_write);
  printf("命中次数     : %" PRIu64 "\n", hit);
  printf("缺失次数     : %" PRIu64 "\n", stat_miss);
  printf("缺失率       : %.4f%%\n",
         total ? 100.0 * (double)stat_miss / (double)total : 0.0);
  printf("写回次数     : %" PRIu64 "\n", stat_wb);
  printf("---------------------------------------------------------\n");
  printf("总周期数     : %" PRIu64 "\n", cycle_cnt);
  printf("  cache 访问 : %" PRIu64 " 周期\n", acccyc);
  printf("  主存访问   : %" PRIu64 " 周期  (读 %" PRIu64 " 次 / 写 %" PRIu64 " 次)\n",
         stat_mem_cyc, stat_mem_rd, stat_mem_wr);
  printf("平均每次访问 : %.4f 周期 (AMAT)\n",
         total ? (double)cycle_cnt / (double)total : 0.0);
  printf("=========================================================\n");
}
