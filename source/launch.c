
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <network.h>
#include <sdcard/wiisd_io.h>

#include "video_tinyload.h"
#include "patchcode.h"
#include "memory.h"
#include "utils.h"
#include "disc.h"
#include "fst.h"
#include "gecko.h"
#include "identify.h"
#include "iospatch.h"
#include "menu.h"
#include "sd.h"
#include "wifi_gecko.h"
#include "wdvd.h"
#include "apploader.h"
#include "main.h"

u8 geckoattached = 0;
int wifigecko = -1;
u8 loaderhbc = 0;
u8 identifysu = 0;
u32 GameIOS = 0;
u32 vmode_reg = 0;
GXRModeObj *vmode = NULL;
u32 AppEntrypoint = 0;
char gameidbuffer[8];
u8 vidMode = 0;
int patchVidModes = 0x0;
u8 vipatchselect = 0;
u8 countryString = 0;
int aspectRatio = -1;

extern void __exception_closeall();

static void power_cb() {
	
	STM_ShutdownToStandby();
}

static void reset_cb() {
	
	STM_RebootSystem();
}

s32 rungamechannel(u64 title) {
		
	VIDEO_Init();
	video_init();
	
	configbytes[0] = config_bytes[0];
	wifi_printf("launch_rungamechannel: configbytes[0] value = %d\n", configbytes[0]);
	configbytes[1] = config_bytes[1];
	wifi_printf("launch_rungamechannel: configbytes[1] value = %d\n", configbytes[1]);
	hooktype = config_bytes[2];
	wifi_printf("launch_rungamechannel: hooktype value = %d\n", hooktype);
	debuggerselect = config_bytes[7];
	wifi_printf("launch_rungamechannel: debuggerselect value = %d\n", debuggerselect);
	u8 codesselect = config_bytes[4];
	wifi_printf("launch_rungamechannel: codesselect value = %d\n", codesselect);
	countryString = config_bytes[13];
	wifi_printf("launch_rungamechannel: countryString value = %d\n", countryString);
	if(codesselect)
		ocarina_set_codes(codelist, codelistend, tempcodelist, codelistsize);	
	switch(config_bytes[1]) {
		case 0:
			vidMode = 0;
		break;
		case 1:
			vidMode = 3;
		break;
		case 2:
			vidMode = 2;
		break;
		case 3:
			vidMode = 4;
		break;
	}
	wifi_printf("launch_rungamechannel: vidMode value = %d\n", vidMode);
	switch(config_bytes[12]) {	
		case 0:			
		break;
		case 1:
			vipatchselect = 1;
		break;
		case 2:
			patchVidModes = 0;
		break;
		case 3:
			patchVidModes = 1;
		break;
		case 4:
			patchVidModes = 2;
		break;
	}
	wifi_printf("launch_rungamechannel: vipatchselect value = %d\n", vipatchselect);
	wifi_printf("launch_rungamechannel: patchVidModes value = %d\n", patchVidModes);
	if(config_bytes[14] > 0)
		aspectRatio = (int)config_bytes[14] - 1;
	wifi_printf("launch_rungamechannel: aspectRatio value = %d\n", aspectRatio);
	setprog(40);

	Disc_SetLowMemPre();

	ISFS_Initialize();
	*Disc_ID = TITLE_LOWER(title);
	vmode = Disc_SelectVMode(vidMode, &vmode_reg);
	s32 channelidentify = Channel_identify(title);
	setprog(80);
	wifi_printf("launch_rungamechannel: channelidentify value = %d\n", channelidentify);
	if (channelidentify == 1) {
		s32 loaddol = ES_Load_dol(bootindex);
		setprog(200);
		wifi_printf("launch_rungamechannel: loaddol value = %d\n", loaddol);
		if (loaddol != 1) {
			ISFS_Deinitialize();
			exitme();
		}
	} else {
		ISFS_Deinitialize();
		exitme();
	}
	load_handler();
	if(config_bytes[5] == 1) {
		*(u32*)(*(u32*)0x80001808) = 0x1;
		DCFlushRange((void*)(*(u32*)(*(u32*)0x80001808)), 4);
	}
	setprog(280);
	if(hooktype != 0 && hookpatched) 
		ocarina_do_code();
		
	if (config_bytes[2] != 0x00) {
		apply_pokevalues();
		sleep(1);
		if(config_bytes[3] == 0x01 && patch_state == 1)
			apply_patch();
	}	
		
	setprog(300);
	AppEntrypoint = bootentrypoint;
	wifi_printf("launch_rungamechannel: AppEntrypoint value = %08x\n", AppEntrypoint);
	GameIOS = channelios;
	wifi_printf("launch_rungamechannel: GameIOS value: %i\n", GameIOS);
	sleep(1);
	ISFS_Deinitialize();
	
	usb_flush(EXI_CHANNEL_1);
	WifiGecko_Close();
	setprog(320);
	sleep(1);
	
	Disc_SetLowMem(GameIOS);
	Disc_SetTime();
	Disc_SetVMode(vmode, vmode_reg);

	u32 level = IRQ_Disable();
	__IOS_ShutdownSubsystems();
	__exception_closeall();

	 *(vu32*)0xCC003024 = 1;

 	if(AppEntrypoint == 0x3400)
	{
 		if(hooktype)
 		{
			asm volatile (
				"lis %r3, returnpoint@h\n"
				"ori %r3, %r3, returnpoint@l\n"
				"mtlr %r3\n"
				"lis %r3, 0x8000\n"
				"ori %r3, %r3, 0x18A8\n"
				"nop\n"
				"mtctr %r3\n"
				"bctr\n"
				"returnpoint:\n"
				"bl DCDisable\n"
				"bl ICDisable\n"
				"li %r3, 0\n"
				"mtsrr1 %r3\n"
				"lis %r4, AppEntrypoint@h\n"
				"ori %r4,%r4,AppEntrypoint@l\n"
				"lwz %r4, 0(%r4)\n"
				"mtsrr0 %r4\n"
				"rfi\n"
			);			
 		}
 		else
 		{
 			asm volatile (
 				"isync\n"
				"lis %r3, AppEntrypoint@h\n"
				"ori %r3, %r3, AppEntrypoint@l\n"
 				"lwz %r3, 0(%r3)\n"
 				"mtsrr0 %r3\n"
 				"mfmsr %r3\n"
 				"li %r4, 0x30\n"
 				"andc %r3, %r3, %r4\n"
 				"mtsrr1 %r3\n"
 				"rfi\n"
 			);
 		}
	}
 	else if(hooktype)
	{
		asm volatile (
			"lis %r3, AppEntrypoint@h\n"
			"ori %r3, %r3, AppEntrypoint@l\n"
			"lwz %r3, 0(%r3)\n"
			"mtlr %r3\n"
			"lis %r3, 0x8000\n"
			"ori %r3, %r3, 0x18A8\n"
			"nop\n"
			"mtctr %r3\n"
			"bctr\n"
		);
	}
	else
	{
		asm volatile (
			"lis %r3, AppEntrypoint@h\n"
			"ori %r3, %r3, AppEntrypoint@l\n"
			"lwz %r3, 0(%r3)\n"
			"mtlr %r3\n"
			"blr\n"
		);
	} 
	IRQ_Restore(level);
	
	return 1;
}

