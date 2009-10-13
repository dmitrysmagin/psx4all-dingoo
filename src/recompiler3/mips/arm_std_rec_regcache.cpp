#include "../common.h"

static INLINE void regClearJump(void)
{
	int i;
	for(i = 0; i < 32; i++)
	{
		if( regcache.mipsh[i].ismapped )
		{
			int mappedto = regcache.mipsh[i].mappedto;
			if( i != 0 && regcache.mipsh[i].mips_ischanged )
			{
				//DEBUGG("mappedto %d pr %d\n", mappedto, PERM_REG_1);
				MIPS_STR_IMM(MIPS_POINTER, mappedto, PERM_REG_1, CalcDisp(i));
			}
			regcache.mipsh[i].mips_ischanged = false;
			regcache.arm[mappedto].ismapped = regcache.mipsh[i].ismapped = false;
			regcache.arm[mappedto].mappedto = regcache.mipsh[i].mappedto = 0;
			regcache.arm[mappedto].arm_type = REG_EMPTY;
			regcache.arm[mappedto].arm_age = 0;
			regcache.arm[mappedto].arm_use = 0;
			regcache.arm[mappedto].arm_islocked = 0;
		}
	}
}

static INLINE void regFreeRegs(void)
{
	//DEBUGF("regFreeRegs");
	int i = 0;
	int firstfound = 0;
	while(regcache.reglist[i] != 0xFF)
	{
		int armreg = regcache.reglist[i];
		//DEBUGF("spilling %dth reg (%d)", i, armreg);

		if(!regcache.arm[armreg].arm_islocked)
		{
			int mipsreg = regcache.arm[armreg].mappedto;
			if( mipsreg != 0 && regcache.mipsh[mipsreg].mips_ischanged )
			{
				MIPS_STR_IMM(MIPS_POINTER, armreg, PERM_REG_1, CalcDisp(mipsreg));
			}
			regcache.mipsh[mipsreg].mips_ischanged = false;
			regcache.arm[armreg].ismapped = regcache.mipsh[mipsreg].ismapped = false;
			regcache.arm[armreg].mappedto = regcache.mipsh[mipsreg].mappedto = 0;
			regcache.arm[armreg].arm_type = REG_EMPTY;
			regcache.arm[armreg].arm_age = 0;
			regcache.arm[armreg].arm_use = 0;
			regcache.arm[armreg].arm_islocked = 0;
			if(firstfound == 0)
			{
				regcache.reglist_cnt = i;
				//DEBUGF("setting reglist_cnt %d", i);
				firstfound = 1;
			}
		}
		//else DEBUGF("locked :(");
		
		i++;
	}
	if (!firstfound) DEBUGF("FATAL ERROR: unable to free register");
}

static u32 regMipsToArmHelper(u32 regmips, u32 action, u32 type)
{
	//DEBUGF("regMipsToArmHelper regmips %d action %d type %d reglist_cnt %d", regmips, action, type, regcache.reglist_cnt);
	int regnum = regcache.reglist[regcache.reglist_cnt];
	
	//DEBUGF("regnum 1 %d", regnum);
	
	while( regnum != 0xFF )
	{
		//DEBUGF("checking reg %d", regnum);
		if(regcache.arm[regnum].arm_type == REG_EMPTY)
		{
			break;
		}
		regcache.reglist_cnt++;
		//DEBUGF("setting reglist_cnt %d", regcache.reglist_cnt);
		regnum = regcache.reglist[regcache.reglist_cnt];
	}

	//DEBUGF("regnum 2 %d", regnum);
	if( regnum == 0xFF )
	{
		regFreeRegs();
		regnum = regcache.reglist[regcache.reglist_cnt];
		if (regnum == 0xff) regClearJump();
	}

	regcache.arm[regnum].arm_type = type;
	regcache.arm[regnum].arm_islocked++;
	regcache.mipsh[regmips].mips_ischanged = false;

	if( action != REG_LOADBRANCH )
	{
		regcache.arm[regnum].arm_age = 0;
		regcache.arm[regnum].arm_use = 0;
		regcache.arm[regnum].ismapped = true;
		regcache.arm[regnum].mappedto = regmips;
		regcache.mipsh[regmips].ismapped = true;
		regcache.mipsh[regmips].mappedto = regnum;
	}
	else
	{
		regcache.arm[regnum].arm_age = 0;
		regcache.arm[regnum].arm_use = 0xFF;
		regcache.arm[regnum].ismapped = false;
		regcache.arm[regnum].mappedto = 0;
		if( regmips != 0 )
		{
			MIPS_LDR_IMM(MIPS_POINTER, regnum, PERM_REG_1, CalcDisp(regmips));
		}
		else
		{
			MIPS_MOV_REG_IMM8(MIPS_POINTER, regnum, 0);
		}
	
		regcache.reglist_cnt++;
		//DEBUGF("setting reglist_cnt %d", regcache.reglist_cnt);
	
		//DEBUGF("regnum 3 %d", regnum);
		return regnum;
	}

	if(action == REG_LOAD)
	{
		if( regmips != 0 )
		{
			MIPS_LDR_IMM(MIPS_POINTER, regcache.mipsh[regmips].mappedto, PERM_REG_1, CalcDisp(regmips));
		}
		else
		{
			MIPS_MOV_REG_IMM8(MIPS_POINTER, regcache.mipsh[regmips].mappedto, 0);
		}
	}

	regcache.reglist_cnt++;
	//DEBUGF("setting reglist_cnt %d (regnum 4 %d)", regcache.reglist_cnt, regnum);

	return regnum;
}

