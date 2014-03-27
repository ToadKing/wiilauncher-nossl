
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogc/es.h>
#include <ogcsys.h>
#include <sys/unistd.h>

#include "identify.h"
#include "certs_dat.h"
#include "gecko.h"
#include "utils.h"
#include "patchcode.h"
#include "videopatch.h"
#include "launch.h"
#include "wifi_gecko.h"
#include "menu.h"
#include "fst.h"
#include "sd.h"

static u8 tmdbuf[MAX_SIGNED_TMD_SIZE] ATTRIBUTE_ALIGN(0x20);
static u8 tikbuf[STD_SIGNED_TIK_SIZE] ATTRIBUTE_ALIGN(0x20);

u8 channelios;
u8 menuios;
u16 bootindex;
u32 bootid;
u32 bootentrypoint;
u8 channelidentified = 0;
u8 menuidentified = 0;
bool hookpatched = false;

s32 Channel_identify(u64 channeltoload)
{
	u64 titleID;
	u32 tmdSize = 0, tikSize = 0;
	signed_blob *ptmd;
	s32 res, ret;
	u32 i;
	char filename[256];
	
	if (!channelidentified)
	{
		if (ISFS_Initialize())
			return 0;
			
		snprintf(filename, sizeof(filename), "/title/%08x/%08x/content/title.tmd", TITLE_UPPER(channeltoload), TITLE_LOWER(channeltoload));
		wifi_printf("identify_Channel_identify: filename value = %s\n", filename);
		ret = ISFS_Open(filename, ISFS_OPEN_READ);
		if(ret < 0){
			ISFS_Deinitialize();
			return 0;
		}
		
		tmdSize = ISFS_Read(ret, (void *) tmdbuf, sizeof(tmdbuf));
		wifi_printf("identify_Channel_identify: tmdSize value = %d\n", tmdSize);
		ISFS_Close(ret);
		
		snprintf(filename, sizeof(filename), "/ticket/%08x/%08x.tik", TITLE_UPPER(channeltoload), TITLE_LOWER(channeltoload));
		wifi_printf("identify_Channel_identify: filename value = %s\n", filename);
		ret = ISFS_Open(filename, ISFS_OPEN_READ);
		if(ret < 0){
			ISFS_Deinitialize();
			return 0;
		}
		
		tikSize = ISFS_Read(ret, (void *) tikbuf, sizeof(tikbuf));
		wifi_printf("identify_Channel_identify: tikSize = %d\n", tikSize);
		ISFS_Close(ret);
		
		ISFS_Deinitialize();
	}
		
	res = ES_Identify((signed_blob*)certs_dat, certs_dat_size, (signed_blob*)tmdbuf, tmdSize, (signed_blob*)tikbuf, tikSize, NULL);
	wifi_printf("identify_Channel_identify: ES_Identify() value = %d\n", res);
	if(res < 0)
		return 0;	
	
	if (!channelidentified)
	{
		//Get current title ID
		ES_GetTitleID(&titleID);
		wifi_printf("identify_Channel_identify: titleID value = %016llX\n", titleID);
		if(titleID != channeltoload){
			return 0;
		}
		
		//Get tmd size
		res = ES_GetStoredTMDSize(titleID, &tmdSize);
		wifi_printf("identify_Channel_identify: ES_GetStoredTMDSize() value = %d\n", res);
		if(res < 0){
			return 0;
		}
		
		//Get tmd
		ptmd = memalign(32, tmdSize);
		res = ES_GetStoredTMD(titleID, ptmd, tmdSize);
		wifi_printf("identify_Channel_identify: ES_GetStoredTMD() value = %d\n", res);
		if(res < 0){
			return 0;
		}
		
		ptmd = (signed_blob*)tmdbuf;
		channelios = ((tmd *)SIGNATURE_PAYLOAD(ptmd))->sys_version & 0xFF;
		bootindex = ((tmd *)SIGNATURE_PAYLOAD(ptmd))->boot_index;
		for (i = 0; i < ((tmd *)SIGNATURE_PAYLOAD(ptmd))->num_contents; i++)
		{
			if (((tmd *)SIGNATURE_PAYLOAD(ptmd))->contents[i].index == bootindex)
				bootid = ((tmd *)SIGNATURE_PAYLOAD(ptmd))->contents[i].cid;
		}
		channelidentified = 1;
	}
	
	return 1;
}