s32 runchannel(u64 channeltoload) {

	printf("\x1b[2J");
	VIDEO_WaitVSync();
	wifi_printf("launch_runchannel: channeltoload value = %016llX\n", channeltoload);
	memset(gameidbuffer, 0, 8);
	gameidbuffer[0] = (channeltoload & 0xff000000) >> 24;
	gameidbuffer[1] = (channeltoload & 0x00ff0000) >> 16;
	gameidbuffer[2] = (channeltoload & 0x0000ff00) >> 8;
	gameidbuffer[3] = channeltoload & 0x000000ff;
	gameidbuffer[4] = 0;
	wifi_printf("launch_runchannel: gameidbuffer value = %s\n", gameidbuffer);
	if (config_bytes[7] == 0x00)
		codelist = (u8 *) 0x800022A8;
	else
		codelist = (u8 *) 0x800028B8;
	codelistend = (u8 *) 0x80003000;
	
	if (config_bytes[2] != 0x00) {
		if(config_bytes[3] == 0x01)
			sd_copy_patch(gameidbuffer);		
		sd_copy_gameconfig(gameidbuffer);	
		if(config_bytes[4] == 0x01)
			sd_copy_codes(gameidbuffer);
	}
	
	menu_drawbootchannel();
	WPAD_Shutdown();
	if ((TITLE_LOWER(channeltoload) == 0x48414341) // allow Mii Channel
		||
		(TITLE_LOWER(channeltoload) == 0x48415941) // allow Photo Channel
		||
		(gameidbuffer[0] != 0x48 &&
		((TITLE_UPPER(channeltoload) == 0x00010001 &&
			(gameidbuffer[0] == 0x43 || gameidbuffer[0] == 0x45 ||
			gameidbuffer[0] == 0x46 || gameidbuffer[0] == 0x4A ||
			gameidbuffer[0] == 0x4C || gameidbuffer[0] == 0x4D ||
			gameidbuffer[0] == 0x4E || gameidbuffer[0] == 0x50 ||
			gameidbuffer[0] == 0x51 || gameidbuffer[0] == 0x57 ||
			gameidbuffer[0] == 0x58 || gameidbuffer[0] == 0x57 ||
			gameidbuffer[0] == 0x51 || gameidbuffer[0] == 0x57 
			)
		) ||
		(TITLE_UPPER(channeltoload) == 0x00010004 &&
			(gameidbuffer[0] == 0x52)
		) ||
		(TITLE_UPPER(channeltoload) == 0x00010005))
		)) {
		printf("\x1b[2J");
		VIDEO_WaitVSync();
		wifi_printf("launch_runchannel: passing to launch_rungamechannel\n");
		rungamechannel(channeltoload);
	}
	else {
		printf("\n    Cannot use the debugger, cheat codes, or patches");
		sleep(1);
		wifi_printf("launch_runchannel: passing to WII_LaunchTitle\n");
		usb_flush(EXI_CHANNEL_1);
		WifiGecko_Close();
		WII_Initialize();
		WII_LaunchTitle(channeltoload);
	}
		
	return 1;
}