static INLINE u32 regMipsToArm(u32 regmips, u32 action, u32 type)
{
	//DEBUGF("starting for regmips %d, action %d, type %d", regmips, action, type);
	if( regcache.mipsh[regmips].ismapped )
	{
		//DEBUGF("regmips %d is mapped", regmips);
		if( action != REG_LOADBRANCH )
		{
			int armreg = regcache.mipsh[regmips].mappedto;
			regcache.arm[armreg].arm_islocked++;
			return armreg;
		}
		else
		{
			//DEBUGF("loadbranch regmips %d", regmips);
			u32 mappedto = regcache.mipsh[regmips].mappedto;
			if( regmips != 0 && regcache.mipsh[regmips].mips_ischanged )
			{
				MIPS_STR_IMM(MIPS_POINTER, mappedto, PERM_REG_1, CalcDisp(regmips));
			}
			regcache.mipsh[regmips].mips_ischanged = false;
			regcache.mipsh[regmips].ismapped = false;
			regcache.mipsh[regmips].mappedto = 0;

			regcache.arm[mappedto].arm_type = type;
			regcache.arm[mappedto].arm_age = 0;
			regcache.arm[mappedto].arm_use = 0xFF;
			regcache.arm[mappedto].ismapped = false;
			regcache.arm[mappedto].arm_islocked++;
			regcache.arm[mappedto].mappedto = 0;

			return mappedto;
		}
	}

	//DEBUGF("calling helper");
	return regMipsToArmHelper(regmips, action, type);
}

static INLINE void regMipsChanged(u32 regmips)
{
	regcache.mipsh[regmips].mips_ischanged = true;
}

static INLINE void regBranchUnlock(u32 regarm)
{
	if (regcache.arm[regarm].arm_islocked > 0) regcache.arm[regarm].arm_islocked--;
}

static INLINE void regClearBranch(void)
{
	int i;
	for(i = 1; i < 32; i++)
	{
		if( regcache.mipsh[i].ismapped )
		{
			if( regcache.mipsh[i].mips_ischanged )
			{
				MIPS_STR_IMM(MIPS_POINTER, regcache.mipsh[i].mappedto, PERM_REG_1, CalcDisp(i));
			}
		}
	}
}

static INLINE void regReset()
{
	int i, i2;
	for(i = 0; i < 32; i++)
	{
		regcache.mipsh[i].mips_ischanged = false;
		regcache.mipsh[i].ismapped = false;
		regcache.mipsh[i].mappedto = 0;
	}

	for(i = 0; i < 32; i++)
	{
		regcache.arm[i].arm_type = REG_EMPTY;
		regcache.arm[i].arm_age = 0;
		regcache.arm[i].arm_use = 0;
		regcache.arm[i].arm_islocked = 0;
		regcache.arm[i].ismapped = false;
		regcache.arm[i].mappedto = 0;
	}

	for (i = 0; i < 8 ; i++)
		regcache.arm[i].arm_type = REG_RESERVED;
	for (i = MIPSREG_T0; i <= MIPSREG_T7; i++)
		regcache.arm[i].arm_type = REG_RESERVED;
        for (i = 24 ; i < 32; i++)
        	regcache.arm[i].arm_type = REG_RESERVED;
	regcache.arm[PERM_REG_1].arm_type = REG_RESERVED;
	regcache.arm[BRANCH_COUNT_REG].arm_type = REG_RESERVED;
	regcache.arm[TEMP_1].arm_type = REG_RESERVED;
	regcache.arm[TEMP_2].arm_type = REG_RESERVED;
	regcache.arm[TEMP_3].arm_type = REG_RESERVED;
	
#if 0
	regcache.arm[0].arm_type = REG_RESERVED;
	regcache.arm[1].arm_type = REG_RESERVED;
	regcache.arm[2].arm_type = REG_RESERVED;
	regcache.arm[3].arm_type = REG_RESERVED;
	regcache.arm[TEMP_1].arm_type = REG_RESERVED;
	regcache.arm[TEMP_2].arm_type = REG_RESERVED;
#ifdef IPHONE
	regcache.arm[9].arm_type = REG_RESERVED;
#endif
	regcache.arm[PERM_REG_1].arm_type = REG_RESERVED;
	regcache.arm[13].arm_type = REG_RESERVED;
	regcache.arm[15].arm_type = REG_RESERVED;

	regcache.arm[12].arm_type = REG_RESERVED;
	regcache.arm[14].arm_type = REG_RESERVED;
#endif
	
	for(i = 0, i2 = 0; i < 32; i++)
	{
		if(regcache.arm[i].arm_type == REG_EMPTY)
		{
			regcache.reglist[i2] = i;
			i2++;
		}
	}
	regcache.reglist[i2] = 0xFF;
	//DEBUGF("reglist len %d", i2);
}

