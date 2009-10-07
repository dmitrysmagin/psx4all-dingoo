#include <sys/cachectl.h>
extern "C" void clear_insn_cache(void)
{
  int a;
  cacheflush(&a, 4, ICACHE);
}
