
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <sys/unistd.h>

#include "patchcode.h"
#include "memory.h"
#include "gecko.h"
#include "wifi_gecko.h"
#include "menu.h"
#include "identify.h"

u32 hooktype;
u8 configbytes[2];
u8 menuhook;

extern void patchhook(u32 address, u32 len);
extern void patchhook2(u32 address, u32 len);
extern void patchhook3(u32 address, u32 len);

extern void multidolpatchone(u32 address, u32 len);
extern void multidolpatchtwo(u32 address, u32 len);

extern void regionfreejap(u32 address, u32 len);
extern void regionfreeusa(u32 address, u32 len);
extern void regionfreepal(u32 address, u32 len);

extern void removehealthcheck(u32 address, u32 len);

extern void copyflagcheck1(u32 address, u32 len);
extern void copyflagcheck2(u32 address, u32 len);
extern void copyflagcheck3(u32 address, u32 len);
extern void copyflagcheck4(u32 address, u32 len);
extern void copyflagcheck5(u32 address, u32 len);
extern void copyflagcheck6(u32 address, u32 len);
extern void copyflagcheck7(u32 address, u32 len);
extern void copyflagcheck8(u32 address, u32 len);

extern void patchupdatecheck(u32 address, u32 len);

extern void movedvdhooks(u32 address, u32 len);

extern void multidolhook(u32 address);
extern void langvipatch(u32 address, u32 len, u8 langbyte);
extern void vipatch(u32 address, u32 len);

static const u32 multidolpatch1[2] = {0x3C03FFB4,0x28004F43};
static const u32 multidolpatch2[2] = {0x3F608000, 0x807B0018};

static const u32 healthcheckhook[2] = {0x41810010,0x881D007D};
static const u32 updatecheckhook[3] = {0x80650050,0x80850054,0xA0A50058};

static const u32 recoveryhooks[3] = {0xA00100AC,0x5400073E,0x2C00000F};

static const u32 nocopyflag1[3] = {0x540007FF, 0x4182001C, 0x80630068};
static const u32 nocopyflag2[3] = {0x540007FF, 0x41820024, 0x387E12E2};
static const u32 nocopyflag3[5] = {0x2C030000, 0x41820200,0x48000058,0x38610100};
static const u32 nocopyflag4[4] = {0x80010008, 0x2C000000, 0x4182000C, 0x3BE00001};
static const u32 nocopyflag5[3] = {0x801D0024,0x540007FF,0x41820024};
//from preloader hack for system 513 (4.3 USA)
static const u32 nocopyflag6[4] = {0x28000001, 0x4082001C, 0x80630068, 0x3880011C};
static const u32 nocopyflag7[3] = {0x540007FF, 0x41820024, 0x387D164A};
static const u32 nocopyflag8[5] = {0x2C030000, 0x40820010, 0x88010020, 0x28000002, 0x418201C8};

static const u32 movedvdpatch[3] = {0x2C040000, 0x41820120, 0x3C608109};
static const u32 regionfreehooks[5] = {0x7C600774, 0x2C000001, 0x41820030,0x40800010,0x2C000000};
static const u32 cIOScode[16] = {0x7f06c378, 0x7f25cb78, 0x387e02c0, 0x4cc63182};
static const u32 cIOSblock[16] = {0x2C1800F9, 0x40820008, 0x3B000024};
static const u32 fwritepatch[8] = {0x9421FFD0,0x7C0802A6,0x90010034,0xBF210014,0x7C9B2378,0x7CDC3378,0x7C7A1B78,0x7CB92B78};  // bushing fwrite
static const u32 vipatchcode[3] = {0x4182000C,0x4180001C,0x48000018};

