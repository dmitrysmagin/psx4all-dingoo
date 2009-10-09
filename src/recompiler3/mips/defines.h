#ifndef __ARM_DEFINES_H__
#define __ARM_DEFINES_H__


//#define WITH_DISASM

#ifdef IPHONE

#define SAVED_ALL_REGS		((1U << ARMREG_R4) | (1U << ARMREG_R5) |			\
							(1U << ARMREG_R6) | (1U << ARMREG_R7) |				\
							(1U << ARMREG_R8)  |								\
							(1U << ARMREG_R10) | (1U << ARMREG_R11) |			\
							(1U << ARMREG_R12) | (1U << ARMREG_LR))

#else

#define SAVED_ALL_REGS		((1U << ARMREG_R4) | (1U << ARMREG_R5) |			\
							(1U << ARMREG_R6) | (1U << ARMREG_R7) |				\
							(1U << ARMREG_R8) | (1U << ARMREG_R9) |				\
							(1U << ARMREG_R10) | (1U << ARMREG_R11) |			\
							(1U << ARMREG_R12) | (1U << ARMREG_LR))

#endif

#define SAVEREG_MASK		( (1U << ARMREG_R2) | (1U << ARMREG_R3) | (1U << ARMREG_R12) | (1U << ARMREG_R14) )

#define REG_CACHE_START		4
#define REG_CACHE_END		14

#define REG_LOAD			0
#define REG_FIND			1
#define REG_LOADBRANCH		2

#define REG_EMPTY			0
#define REG_REGISTER		1
#define REG_TEMPORARY		2
#define REG_RESERVED		3
#define REG_REGISTERBRANCH	4

/* general defines */
#define ARM_POINTER 		0

#ifdef GIZMONDO
#define RECMEM_SIZE			(5*1024*1024)
#define RECMEM_SIZE_MAX 	(RECMEM_SIZE-(512*1024))
#else
#define RECMEM_SIZE			(8*1024*1024)
#define RECMEM_SIZE_MAX 	(RECMEM_SIZE-(512*1024))
#endif

#define TEMP_1 				MIPSREG_T0
#define TEMP_2 				MIPSREG_T1
#define TEMP_3 				MIPSREG_T2

/* PERM_REG_1 is used to store psxRegs struct address */
#define PERM_REG_1 			MIPSREG_S7

/* GPR offset */
#define CalcDisp(rx) ((rx) << 2)

/* CP0 offset */
#define CalcDispCP0(rx) (136 + ((rx) << 2))

/* CP2C offset */
#define CalcDispCP2C(rx) (520 + ((rx) << 2))

/* call func */
#define CALLFunc(func)						 															\
	{																															\
		/* ARM_BL(ARM_POINTER, arm_relative_offset(recMem, func, 8)); */	\
		MIPS_EMIT(ARM_POINTER, 0x0c000000 | ((func & 0x0fffffff) >> 2)); /* jal func */ \
		MIPS_EMIT(ARM_POINTER, 0); /* nop */ \
	}

#define CALLFunc_NoFlush(func)		 											\
	CALLFunc(func)						 										\

#define CALLFunc_Branch(func)		 											\
	/* ARM_BL(ARM_POINTER, arm_relative_offset(recMem, func, 8)); */					\
	MIPS_EMIT(ARM_POINTER, 0x0c000000 | ((func & 0x0fffffff) >> 2)); /* jal func */ \
	MIPS_EMIT(ARM_POINTER, 0); /* nop */ \
	rec_recompile_end(ARMCOND_AL);												\

#ifdef GIZMONDO
#define gp2x_sync() sync()
#else
#define gp2x_sync() sync()
#endif

#define _oB_ (_u32(_rRs_) + _Imm_)

#define write32(val) 															\
	*recMem++ = (u32)(val);			 											\

#if 1

#define arm_relative_offset(source, offset, next) 								\
  ((((u32)(offset) - ((u32)(source) + (next))) >> 2) & 0xFFFFFF)				\

#define mips_relative_offset(source, offset, next) \
  ((((u32)(offset) - ((u32)(source) + (next))) >> 2) & 0xFFFF) \
  

#else

#define arm_relative_offset(source, offset, next) 								\
  arm_patch_relative_offset(((u32)source), ((u32)offset))						\

// Made into a function to avoid a compiler bug in Toolchain 0.30
u32 arm_patch_relative_offset(u32 source, u32 offset)
{
	u32 new_source = source + 8;
	u32 final_offset = ((offset - new_source) / 4) & 0xFFFFFF;
	// It seems it needs this printf with new_source as an argument for some sort of cache issue	
	printf("", new_source);
	return final_offset;
}

#endif

#define LoadImmediate32(imm32, reg)												\
	mips_load_imm32(imm32, reg)													\

#define LoadImmediate32_COND(imm32, reg, condition)								\
	arm_load_imm32_cond(imm32, reg, condition)									\

#define mips_load_imm32(imm, ireg) \
{ \
  MIPS_EMIT(0, 0x3c000000 | (ireg << 16) | ((imm) >> 16)); /* lui */ \
  MIPS_EMIT(0, 0x34000000 | (ireg << 21) | (ireg << 16) | ((imm) & 0xffff)); /* ori */ \
}

#define arm_load_imm32(imm, ireg)												\
{																				\
  u32 store_count = arm_disect_imm_32bit(imm, stores, rotations);				\
  u32 i;																		\
																				\
  ARM_MOV_REG_IMM(0, ireg, stores[0], rotations[0]);							\
																				\
  for(i = 1; i < store_count; i++)												\
  {																				\
    ARM_ORR_REG_IMM(0, ireg, ireg, stores[i], rotations[i]);					\
  }																				\
}																				\

#define arm_load_imm32_cond(imm, ireg, cond)									\
{																				\
  u32 store_count = arm_disect_imm_32bit(imm, stores, rotations);				\
  u32 i;																		\
																				\
  ARM_MOV_REG_IMM_COND(0, ireg, stores[0], rotations[0], cond);					\
																				\
  for(i = 1; i < store_count; i++)												\
  {																				\
    ARM_ORR_REG_IMM_COND(0, ireg, ireg, stores[i], rotations[i], cond);			\
  }																				\
}

#endif
