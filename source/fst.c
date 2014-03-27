
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <sys/unistd.h>
#include <ogc/ipc.h>

#include "fst.h"
#include "gecko.h"
#include "memory.h"
#include "patchcode.h"
#include "codehandler.h"
#include "codehandleronly.h"
#include "multidol.h"
#include "wifi_gecko.h"

u8 *codelistend;
void *codelist;
u32 debuggerselect = 0;
u8 *code_buf = NULL;
u32 code_size = 0;

extern const u32 viwiihooks[4];
extern const u32 kpadhooks[4];
extern const u32 joypadhooks[4];
extern const u32 gxdrawhooks[4];
extern const u32 gxflushhooks[4];
extern const u32 ossleepthreadhooks[4];
extern const u32 axnextframehooks[4];

void ocarina_set_codes(void *list, u8 *listend, u8 *cheats, u32 cheatSize)
{
	codelist = list;
	codelistend = listend;
	code_buf = cheats;
	code_size = cheatSize;
	wifi_printf("fst_ocarina_set_codes: code_size value = %08X\n", code_size);
	if(code_size <= 0)
	{
		code_buf = NULL;
		code_size = 0;
		return;
	}
	if (code_size > (u32)codelistend - (u32)codelist)
	{
		wifi_printf("fst_ocarina_set_codes: codes are too long\n");
		code_buf = NULL;
		code_size = 0;
		return;
	}
}

void load_handler()
{
	if(debuggerselect == 0x01)
	{
		wifi_printf("fst_load_handler: debugger selected\n");
		memcpy((void*)0x80001800, codehandler, codehandler_size);
		if(code_size > 0 && code_buf)
		{
			wifi_printf("fst_load_handler: codes found\n");
			memcpy((void*)0x80001CDE, &codelist, 2);
			memcpy((void*)0x80001CE2, ((u8*) &codelist) + 2, 2);
			memcpy((void*)0x80001F5A, &codelist, 2);
			memcpy((void*)0x80001F5E, ((u8*) &codelist) + 2, 2);
		}
		else
			wifi_printf("fst_load_handler: no codes found\n");
		DCFlushRange((void*)0x80001800, codehandler_size);
		ICInvalidateRange((void*)0x80001800, codehandler_size);
	}
	else
	{
		wifi_printf("fst_load_handler: no debugger selected\n");
		memcpy((void*)0x80001800, codehandleronly, codehandleronly_size);
		if(code_size > 0 && code_buf)
		{
			wifi_printf("fst_load_handler: codes found\n");
			memcpy((void*)0x80001906, &codelist, 2);
			memcpy((void*)0x8000190A, ((u8*) &codelist) + 2, 2);
		}
		else
			wifi_printf("fst_load_handler: no codes found\n");
		DCFlushRange((void*)0x80001800, codehandleronly_size);
		ICInvalidateRange((void*)0x80001800, codehandleronly_size);
	}

	// Load multidol handler
	memcpy((void*)0x80001000, multidol, multidol_size);
	DCFlushRange((void*)0x80001000, multidol_size);
	ICInvalidateRange((void*)0x80001000, multidol_size);
	switch(hooktype)
	{
		case 0x01:
			memcpy((void*)0x8000119C,viwiihooks,12);
			memcpy((void*)0x80001198,viwiihooks+3,4);
			break;
		case 0x02:
			memcpy((void*)0x8000119C,kpadhooks,12);
			memcpy((void*)0x80001198,kpadhooks+3,4);
			break;
		case 0x03:
			memcpy((void*)0x8000119C,joypadhooks,12);
			memcpy((void*)0x80001198,joypadhooks+3,4);
			break;
		case 0x04:
			memcpy((void*)0x8000119C,gxdrawhooks,12);
			memcpy((void*)0x80001198,gxdrawhooks+3,4);
			break;
		case 0x05:
			memcpy((void*)0x8000119C,gxflushhooks,12);
			memcpy((void*)0x80001198,gxflushhooks+3,4);
			break;
		case 0x06:
			memcpy((void*)0x8000119C,ossleepthreadhooks,12);
			memcpy((void*)0x80001198,ossleepthreadhooks+3,4);
			break;
		case 0x07:
			memcpy((void*)0x8000119C,axnextframehooks,12);
			memcpy((void*)0x80001198,axnextframehooks+3,4);
			break;
	}
	DCFlushRange((void*)0x80001198,16); 
}

int ocarina_do_code()
{
	if(codelist)
		memset(codelist, 0, (u32)codelistend - (u32)codelist);

	wifi_printf("fst_ocarina_do_code: code_size value = %08X\n", code_size);
	if(code_size > 0 && code_buf)
	{
		memcpy(codelist, code_buf, code_size);
		DCFlushRange(codelist, (u32)codelistend - (u32)codelist);
	}

	return 1;
}