const u32 viwiihooks[4] = {0x7CE33B78,0x38870034,0x38A70038,0x38C7004C};
const u32 kpadhooks[4] = {0x9A3F005E,0x38AE0080,0x389FFFFC,0x7E0903A6};
const u32 kpadoldhooks[6] = {0x801D0060, 0x901E0060, 0x801D0064, 0x901E0064, 0x801D0068, 0x901E0068};
const u32 joypadhooks[4] = {0x3AB50001, 0x3A73000C, 0x2C150004, 0x3B18000C};
const u32 gxdrawhooks[4] = {0x3CA0CC01, 0x38000061, 0x3C804500, 0x98058000};
const u32 gxflushhooks[4] = {0x90010014, 0x800305FC, 0x2C000000, 0x41820008};
const u32 ossleepthreadhooks[4] = {0x90A402E0, 0x806502E4, 0x908502E4, 0x2C030000};
const u32 axnextframehooks[4] = {0x3800000E, 0x7FE3FB78, 0xB0050000, 0x38800080};
const u32 wpadbuttonsdownhooks[4] = {0x7D6B4A14, 0x816B0010, 0x7D635B78, 0x4E800020};
const u32 wpadbuttonsdown2hooks[4] = {0x7D6B4A14, 0x800B0010, 0x7C030378, 0x4E800020};

const u32 multidolhooks[4] = {0x7C0004AC, 0x4C00012C, 0x7FE903A6, 0x4E800420};
const u32 multidolchanhooks[4] = {0x4200FFF4, 0x48000004, 0x38800000, 0x4E800020};

const u32 langpatch[3] = {0x7C600775, 0x40820010, 0x38000000};

static const u32 oldpatch002[3] = {0x2C000000, 0x40820214, 0x3C608000};
static const u32 newpatch002[3] = {0x2C000000, 0x48000214, 0x3C608000};

bool dogamehooks(void *addr, u32 len, bool channel)
{
	/*
	0 No Hook
	1 VBI
	2 KPAD read
	3 Joypad Hook
	4 GXDraw Hook
	5 GXFlush Hook
	6 OSSleepThread Hook
	7 AXNextFrame Hook
	*/

	void *addr_start = addr;
	void *addr_end = addr+len;

	while(addr_start < addr_end)
	{
		switch(hooktype)
		{
			case 0x00:
				hookpatched = true;
				break;

			case 0x01:
				if(memcmp(addr_start, viwiihooks, sizeof(viwiihooks))==0)
				{
					patchhook((u32)addr_start, len);
					wifi_printf("patchcode_dogamehooks: viwiihooks at = %08X\n", addr_start);
					hookpatched = true;
				}
				break;

			case 0x02:
				if(memcmp(addr_start, kpadhooks, sizeof(kpadhooks))==0)
				{
					patchhook((u32)addr_start, len);
					wifi_printf("patchcode_dogamehooks: kpadhooks at = %08X\n", addr_start);
					hookpatched = true;
				}

				if(memcmp(addr_start, kpadoldhooks, sizeof(kpadoldhooks))==0)
				{
					patchhook((u32)addr_start, len);
					wifi_printf("patchcode_dogamehooks: kpadoldhooks at = %08X\n", addr_start);
					hookpatched = true;
				}
				break;

			case 0x03:
				if(memcmp(addr_start, joypadhooks, sizeof(joypadhooks))==0)
				{
					patchhook((u32)addr_start, len);
					wifi_printf("patchcode_dogamehooks: joypadhooks at = %08X\n", addr_start);
					hookpatched = true;
				}
				break;

			case 0x04:
				if(memcmp(addr_start, gxdrawhooks, sizeof(gxdrawhooks))==0)
				{
					patchhook((u32)addr_start, len);
					wifi_printf("patchcode_dogamehooks: gxdrawhooks at = %08X\n", addr_start);
					hookpatched = true;
				}
				break;

			case 0x05:
				if(memcmp(addr_start, gxflushhooks, sizeof(gxflushhooks))==0)
				{
					patchhook((u32)addr_start, len);
					wifi_printf("patchcode_dogamehooks: gxflushhooks at = %08X\n", addr_start);
					hookpatched = true;
				}
				break;

			case 0x06:
				if(memcmp(addr_start, ossleepthreadhooks, sizeof(ossleepthreadhooks))==0)
				{
					patchhook((u32)addr_start, len);
					wifi_printf("patchcode_dogamehooks: ossleepthreadhooks at = %08X\n", addr_start);
					hookpatched = true;
				}
				break;

			case 0x07:
				if(memcmp(addr_start, axnextframehooks, sizeof(axnextframehooks))==0)
				{
					patchhook((u32)addr_start, len);
					wifi_printf("patchcode_dogamehooks: axnextframehooks at = %08X\n", addr_start);
					hookpatched = true;
				}
				break;
		}
		if (hooktype != 0)
		{
			if(channel && memcmp(addr_start, multidolchanhooks, sizeof(multidolchanhooks))==0)
			{
				*(((u32*)addr_start)+1) = 0x7FE802A6;
				DCFlushRange(((u32*)addr_start)+1, 4);

				multidolhook((u32)addr_start+sizeof(multidolchanhooks)-4);
				wifi_printf("patchcode_dogamehooks: multidolchanhooks at = %08X\n", addr_start);
			}
			else if(!channel && memcmp(addr_start, multidolhooks, sizeof(multidolhooks))==0)
			{
				multidolhook((u32)addr_start+sizeof(multidolhooks)-4);
				wifi_printf("patchcode_dogamehooks: multidoldolhooks at = %08X\n", addr_start);
			}			
		}
		addr_start += 4;
	}
	return hookpatched;
}

