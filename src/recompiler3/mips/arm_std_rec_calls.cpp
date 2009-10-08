#include "../common.h"

u32 psxBranchTest_rec(u32 cycles, u32 pc)
{
	/* Misc helper */
	psxRegs->pc = pc;
	psxRegs->cycle += (u32)( (float)(cycles) * BIAS_CYCLE_INC );

	/* Make sure interrupts  always when mcd is active */
	//if( mcdst != 0 || (psxRegs->cycle - psxRegs->psx_next_io_base)  >= psxRegs->psx_next_io_count )
	{
		update_hw((u32)( (float)(cycles) * BIAS_CYCLE_INC ));
	}

	u32 compiledpc = (u32)PC_REC32(psxRegs->pc);
	if( compiledpc != 0 )
	{
		DEBUGF("returning to 0x%x (t2 0x%x t3 0x%x)\n", compiledpc, psxRegs->GPR.n.t2, psxRegs->GPR.n.t3);
		return compiledpc;
	}
	u32 a = recRecompile();
	DEBUGF("returning to 0x%x (t2 0x%x t3 0x%x)\n", a, psxRegs->GPR.n.t2, psxRegs->GPR.n.t3);
	return a;
}

#ifdef IPHONE
extern "C" void sys_icache_invalidate(const void* Addr, size_t len);

void clear_insn_cache(u32 start, u32 end, int type)
{
	sys_icache_invalidate((void*)start, end - start);
}
#endif