s32 runmenu() {

	WPAD_Shutdown();
	printf("\x1b[2J");
	VIDEO_WaitVSync();		
	hooktype = config_bytes[2];
	wifi_printf("launch_runmenu: hooktype value = %d\n", hooktype);
	debuggerselect = config_bytes[7];
	wifi_printf("launch_runmenu: debuggerselect value = %d\n", debuggerselect);
	u8 codesselect = config_bytes[4];
	wifi_printf("launch_runmenu: codesselect value = %d\n", codesselect);	
	if (config_bytes[7] == 0x00)
		codelist = (u8 *) 0x800022A8;
	else
		codelist = (u8 *) 0x800028B8;
	codelistend = (u8 *) 0x80003000;

	boot_menu();

	AppEntrypoint = bootentrypoint;
	wifi_printf("launch_runmenu: AppEntrypoint value = %08x\n", AppEntrypoint);

	__io_wiisd.shutdown();
	usb_flush(EXI_CHANNEL_1);
	WifiGecko_Close();
	SYS_ResetSystem(SYS_SHUTDOWN,0,0);
	
	if (hooktype == 0)
	{
		__asm__(
			"bl DCDisable\n"
			"bl ICDisable\n"
			"li %r3, 0\n"
			"mtsrr1 %r3\n"
			"lis %r4, 0x0000\n"
			"ori %r4,%r4,0x3400\n"
			"mtsrr0 %r4\n"
			"rfi\n"
		);
	}
	else
	{
		__asm__(
			"lis %r3, returnpointmenu@h\n"
			"ori %r3, %r3, returnpointmenu@l\n"
			"mtlr %r3\n"
			"lis %r3, 0x8000\n"
			"ori %r3, %r3, 0x18A8\n"
			"mtctr %r3\n"
			"bctr\n"
			"returnpointmenu:\n"
			"bl DCDisable\n"
			"bl ICDisable\n"
			"li %r3, 0\n"
			"mtsrr1 %r3\n"
			"lis %r4, 0x0000\n"
			"ori %r4,%r4,0x3400\n"
			"mtsrr0 %r4\n"
			"rfi\n"
		);
	}
	
	return 1;
}