bool domenuhooks(void *addr, u32 len)
{
	void *addr_start = addr;
	
	// Patches
	if(config_bytes[8] == 0x01){
		patchmenu(addr_start, len, 0);	// menu size
		return menuhook;
	}
	// Region Free
	if(config_bytes[9] == 0x01){
		patchmenu(addr_start, len, 2);	
	}
	// Health check
	if(config_bytes[11] == 0x01){
		patchmenu(addr_start, len, 3);
	}
	// No copy bit remove
	if(config_bytes[10] == 0x01){
		patchmenu(addr_start, len, 4);
	}
	
	if (config_bytes[2] != 0x00) {
		menuhook = 0;
		patchmenu(addr_start, len, 1);
	}
	
	// move dvd channel
	patchmenu(addr_start, len, 5);
	
	DCFlushRange(addr_start, len);
	
	return menuhook;
}

void patchmenu(void *addr, u32 len, int patchnum)
{
	void *addr_start = addr;
	void *addr_end = addr+len;
	
	wifi_printf("patchcode_patchmenu: patchnum value = %d\n", patchnum);
	while(addr_start < addr_end)
	{
		switch (patchnum)
		{
			case 0:
				if(memcmp(addr_start, recoveryhooks, sizeof(recoveryhooks))==0){
					patchhook3((u32)addr_start, len);
					menuhook = 1;
					wifi_printf("patchcode_patchmenu: found recoveryhooks = %08X\n", (u32)addr_start);
				}
				break;
				
			case 1:
				if(memcmp(addr_start, viwiihooks, sizeof(viwiihooks))==0){
					patchhook2((u32)addr_start, len);
					menuhook = 1;
					wifi_printf("patchcode_patchmenu: found viwiihooks = %08X\n", (u32)addr_start);
				}
				
				break;
				
			case 2:
				// jap region free	
				if(memcmp(addr_start, regionfreehooks, sizeof(regionfreehooks))==0){
					regionfreejap((u32)addr_start, len);
					wifi_printf("patchcode_patchmenu: found regionfreejap = %08X\n", (u32)addr_start);
				}
				
				// usa region free
				if(memcmp(addr_start, regionfreehooks, sizeof(regionfreehooks))==0){
					regionfreeusa((u32)addr_start, len);
					wifi_printf("patchcode_patchmenu: found regionfreeusa = %08X\n", (u32)addr_start);
				}
				
				// pal region free
				if(memcmp(addr_start, regionfreehooks, sizeof(regionfreehooks))==0){
					regionfreepal((u32)addr_start, len);
					wifi_printf("patchcode_patchmenu: found regionfreepal = %08X\n", (u32)addr_start);
				}
				
				// skip disc update
				if(memcmp(addr_start, updatecheckhook, sizeof(updatecheckhook))==0){
					patchupdatecheck((u32)addr_start, len);
					wifi_printf("patchcode_patchmenu: found updatecheckhook = %08X\n", (u32)addr_start);
				}
				break;
				
				// button skip
			case 3:
				if(memcmp(addr_start, healthcheckhook, sizeof(healthcheckhook))==0){
					removehealthcheck((u32)addr_start, len);
					wifi_printf("patchcode_patchmenu: found healthcheckhook = %08X\n", (u32)addr_start);
				}
				break;
				
				// no copy flags
			case 4:
				// Remove the actual flag so can copy back
				if(memcmp(addr_start, nocopyflag5, sizeof(nocopyflag5))==0){
					copyflagcheck5((u32)addr_start, len);
					wifi_printf("patchcode_patchmenu: found nocopyflag5 = %08X\n", (u32)addr_start);
				}
				
				
				if(memcmp(addr_start, nocopyflag1, sizeof(nocopyflag1))==0){
					copyflagcheck1((u32)addr_start, len);
					wifi_printf("patchcode_patchmenu: found nocopyflag1 = %08X\n", (u32)addr_start);
				}
				
				if(memcmp(addr_start, nocopyflag2, sizeof(nocopyflag2))==0){
					copyflagcheck2((u32)addr_start, len);
					wifi_printf("patchcode_patchmenu: found nocopyflag2 = %08X\n", (u32)addr_start);
				}
				
				if(memcmp(addr_start, nocopyflag3, sizeof(nocopyflag2))==0){
					copyflagcheck3((u32)addr_start, len);
					wifi_printf("patchcode_patchmenu: found nocopyflag3 = %08X\n", (u32)addr_start);
				}

				if(memcmp(addr_start, nocopyflag4, sizeof(nocopyflag4))==0){
					copyflagcheck4((u32)addr_start, len);
					wifi_printf("patchcode_patchmenu: found nocopyflag4 = %08X\n", (u32)addr_start);
				}
				
				if(memcmp(addr_start, nocopyflag6, sizeof(nocopyflag6))==0){
					copyflagcheck6((u32)addr_start, len);
					wifi_printf("patchcode_patchmenu: found nocopyflag6 = %08X\n", (u32)addr_start);
				}
				
				if(memcmp(addr_start, nocopyflag7, sizeof(nocopyflag7))==0){
					copyflagcheck7((u32)addr_start, len);
					wifi_printf("patchcode_patchmenu: found nocopyflag7 = %08X\n", (u32)addr_start);
				}
				
				if(memcmp(addr_start, nocopyflag8, sizeof(nocopyflag8))==0){
					copyflagcheck8((u32)addr_start, len);
					wifi_printf("patchcode_patchmenu: found nocopyflag8 = %08X\n", (u32)addr_start);
				}				
				break;
				
				// move disc channel
			case 5:
				if(memcmp(addr_start, movedvdpatch, sizeof(movedvdpatch))==0){
					movedvdhooks((u32)addr_start, len);
					wifi_printf("patchcode_patchmenu: found movvedvdhooks = %08X\n", (u32)addr_start);
				}
				break;
		}
		addr_start += 4;
	}
}

