#include <sys/cachectl.h>
void clear_insn_cache(void)
{
  int a;
  cacheflush(&a, sizeof(a), ICACHE);
}
