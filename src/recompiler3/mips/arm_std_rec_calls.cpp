#include "../common.h"

u32 psxBranchTest_rec(u32 cycles, u32 pc)
{
	static u32 cum = 0;
	/* Misc helper */
	psxRegs->pc = pc;
	psxRegs->cycle += cycles;
	cum += cycles;

	/* Make sure interrupts  always when mcd is active */
	//if( mcdst != 0 || (psxRegs->cycle - psxRegs->psx_next_io_base)  >= psxRegs->psx_next_io_count )
	if (cum > UPDATE_HW_PERIOD)
	{
		update_hw(cum);
		cum = 0;
	}

	u32 compiledpc = (u32)PC_REC32(psxRegs->pc);
	if( compiledpc != 0 )
	{
		//DEBUGF("returning to 0x%x (t2 0x%x t3 0x%x)\n", compiledpc, psxRegs->GPR.n.t2, psxRegs->GPR.n.t3);
		return compiledpc;
	}
	u32 a = recRecompile();
	//DEBUGF("returning to 0x%x (t2 0x%x t3 0x%x)\n", a, psxRegs->GPR.n.t2, psxRegs->GPR.n.t3);
	return a;
}

u32 psxBranchTest_simple(s32 cycles, u32 pc)
{
	//DEBUGF("btsimple cycles %d pc 0x%x", cycles, pc);
	psxRegs->pc = pc;
	psxRegs->cycle += UPDATE_HW_PERIOD - cycles;
	update_hw(UPDATE_HW_PERIOD - cycles);
	u32 compiledpc = (u32)PC_REC32(psxRegs->pc);
	if (compiledpc) return compiledpc;
	return recRecompile();
}

#ifdef IPHONE
extern "C" void sys_icache_invalidate(const void* Addr, size_t len);

void clear_insn_cache(u32 start, u32 end, int type)
{
	sys_icache_invalidate((void*)start, end - start);
}
#endif