void langpatcher(void *addr, u32 len)
{
	void *addr_start = addr;
	void *addr_end = addr+len;

	while(addr_start < addr_end)
	{
		if(memcmp(addr_start, langpatch, sizeof(langpatch))==0)
			if(configbytes[0] != 0xCD) {
				langvipatch((u32)addr_start, len, configbytes[0]);
				wifi_printf("patchcode_langpatcher: langpatcher found at %08X\n", addr_start);
			}
		addr_start += 4;
	}
}

void vidolpatcher(void *addr, u32 len)
{
	void *addr_start = addr;
	void *addr_end = addr+len;

	while(addr_start < addr_end)
	{
		if(memcmp(addr_start, vipatchcode, sizeof(vipatchcode))==0) {
			vipatch((u32)addr_start, len);
			wifi_printf("patchcode_vidolpatcher: vidolpatcher found at %08X\n", addr_start);
		}
		addr_start += 4;
	}
}

void PatchAspectRatio(void *addr, u32 len, u8 aspect)
{
	if(aspect > 1)
		return;
	wifi_printf("patchcode_PatchAspectRatio: aspect value = %d\n", aspect);

	static const u32 aspect_searchpattern1[5] = {
		0x9421FFF0, 0x7C0802A6, 0x38800001, 0x90010014, 0x38610008
	};

	static const u32 aspect_searchpattern2[15] = {
		0x2C030000, 0x40820010, 0x38000000, 0x98010008, 0x48000018,
		0x88010008, 0x28000001, 0x4182000C, 0x38000000, 0x98010008,
		0x80010014, 0x88610008, 0x7C0803A6, 0x38210010, 0x4E800020
	};

	u8 *addr_start = (u8 *) addr;
	u8 *addr_end = addr_start + len - sizeof(aspect_searchpattern1) - 4 - sizeof(aspect_searchpattern2);

	while(addr_start < addr_end)
	{
		if(   (memcmp(addr_start, aspect_searchpattern1, sizeof(aspect_searchpattern1)) == 0)
		   && (memcmp(addr_start + 4 + sizeof(aspect_searchpattern1), aspect_searchpattern2, sizeof(aspect_searchpattern2)) == 0))
		{
			*((u32 *)(addr_start+0x44)) = (0x38600000 | aspect);
			wifi_printf("patchcode_PatchAspectRatio: aspect patch found at %08X\n", addr_start);
			break;
		}
		addr_start += 4;
	}
}

