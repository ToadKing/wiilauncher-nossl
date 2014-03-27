
#include <stdio.h>
#include <ogcsys.h>
#include <string.h>
#include <malloc.h>
#include "apploader.h"
#include "wdvd.h"
#include "patchcode.h"
#include "videopatch.h"
#include "fst.h"
#include "gecko.h"
#include "memory.h"
#include "video_tinyload.h"
#include "wifi_gecko.h"
#include "identify.h"

typedef int   (*app_main)(void **dst, int *size, int *offset);
typedef void  (*app_init)(void (*report)(const char *fmt, ...));
typedef void *(*app_final)();
typedef void  (*app_entry)(void (**init)(void (*report)(const char *fmt, ...)), int (**main)(), void *(**final)());

static u8 *appldr = (u8 *)0x81200000;

#define APPLDR_OFFSET	0x910
#define APPLDR_CODE		0x918

void maindolpatches(void *dst, int len, u8 vidMode, GXRModeObj *vmode, bool vipatch, 
				bool countryString, u8 patchVidModes, int aspectRatio);

static struct
{
	char revision[16];
	void *entry;
	s32 size;
	s32 trailersize;
	s32 padding;
} apploader_hdr ATTRIBUTE_ALIGN(32);

u32 Apploader_Run(u8 vidMode, GXRModeObj *vmode, bool vipatch, bool countryString, u8 patchVidModes, int aspectRatio)
{
	void *dst = NULL;
	int len = 0;
	int offset = 0;
	u32 appldr_len;
	s32 ret;

	app_entry appldr_entry;
	app_init  appldr_init;
	app_main  appldr_main;
	app_final appldr_final;

	ret = WDVD_Read(&apploader_hdr, 0x20, APPLDR_OFFSET);
	wifi_printf("apploader_Apploader_Run: WDVD_Read() return value = %d\n", ret);
	if(ret < 0)
		return 0;

	appldr_len = apploader_hdr.size + apploader_hdr.trailersize;
	wifi_printf("apploader_Apploader_Run: appldr_len value = %08X\n", appldr_len); 

	ret = WDVD_Read(appldr, appldr_len, APPLDR_CODE);
	wifi_printf("apploader_Apploader_Run: WDVD_Read() return value = %d\n", ret);
	if(ret < 0)
		return 0;
	DCFlushRange(appldr, appldr_len);
	ICInvalidateRange(appldr, appldr_len);

	appldr_entry = apploader_hdr.entry;
	appldr_entry(&appldr_init, &appldr_main, &appldr_final);
	appldr_init(gprintf);

	while(appldr_main(&dst, &len, &offset))
	{
		WDVD_Read(dst, len, offset);
		wifi_printf("apploader_Apploader_Run: dst = %08X len = %08X offset = %08X\n",
			(u32)dst, (u32)len, (u32)offset);
		maindolpatches(dst, len, vidMode, vmode, vipatch, countryString, 
						patchVidModes, aspectRatio);
		DCFlushRange(dst, len);
		ICInvalidateRange(dst, len);
		prog(10);
	}
	
	return (u32)appldr_final();
}

void maindolpatches(void *dst, int len, u8 vidMode, GXRModeObj *vmode, bool vipatch, bool countryString, u8 patchVidModes, int aspectRatio)
{
	patchVideoModes(dst, len, vidMode, vmode, patchVidModes);
	if(vipatch)
		vidolpatcher(dst, len);
	if(configbytes[0] != 0xCD)
		langpatcher(dst, len);
	if(countryString)
		PatchCountryStrings(dst, len);
	if(aspectRatio != -1)
		PatchAspectRatio(dst, len, aspectRatio);
	if(hooktype != 0) 
		hookpatched = dogamehooks(dst, len, false);
	nossl(dst, len);
}

