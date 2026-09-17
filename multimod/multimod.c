#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

static uint64_t map[64];

uint64_t modmadd(uint64_t, uint64_t, uint64_t);
void int_map(uint64_t,uint64_t);

uint64_t multimod(uint64_t a, uint64_t b, uint64_t m) {
  init_map(b,m);
  uint64_t result = 0;
  uint64_t ta = a%m;
  uint64_t tb = b%m;
  int top = -1;
  int stack[64];
  while(a!=0){
    uint64_t temp = a%2;
    stack[++top] = (int)temp;
    a = a/2;
  }
  while(top>=0){
    if(stack[top] == 1){
      result = modmadd(result,map[top],m);
    }
    top--;
  }
  return result;
}
//制作一个b*2^i mod m的表，考虑溢出
void init_map(uint64_t b,uint64_t m){
  map[0] = b%m;
  for(int i=1;i<64;i++){
    map[i] = modmadd(map[i-1],map[i-1],m);
  }
}


//计算两个数进行运算之后mod m的情况，考虑了溢出情况
uint64_t modmadd(uint64_t a, uint64_t b, uint64_t m){
  if( (a+b)<a || (a+b) < b){
    return ((a+b)%m + (UINT64_MAX%m) + 1)%m;
  }
  else{
    return (a+b)%m;
  }
}

void mytest(uint64_t a, uint64_t b, uint64_t m){
  char cmd[256];
  char buf[256];
  snprintf(cmd, sizeof cmd,
             "python3 -c 'print(%llu * %llu // %llu)'",
             (unsigned long long)a,
             (unsigned long long)b,
             (unsigned long long)m);
  FILE *fp = popen(cmd, "r");
  assert(fp);
  fscanf(fp, "%s", buf);
  printf("popen() returns: %s\n", buf);
  pclose(fp);
  return;
}