s32 ES_Load_dol(u16 index)
{
	u32 i;
	s32 ret;
	u8 hookpatcheda = 0;
	u8 hookpatchedb = 0;
	
	static dolheader dolfile[1] ATTRIBUTE_ALIGN(32);
	
	ret = ES_OpenContent(index);
	wifi_printf("identify_ES_Load_dol: ES_OpenContent() value = %d\n", ret);
	if(ret < 0){
		return 0;
	}
	
	ES_ReadContent(ret, (void *)dolfile, sizeof(dolheader));
	
	bootentrypoint = dolfile->entry_point;
	
	memset((void *)dolfile->bss_start, 0, dolfile->bss_size);
	
	for (i = 0; i < 7; i++)
	{
		if(dolfile->section_start[i] < sizeof(dolheader))
			continue;
		
		ES_SeekContent(ret, dolfile->section_pos[i], 0);
		ES_ReadContent(ret, (void *)dolfile->section_start[i], dolfile->section_size[i]);
		DCFlushRange((void *)dolfile->section_start[i], dolfile->section_size[i]);
		ICInvalidateRange((void *)dolfile->section_start[i], dolfile->section_size[i]);
		wifi_printf("identify_ES_Load_dol: section [%d] pos=%08X start= %08X size=%08X\n", 
			i+1, (u32)dolfile->section_pos[i], (u32)dolfile->section_start[i],
			(u32)dolfile->section_size[i]);
		patchVideoModes((void *)dolfile->section_start[i], dolfile->section_size[i], vidMode, vmode, patchVidModes);
		if(vipatchselect)
			vidolpatcher((void *)dolfile->section_start[i], dolfile->section_size[i]);
		if(configbytes[0] != 0xCD)
			langpatcher((void *)dolfile->section_start[i], dolfile->section_size[i]);
		if(countryString)
			PatchCountryStrings((void *)dolfile->section_start[i], dolfile->section_size[i]);
		if(aspectRatio != -1)
			PatchAspectRatio((void *)dolfile->section_start[i], dolfile->section_size[i], aspectRatio);
		if(hooktype != 0) 
			hookpatcheda = dogamehooks((void *)dolfile->section_start[i], dolfile->section_size[i], true);
		nossl((void *)dolfile->section_start[i], dolfile->section_size[i]);
		DCFlushRange((void *)dolfile->section_start[i], dolfile->section_size[i]);
	}
	wifi_printf("identify_ES_Load_dol: hookpatcheda value = %d\n", hookpatcheda);
	
	for(i = 7; i < 18; i++)
	{
		if(dolfile->section_start[i] < sizeof(dolheader))
			continue;
		
		ES_SeekContent(ret, dolfile->section_pos[i], 0);
		ES_ReadContent(ret, (void *)dolfile->section_start[i], dolfile->section_size[i]);
		DCFlushRange((void *)dolfile->section_start[i], dolfile->section_size[i]);
		ICInvalidateRange((void *)dolfile->section_start[i], dolfile->section_size[i]);
		wifi_printf("identify_ES_Load_dol: section [%d] pos=%08X start= %08X size=%08X\n", 
			i+1, (u32)dolfile->section_pos[i], (u32)dolfile->section_start[i],
			(u32)dolfile->section_size[i]);
		patchVideoModes((void *)dolfile->section_start[i], dolfile->section_size[i], vidMode, vmode, patchVidModes);
		if(vipatchselect)
			vidolpatcher((void *)dolfile->section_start[i], dolfile->section_size[i]);
		if(configbytes[0] != 0xCD)
			langpatcher((void *)dolfile->section_start[i], dolfile->section_size[i]);
		if(countryString)
			PatchCountryStrings((void *)dolfile->section_start[i], dolfile->section_size[i]);
		if(aspectRatio != -1)
			PatchAspectRatio((void *)dolfile->section_start[i], dolfile->section_size[i], aspectRatio);		
		if(hooktype != 0) 
			hookpatchedb = dogamehooks((void *)dolfile->section_start[i], dolfile->section_size[i], true);		
		DCFlushRange((void *)dolfile->section_start[i], dolfile->section_size[i]);
	}
	wifi_printf("identify_ES_Load_dol: hookpatchedb value = %d\n", hookpatchedb);

	ES_CloseContent(ret);	
	if ( hookpatcheda || hookpatchedb )
		hookpatched = true;
	return 1;
}