s32 rundisc() {

	Disc_SetLowMemPre();
	WDVD_Init();
	if(Disc_Open() != 0) {
		rundiscnow = 0;
		menu_number = 0;
		printf("\n    Cannot open disc");
		sleep(2);
		return 0;
	}
	
	if(disc_type == IS_UNK_DISC) {
		rundiscnow = 0;
		menu_number = 0;
		printf("\n    Unknown disc");
		sleep(2);
		return 0;
	}
	if(disc_type == IS_NGC_DISC) {
		printf("\n    Launch GameCube disc");
		printf("\n    Cannot use cheat codes and other features");		
		sleep(2);
		
		WII_LaunchTitle(0x100000100LL);
		
		return 0;
	}
	memset(gameidbuffer, 0, 8);
	memcpy(gameidbuffer, (char*)0x80000000, 6);
	wifi_printf("launch_rundisc: gameidbuffer value = %s\n", gameidbuffer);
	if (config_bytes[7] == 0x00)
		codelist = (u8 *) 0x800022A8;
	else
		codelist = (u8 *) 0x800028B8;
	codelistend = (u8 *) 0x80003000;
	
	if (config_bytes[2] != 0x00) {
		if(config_bytes[3] == 0x01)
			sd_copy_patch(gameidbuffer);		
		sd_copy_gameconfig(gameidbuffer);	
		if(config_bytes[4] == 0x01)
			sd_copy_codes(gameidbuffer);
	}
		
	menu_drawbootdisc();
	WPAD_Shutdown();
	printf("\x1b[2J");
	VIDEO_WaitVSync();

	VIDEO_Init();
	video_init();
	
	configbytes[0] = config_bytes[0];
	wifi_printf("launch_rundisc: configbytes[0] value = %d\n", configbytes[0]);
	configbytes[1] = config_bytes[1];
	wifi_printf("launch_rundisc: configbytes[1] value = %d\n", configbytes[1]);
	hooktype = config_bytes[2];
	wifi_printf("launch_rundisc: hooktype value = %d\n", hooktype);
	debuggerselect = config_bytes[7];
	wifi_printf("launch_rundisc: debuggerselect value = %d\n", debuggerselect);
	u8 codesselect = config_bytes[4];
	wifi_printf("launch_rundisc: codesselect value = %d\n", codesselect);
	if(codesselect)
		ocarina_set_codes(codelist, codelistend, tempcodelist, codelistsize);
	countryString = config_bytes[13];
	wifi_printf("launch_rundisc: countryString value = %d\n", countryString);	
	switch(config_bytes[1]) {
		case 0:
			vidMode = 0;
		break;
		case 1:
			vidMode = 3;
		break;
		case 2:
			vidMode = 2;
		break;
		case 3:
			vidMode = 4;
		break;
	}
	wifi_printf("launch_rundisc: vidMode value = %d\n", vidMode);
	switch(config_bytes[12]) {	
		case 0:			
		break;
		case 1:
			vipatchselect = 1;
		break;
		case 2:
			patchVidModes = 0;
		break;
		case 3:
			patchVidModes = 1;
		break;
		case 4:
			patchVidModes = 2;
		break;
	}
	wifi_printf("launch_rundisc: vipatchselect value = %d\n", vipatchselect);
	wifi_printf("launch_rundisc: patchVidModes value = %d\n", patchVidModes);
	if(config_bytes[14] > 0)
		aspectRatio = (int)config_bytes[14] - 1;
	wifi_printf("launch_rundisc: aspectRatio value = %d\n", aspectRatio);
	setprog(40);
	
	u32 offset = 0;
	Disc_FindPartition(&offset);
	WDVD_OpenPartition(offset, &GameIOS);
	wifi_printf("launch_rundisc: GameIOS value: %i\n", GameIOS);
	vmode = Disc_SelectVMode(vidMode, &vmode_reg);
	setprog(60);
	AppEntrypoint = Apploader_Run(vidMode, vmode, vipatchselect, countryString,
					patchVidModes, aspectRatio);
	load_handler();
	if(config_bytes[5] == 1) {
		*(u32*)(*(u32*)0x80001808) = 0x1;
		DCFlushRange((void*)(*(u32*)(*(u32*)0x80001808)), 4);
	}
	if(hooktype != 0 && hookpatched) 
		ocarina_do_code();
	
	if (config_bytes[2] != 0x00) {
		apply_pokevalues();
		sleep(1);
		if(config_bytes[3] == 0x01 && patch_state == 1)
			apply_patch();
	}
	
	wifi_printf("launch_rundisc: AppEntrypoint value = %08X\n", AppEntrypoint);
	sleep(1);
	setprog(300);
	WDVD_Close();
	usb_flush(EXI_CHANNEL_1);
	WifiGecko_Close();
	setprog(320);
	sleep(1);

	Disc_SetLowMem(GameIOS);
	Disc_SetTime();
	Disc_SetVMode(vmode, vmode_reg);

	u32 level = IRQ_Disable();
	__IOS_ShutdownSubsystems();
	__exception_closeall();

	*(vu32*)0xCC003024 = 1;

	// for online
	*(u32*)0x80003180 = *(u32*)0x80000000;

	DCFlushRange((void*)0x80000000, 0x3F00);

 	if(AppEntrypoint == 0x3400)
	{
 		if(hooktype)
 		{
			asm volatile (
				"lis %r3, returnpointdisc@h\n"
				"ori %r3, %r3, returnpointdisc@l\n"
				"mtlr %r3\n"
				"lis %r3, 0x8000\n"
				"ori %r3, %r3, 0x18A8\n"
				"nop\n"
				"mtctr %r3\n"
				"bctr\n"
				"returnpointdisc:\n"
				"bl DCDisable\n"
				"bl ICDisable\n"
				"li %r3, 0\n"
				"mtsrr1 %r3\n"
				"lis %r4, AppEntrypoint@h\n"
				"ori %r4,%r4,AppEntrypoint@l\n"
				"lwz %r4, 0(%r4)\n"
				"mtsrr0 %r4\n"
				"rfi\n"
			);
 		}
 		else
 		{
 			asm volatile (
 				"isync\n"
				"lis %r3, AppEntrypoint@h\n"
				"ori %r3, %r3, AppEntrypoint@l\n"
 				"lwz %r3, 0(%r3)\n"
 				"mtsrr0 %r3\n"
 				"mfmsr %r3\n"
 				"li %r4, 0x30\n"
 				"andc %r3, %r3, %r4\n"
 				"mtsrr1 %r3\n"
 				"rfi\n"
 			);
 		}
	}
 	else if(hooktype)
	{
		asm volatile (
			"lis %r3, AppEntrypoint@h\n"
			"ori %r3, %r3, AppEntrypoint@l\n"
			"lwz %r3, 0(%r3)\n"
			"mtlr %r3\n"
			"lis %r3, 0x8000\n"
			"ori %r3, %r3, 0x18A8\n"
			"nop\n"
			"mtctr %r3\n"
			"bctr\n"
		);
	}
	else
	{
		asm volatile (
			"lis %r3, AppEntrypoint@h\n"
			"ori %r3, %r3, AppEntrypoint@l\n"
			"lwz %r3, 0(%r3)\n"
			"mtlr %r3\n"
			"blr\n"
		);
	}
	IRQ_Restore(level);

	return 1;
}

