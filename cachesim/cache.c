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

void cycle_increase(int n) { cycle_cnt += n; }

// TODO: implement the following functions

uint32_t cache_read(uintptr_t addr) {
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
    cycle_increase(1);
    return ret;
  }
  //如果没有命中，则从主存中读取数据到cache中，并且随机选择一个块替换掉原来的数据
  int replace_block = cache_associativity * group_num + (rand() % cache_associativity);
  //从主存中读取数据到cache中
  mem_read(block_num, cache_arr + (replace_block << BLOCK_WIDTH));
  //更新cache的tag和有效位
  cache_tag_arr[replace_block] = block_num / cache_group_num;
  cache_valid_arr[replace_block] = true;
  return *(uint32_t*)(cache_arr + (replace_block << BLOCK_WIDTH) + offset);
}
// 往 cache 中 addr 地址所属的块写入数据 data，写掩码为 wmask
// 例如当 wmask 为 0xff 时，只写入低8比特
// 若缺失，需要先从内存中读入数据
void cache_write(uintptr_t addr, uint32_t data, uint32_t wmask) {
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
  }
  //初始化cache内存
  for(int i = 0;i<total_size_width;i++){
    cache_arr[i] = rand() & 0xff;
  }
}

void display_statistic(void) {
}