s32 Menu_identify()
{
	u64 titleID;
	u32 tmdSize = 0, tikSize = 0;
	signed_blob *ptmd;
	s32 res, ret;
	u32 i;
	char filename[256];
	
	u64 channeltoload = 0x0000000100000002ULL;
	if (!menuidentified)
	{
		if (ISFS_Initialize())
			return 0;
			
		snprintf(filename, sizeof(filename), "/title/%08x/%08x/content/title.tmd", TITLE_UPPER(channeltoload), TITLE_LOWER(channeltoload));
		wifi_printf("identify_Menu_identify: filename value = %s\n", filename);
		ret = ISFS_Open(filename, ISFS_OPEN_READ);
		if(ret < 0){
			ISFS_Deinitialize();
			return 0;
		}
		
		tmdSize = ISFS_Read(ret, (void *) tmdbuf, sizeof(tmdbuf));
		wifi_printf("identify_Menu_identify: tmdSize value = %d\n", tmdSize);
		ISFS_Close(ret);
		
		snprintf(filename, sizeof(filename), "/ticket/%08x/%08x.tik", TITLE_UPPER(channeltoload), TITLE_LOWER(channeltoload));
		wifi_printf("identify_Menu_identify: filename value = %s\n", filename);
		ret = ISFS_Open(filename, ISFS_OPEN_READ);
		if(ret < 0){
			ISFS_Deinitialize();
			return 0;
		}
		
		tikSize = ISFS_Read(ret, (void *) tikbuf, sizeof(tikbuf));
		wifi_printf("identify_Menu_identify: tikSize = %d\n", tikSize);
		ISFS_Close(ret);
		
		ISFS_Deinitialize();
	}
		
	res = ES_Identify((signed_blob*)certs_dat, certs_dat_size, (signed_blob*)tmdbuf, tmdSize, (signed_blob*)tikbuf, tikSize, NULL);
	wifi_printf("identify_Menu_identify: ES_Identify() value = %d\n", res);
	if(res < 0)
		return 0;	
	
	if (!menuidentified)
	{
		//Get current title ID
		ES_GetTitleID(&titleID);
		wifi_printf("identify_Menu_identify: titleID value = %016llX\n", titleID);
		if(titleID != channeltoload){
			return 0;
		}
		
		//Get tmd size
		res = ES_GetStoredTMDSize(titleID, &tmdSize);
		wifi_printf("identify_Menu_identify: ES_GetStoredTMDSize() value = %d\n", res);
		if(res < 0){
			return 0;
		}
		
		//Get tmd
		ptmd = memalign(32, tmdSize);
		res = ES_GetStoredTMD(titleID, ptmd, tmdSize);
		wifi_printf("identify_Menu_identify: ES_GetStoredTMD() value = %d\n", res);
		if(res < 0){
			return 0;
		}
		
		ptmd = (signed_blob*)tmdbuf;
		menuios = ((tmd *)SIGNATURE_PAYLOAD(ptmd))->sys_version & 0xFF;
		bootindex = ((tmd *)SIGNATURE_PAYLOAD(ptmd))->boot_index;
		for (i = 0; i < ((tmd *)SIGNATURE_PAYLOAD(ptmd))->num_contents; i++)
		{
			if (((tmd *)SIGNATURE_PAYLOAD(ptmd))->contents[i].index == bootindex)
				bootid = ((tmd *)SIGNATURE_PAYLOAD(ptmd))->contents[i].cid;
		}
		menuidentified = 1;
		wifi_printf("identify_Menu_identify: menuios value = %d\n", menuios);
	}
	
	return 1;
}

s32 ES_Load_dol_menu(u16 index)
{
	u32 i;
	s32 ret;
	u8 hookpatcheda = 0;
	u8 hookpatchedb = 0;
	
	static dolheader dolfile[1] ATTRIBUTE_ALIGN(32);
	
	ret = ES_OpenContent(index);
	wifi_printf("identify_ES_Load_dol_menu: ES_OpenContent() value = %d\n", ret);
	if(ret < 0){
		return 0;
	}
	
	ES_ReadContent(ret, (void *)dolfile, sizeof(dolheader));
	
	bootentrypoint = dolfile->entry_point;
	
	memset((void *)dolfile->bss_start, 0, dolfile->bss_size);
	
	for (i = 0; i < 7; i++)
	{
		if(dolfile->section_start[i] < sizeof(dolheader))
			continue;
		
		ES_SeekContent(ret, dolfile->section_pos[i], 0);
		ES_ReadContent(ret, (void *)dolfile->section_start[i], dolfile->section_size[i]);		 
		hookpatcheda = domenuhooks((void *)dolfile->section_start[i], dolfile->section_size[i]);
		DCFlushRange((void *)dolfile->section_start[i], dolfile->section_size[i]);
		ICInvalidateRange((void *)dolfile->section_start[i], dolfile->section_size[i]);
		wifi_printf("identify_ES_Load_dol: section [%d] pos=%08X start= %08X size=%08X\n", 
			i+1, (u32)dolfile->section_pos[i], (u32)dolfile->section_start[i],
			(u32)dolfile->section_size[i]);
	}
	
	for(i = 7; i < 18; i++)
	{
		if(dolfile->section_start[i] < sizeof(dolheader))
			continue;
		
		ES_SeekContent(ret, dolfile->section_pos[i], 0);
		ES_ReadContent(ret, (void *)dolfile->section_start[i], dolfile->section_size[i]);
		hookpatchedb = domenuhooks((void *)dolfile->section_start[i], dolfile->section_size[i]);
		DCFlushRange((void *)dolfile->section_start[i], dolfile->section_size[i]);
		ICInvalidateRange((void *)dolfile->section_start[i], dolfile->section_size[i]);
		wifi_printf("identify_ES_Load_dol: section [%d] pos=%08X start= %08X size=%08X\n", 
			i+1, (u32)dolfile->section_pos[i], (u32)dolfile->section_start[i],
			(u32)dolfile->section_size[i]);
	}
	
	ES_CloseContent(ret);
	if ( hookpatcheda || hookpatchedb )
		hookpatched = true;
	return 1;
}

