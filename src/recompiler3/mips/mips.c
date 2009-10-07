#include <sys/cachectl.h>
void clear_insn_cache(void* x, int size, int flags)
{
  cacheflush(x, size, ICACHE);
}
