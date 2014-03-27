
#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <ogc/machine/processor.h>
#include <string.h>
#include "iospatch.h"
#include "gecko.h"
#include "wifi_gecko.h"

#define MEM_REG_BASE 0xd8b4000
#define MEM_PROT (MEM_REG_BASE + 0x20a)
#define CheckAHBPROT()  (read32(0x0D800064) == 0xFFFFFFFF)
#define MEM2_PROT		0x0D8B420A
#define ES_MODULE_START	((u16 *)0x939F0000)
#define ES_MODULE_END	(ES_MODULE_START + 0x4000)
#define ES_HACK_OFFSET	4

static const u8 isfs_permissions_old[] = { 0x42, 0x8B, 0xD0, 0x01, 0x25, 0x66 };
static const u8 isfs_permissions_patch[] = { 0x42, 0x8B, 0xE0, 0x01, 0x25, 0x66 };
static const u8 es_identify_old[] = { 0x28, 0x03, 0xD1, 0x23 };
static const u8 es_identify_patch[] = { 0x00, 0x00 };

static void disable_memory_protection() {
    
	write32(MEM_PROT, read32(MEM_PROT) & 0x0000FFFF);
}

static void applyingpatch(const char* which) {
	
	printf("\n    Applying patch: %s...", which);
}

static u32 printresult(u32 successful) {
	
	if(successful) {
		printf(" Done");
	} else {
		printf(" Failed");
	}
	
	return successful;
}

static u32 apply_patch(char *name, const u8 *old, u32 old_size, const u8 *patch, u32 patch_size, u32 patch_offset) {   
	
	applyingpatch(name);
	u8 *ptr_start = (u8*)*((u32*)0x80003134), *ptr_end = (u8*)0x94000000;
    u32 found = 0;
    u8 *location = NULL;
    while (ptr_start < (ptr_end - patch_size)) {
        if (!memcmp(ptr_start, old, old_size)) {
            found++;
            location = ptr_start + patch_offset;
            u8 *start = location;
            u32 i;
            for (i = 0; i < patch_size; i++) {
                *location++ = patch[i];
            }
            DCFlushRange((u8 *)(((u32)start) >> 5 << 5), (patch_size >> 5 << 5) + 64);
			ICInvalidateRange((u8 *)(((u32)start) >> 5 << 5), (patch_size >> 5 << 5) + 64);
        }
        ptr_start++;
    }
    
	return found;
}

u32 IOSPATCH_Apply() {
    
	u32 count = 0;
    if (CheckAHBPROT()) {
        disable_memory_protection();
		count += printresult(apply_patch("ISFS_Permissions", isfs_permissions_old, sizeof(isfs_permissions_old), isfs_permissions_patch, sizeof(isfs_permissions_patch), 0));
		count += printresult(apply_patch("ES_Identify", es_identify_old, sizeof(es_identify_old), es_identify_patch, sizeof(es_identify_patch), 2));
    }
    
	return count;
}

s32 ReloadIosKeepingRights(s32 ios) {
	
	s32 found = 0;
	if (CheckAHBPROT()) {
		static const u16 ticket_check[] = {
			0x685B,			
			0x22EC, 0x0052,	
			0x189B,			
			0x681B,			
			0x4698,			
			0x07DB			
		};

		write16(MEM2_PROT, 2);
		
		for (u16 *patchme = ES_MODULE_START; patchme < ES_MODULE_END; patchme++)
		{	
			if (!memcmp(patchme, ticket_check, sizeof(ticket_check))) {
				patchme[ES_HACK_OFFSET] = 0x23FF;
				DCFlushRange(patchme+ES_HACK_OFFSET, 2);
				found += 1;
				break;
			}
		}
	}

	s32 result = IOS_ReloadIOS(ios);
	if (CheckAHBPROT()) {
		wifi_printf("iospatch_ReloadIosKeepingRights: new IOS %d with AHBPROT = %d\n", ios, result);
	} 
	else {
		wifi_printf("iospatch_ReloadIosKeepingRights: new IOS %d without AHBPROT = %d\n", ios, result);	
	}
	
	return found;
}

s32 ReloadAppIos(s32 ios)
{
	if(ios == IOS_GetVersion())
		return 0;

	s32 result = ReloadIosKeepingRights(ios);

	return result;
}