void boot_menu()
{	
	ISFS_Initialize();
	s32 menuidentify = Menu_identify();
	wifi_printf("identify_boot_menu: menuidentify value = %d\n", menuidentify);
	if (menuidentify == 1) {
		s32 loaddol = ES_Load_dol_menu(bootindex);
		wifi_printf("identify_boot_menu: loaddol value = %d\n", loaddol);
		if (loaddol != 1) {
			ISFS_Deinitialize();
			exitme();
		}
	} else {
		ISFS_Deinitialize();
		exitme();
	}
	load_handler();

}

void apply_pokevalues() {
	
	u32 i, *codeaddr, *codeaddr2, *addrfound = NULL;
	
	if (gameconfsize != 0)
	{
		for (i = 0; i < (gameconfsize / 4); i++)
		{
			if (*(gameconf + i) == 0)
			{
				if (((u32 *) (*(gameconf + i + 1))) == NULL ||
					*((u32 *) (*(gameconf + i + 1))) == *(gameconf + i + 2))
				{
					*((u32 *) (*(gameconf + i + 3))) = *(gameconf + i + 4);
					DCFlushRange((void *) *(gameconf + i + 3), 4);
					if (((u32 *) (*(gameconf + i + 1))) == NULL)
						wifi_printf("identify_apply_pokevalues: poke ");
					else
						wifi_printf("identify_apply_pokevalues: pokeifequal ");
					wifi_printf("0x%08X = %08X\n",
						(u32)(*(gameconf + i + 3)), (u32)(*(gameconf + i + 4)));
				}
				i += 4;
			}
			else
			{
				codeaddr = (u32 *)(*(gameconf + i + *(gameconf + i) + 1));
				codeaddr2 = (u32 *)(*(gameconf + i + *(gameconf + i) + 2));
				if (codeaddr == 0 && addrfound != NULL)
					codeaddr = addrfound;
				else if (codeaddr == 0 && codeaddr2 != 0)
					codeaddr = (u32 *) ((((u32) codeaddr2) >> 28) << 28);
				else if (codeaddr == 0 && codeaddr2 == 0)
				{
					i += *(gameconf + i) + 4;
					continue;
				}
				if (codeaddr2 == 0)
					codeaddr2 = codeaddr + *(gameconf + i);
				addrfound = NULL;
				while (codeaddr <= (codeaddr2 - *(gameconf + i)))
				{
					if (memcmp(codeaddr, gameconf + i + 1, (*(gameconf + i)) * 4) == 0)
					{
						*(codeaddr + ((*(gameconf + i + *(gameconf + i) + 3)) / 4)) = *(gameconf + i + *(gameconf + i) + 4);
						if (addrfound == NULL) addrfound = codeaddr;
						DCFlushRange((void *) (codeaddr + ((*(gameconf + i + *(gameconf + i) + 3)) / 4)), 4);
						wifi_printf("identify_apply_pokevalues: searchandpoke 0x%08X = %08X\n",
							(u32)((codeaddr + ((*(gameconf + i + *(gameconf + i) + 3)) / 4))), 
							(u32)(*(gameconf + i + *(gameconf + i) + 4)));
					}
					codeaddr++;
				}
				i += *(gameconf + i) + 4;
			}
		}
	}
}

u32 be32(const u8 *p) {
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

void apply_patch() {

	int i;

	u8 *filebuff = (u8*)sdbuffer;
	u8 no_patches;
	u32 patch_dest;
	u32 patch_size;

	no_patches = filebuff[0];

	filebuff += 1;

	for(i=0;i<no_patches;i++)
	{
		patch_dest = be32(filebuff);
		patch_size = be32(filebuff+4);

		memcpy((u8*)patch_dest, filebuff+8, patch_size);
		DCFlushRange((u8*)patch_dest, patch_size);
		wifi_printf("identify_apply_patch: i = %d, patch_dest = %08X, patch_size = %08X\n",
			i, patch_dest, patch_size);
		filebuff = filebuff + patch_size + 8;
	}
}