void prepare() {
	
	if (strncmp(ipaddress,DESTINATION_IP, 15) != 0) {
		wifigecko = WifiGecko_Connect();
		wifi_printf("launch_prepare: wifigecko value = %d\n", wifigecko); 
	}
	sleep(1);
	geckoattached = InitGecko();
	wifi_printf("launch_prepare: geckoattached value = %d\n", geckoattached);
	sleep(1);
	
	sys_init();
	resetscreen();
	
	if (*((u32 *) 0x80001804) == 0x53545542 && 
		(
		*((u32 *) 0x80001808) == 0x48415858 ||
		*((u32 *) 0x80001808) == 0x4A4F4449 ||
		*((u32 *) 0x80001808) == 0xAF1BF516
		)
		)
		loaderhbc = 1;
	wifi_printf("launch_prepare: loaderhbc value = %d\n", loaderhbc);
	
	printf("\n    Load IOS 36 and keep AHBPROT...");
	WifiGecko_Close();
	s32 retreload = ReloadAppIos(36);
	WifiGecko_Connect();
	wifi_printf("launch_prepare: retreload value = %d\n", retreload);
	resetscreen();
	printf("\n    Load IOS 36 and keep AHBPROT...");
	if(retreload) {
		printf(" Done");
	} else {
		printf(" Failed");
	}
	printf("\n    Patch IOS 36:");
	u32 retiospatch = IOSPATCH_Apply();
	wifi_printf("launch_prepare: retiospatch value = %d\n", retiospatch);
	if (retiospatch == 2)
		identifysu = 1;
	if (identifysu) {
		printf("\n    All assigned patches were implemented");
	} else {
		printf("\n    Missing one or more assigned patches");
	}
	
	sleep(2);
	printf("\x1b[2J");

	PAD_Init();
	WPAD_Init();
	WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
	
	AUDIO_Init (NULL);
	
	SYS_SetPowerCallback (power_cb);
    SYS_SetResetCallback (reset_cb);
	
	sd_refresh();
	sd_init();
	sd_load_config();
	wifi_printf("launch_prepare: sd_found value = %d\n", sd_found);
}