void PatchCountryStrings(void *Address, int Size)
{
	u8 SearchPattern[4] = {0x00, 0x00, 0x00, 0x00};
	u8 PatchData[4] = {0x00, 0x00, 0x00, 0x00};
	u8 *Addr = (u8*)Address;
	int wiiregion = CONF_GetRegion();

	wifi_printf("patchcode_PatchCountryStrings: wiiregion value = %d\n", wiiregion);
	switch(wiiregion)
	{
		case CONF_REGION_JP:
			SearchPattern[0] = 0x00;
			SearchPattern[1] = 'J';
			SearchPattern[2] = 'P';
			break;
		case CONF_REGION_EU:
			SearchPattern[0] = 0x02;
			SearchPattern[1] = 'E';
			SearchPattern[2] = 'U';
			break;
		case CONF_REGION_KR:
			SearchPattern[0] = 0x04;
			SearchPattern[1] = 'K';
			SearchPattern[2] = 'R';
			break;
		case CONF_REGION_CN:
			SearchPattern[0] = 0x05;
			SearchPattern[1] = 'C';
			SearchPattern[2] = 'N';
			break;
		case CONF_REGION_US:
		default:
			SearchPattern[0] = 0x01;
			SearchPattern[1] = 'U';
			SearchPattern[2] = 'S';
	}

	const char DiscRegion = ((u8*)Disc_ID)[3];
	wifi_printf("PatchCountryStrings: DiscRegion value = %c\n", DiscRegion);
	switch(DiscRegion)
	{
		case 'J':
			PatchData[1] = 'J';
			PatchData[2] = 'P';
			break;
		case 'D':
		case 'F':
		case 'P':
		case 'X':
		case 'Y':
			PatchData[1] = 'E';
			PatchData[2] = 'U';
			break;

		case 'E':
		default:
			PatchData[1] = 'U';
			PatchData[2] = 'S';
	}
	while (Size >= 4)
	{
		if(Addr[0] == SearchPattern[0] && Addr[1] == SearchPattern[1] && Addr[2] == SearchPattern[2] && Addr[3] == SearchPattern[3])
		{
			Addr += 1;
			*Addr = PatchData[1];
			Addr += 1;
			*Addr = PatchData[2];
			Addr += 1;
			Addr += 1;
			Size -= 4;
		}
		else
		{
			Addr += 4;
			Size -= 4;
		}
	}
}

