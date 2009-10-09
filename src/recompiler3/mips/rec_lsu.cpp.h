#if 1
static INLINE void iPushOfB()
{
	s32 imm16 = (s32)(s16)_Imm_;
	u32 rs = _Rs_;
	u32 r1;
	if (rs)
	{
		r1 = regMipsToArm(rs, REG_LOAD, REG_REGISTER);
		if (imm16)
		{
			gen(ADDI, MIPSREG_A0, r1, imm16);
		}
		else
		{
			gen(MOV, MIPSREG_A0, r1);
		}
		regBranchUnlock(r1);
	}
	else
	{
		gen(MOVI16, MIPSREG_A0, imm16);
	}
}

static void recLB()
{
// Rt = mem[Rs + Im] (signed)
	u32 rt = _Rt_;

	iPushOfB();
	CALLFunc_NoFlush((u32)psxMemReadS8);
	if (rt)
	{
		u32 r1 = regMipsToArm(rt, REG_FIND, REG_REGISTER);
		MIPS_MOV_REG_REG(MIPS_POINTER, r1, MIPSREG_V0);
		regMipsChanged(rt);
		regBranchUnlock(r1);
	}
}

static void recLBU()
{
// Rt = mem[Rs + Im] (unsigned)
	u32 rt = _Rt_;

	iPushOfB();

	CALLFunc_NoFlush((u32)psxMemRead8);
	if (rt)
	{
		u32 r1 = regMipsToArm(rt, REG_FIND, REG_REGISTER);
		MIPS_MOV_REG_REG(MIPS_POINTER, r1, MIPSREG_V0);
		regMipsChanged(rt);
		regBranchUnlock(r1);
	}
}

static void recLH()
{
// Rt = mem[Rs + Im] (signed)
	u32 rt = _Rt_;

	iPushOfB();
	CALLFunc_NoFlush((u32)psxMemReadS16);
	if (rt)
	{
		u32 r1 = regMipsToArm(rt, REG_FIND, REG_REGISTER);
		MIPS_MOV_REG_REG(MIPS_POINTER, r1, MIPSREG_V0);
		regMipsChanged(rt);
		regBranchUnlock(r1);
	}
}

static void recLHU()
{
// Rt = mem[Rs + Im] (unsigned)
	u32 rt = _Rt_;

	iPushOfB();
	CALLFunc_NoFlush((u32)psxMemRead16);
	if (rt)
	{
		u32 r1 = regMipsToArm(rt, REG_FIND, REG_REGISTER);
		MIPS_MOV_REG_REG(MIPS_POINTER, r1, MIPSREG_V0);
		regMipsChanged(rt);
		regBranchUnlock(r1);
	}
}

static void recLW()
{
// Rt = mem[Rs + Im] (unsigned)
	u32 rt = _Rt_;

	iPushOfB();
	CALLFunc_NoFlush((u32)psxMemRead32);
	if (rt)
	{
		u32 r1 = regMipsToArm(rt, REG_FIND, REG_REGISTER);
		MIPS_MOV_REG_REG(MIPS_POINTER, r1, MIPSREG_V0);
		regMipsChanged(rt);
		regBranchUnlock(r1);
	}
}

REC_FUNC_TEST(LWL);
REC_FUNC_TEST(LWR);

static void recSB()
{
// mem[Rs + Im] = Rt
	u32 rt = _Rt_;

	iPushOfB();	
	if (rt)
	{
		u32 r1 = regMipsToArm(rt, REG_LOAD, REG_REGISTER);
		MIPS_MOV_REG_REG(MIPS_POINTER, MIPSREG_A1, r1);
		regBranchUnlock(r1);
	}
	else
	{
		MIPS_MOV_REG_IMM8(MIPS_POINTER, MIPSREG_A1, 0);
	}
	CALLFunc_NoFlush((u32)psxMemWrite8);
}

static void recSH()
{
// mem[Rs + Im] = Rt
	u32 rt = _Rt_;

	iPushOfB();	
	if (rt)
	{
		u32 r1 = regMipsToArm(rt, REG_LOAD, REG_REGISTER);
		MIPS_MOV_REG_REG(MIPS_POINTER, MIPSREG_A1, r1);
		regBranchUnlock(r1);
	}
	else
	{
		MIPS_MOV_REG_IMM8(MIPS_POINTER, MIPSREG_A1, 0);
	}
	CALLFunc_NoFlush((u32)psxMemWrite16);
}

static void recSW()
{
// mem[Rs + Im] = Rt
	u32 rt = _Rt_;

	iPushOfB();
	if (rt)
	{
		u32 r1 = regMipsToArm(rt, REG_LOAD, REG_REGISTER);
		MIPS_EMIT(MIPS_POINTER, 0x00000021 | (r1 << 21) | (MIPSREG_A1 << 11)); /* move a1, r1 */
		regBranchUnlock(r1);
	}
	else
	{
		MIPS_EMIT(MIPS_POINTER, 0x3c000000 | (MIPSREG_A1 << 16)); /* lui ,0 */
	}
	CALLFunc_NoFlush((u32)psxMemWrite32);
}

REC_FUNC_TEST(SWL);
REC_FUNC_TEST(SWR);

#else

REC_FUNC_TEST(LB);
REC_FUNC_TEST(LBU);
REC_FUNC_TEST(LH);
REC_FUNC_TEST(LHU);
REC_FUNC_TEST(LW);
REC_FUNC_TEST(SB);
REC_FUNC_TEST(SH);
REC_FUNC_TEST(SW);
REC_FUNC_TEST(LWL);
REC_FUNC_TEST(LWR);
REC_FUNC_TEST(SWL);
REC_FUNC_TEST(SWR);
#endif
