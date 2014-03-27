
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <gccore.h>
#include <fat.h>
#include <ogcsys.h>
#include <sys/unistd.h>
#include <sys/dir.h>
#include <wiiuse/wpad.h>
#include <sdcard/wiisd_io.h>

#include "utils.h"
#include "identify.h"
#include "menu.h"
#include "gecko.h"
#include "launch.h"
#include "fst.h"
#include "sd.h"
#include "wifi_gecko.h"
#include "disc.h"
#include "wdvd.h"
#include "gc_dvd.h"
#include "download.h"

GXRModeObj *rmode = NULL;
u32 *xfb;

void sys_init(void)
{
	VIDEO_Init();
	
	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	
	VIDEO_SetBlack(FALSE);

	VIDEO_Flush();

	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

    int x = 24, y = 32, w, h;
    w = rmode->fbWidth - (32);
    h = rmode->xfbHeight - (48);
	CON_InitEx(rmode, x, y, w, h);

    VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
}

void resetscreen()
{
	printf("\x1b[2J");
	printf("\n    WiiLauncher /w NoSSL v0.1 (c) 2013 conanac + Toad King");
	printf("\n    Thanks to developers of: geckoos, wiiflow, tinyload, ftpii, wiixplorer,");
	printf("\n    cleanrip. And others whose codes are reused in this wiibrew");
	printf("\n    USB Gecko = [%s]      WiFi Debug = [%s]", geckoattached ? "ON" : "OFF",
			(wifigecko != -1) ? "ON" : "OFF");     
	printf("\n\n");
}

void resetscreeneditheader(char *name, char *gameidbuffer, char flag)
{
	printf("\x1b[2J");
	printf("\n    WiiLauncher v0.1 (c) 2013 conanac");
	printf("\n    USB Gecko = [%s]      WiFi Debug = [%s]", geckoattached ? "ON" : "OFF",
			(wifigecko != -1) ? "ON" : "OFF");     
	printf("\n\n");
	printf("\n    Game ID: %s", gameidbuffer);
	printf("\n    [%c] %s", flag, name);
}

void resetscreeneditfooter(char *note)
{
	printf("\n\n%s", note);
}

// Root Menu
#define root_menu_items 7
int root_menu_selected = 0;
typedef struct{char *ritems; int selected;} rootitems;
rootitems rootmenu[]=
	{
	{"Launch Game",0},
	{"Launch Channel",0},
	{"Launch Rebooter",0},	
	{"Config Options",0},
	{"Rebooter Options",0},
	{"Download Cheat Codes",0},
	{"Exit",0},
	};

// Config Menu
#define config_menu_items 13
int config_menu_selected = 0;
typedef struct{char *citems; int selected;} configitems;
configitems configmenu[]=
	{
	{"Game Language:",0},
	{"Force NTSC:",0},
	{"Force PAL60:",0},
	{"Force PAL50:",0},
	{"Gecko Hook Type:",0},
	{"Load Debugger:",0},
	{"SD File Patcher:",0},
	{"SD Cheats:",0},
	{"Gecko Pause Start:",0},
	{"Video Modes Patch:",0},
	{"Country String Patch:",0},
	{"Aspect Ratio:",0},
	{"Save Config",0}
	};
	
// Rebooter Config Menu
#define rebooterconf_menu_items 5
int rebooterconf_menu_selected = 0;
typedef struct{char *rcitems; int selected;} rebooterconfitems;
rebooterconfitems rebooterconfmenu[]=
	{
	{"Recovery Mode:",0},
	{"Region Free:",0},
	{"Remove Copy Flags:",0},
	{"Button Skip:",0},
	{"Save Config",0}
	};

// Channel List Menu
u16 channellist_menu_items = 0;
int channellist_menu_selected = 0;
int channellist_menu_top = 0;
typedef struct{char *clitems; int selected; u64 titleid;} channellistitems;
channellistitems channellistmenu[256];

// Download List Menu
u16 downloadlist_menu_items = 0;
int downloadlist_menu_selected = 0;
int downloadlist_menu_top = 0;
typedef struct{char *dlitems; int selected; char gameidbuffer[8];} downloadlistitems;
downloadlistitems downloadlistmenu[257];

// Code List Menu
u16 codelist_menu_items = 0;
int codelist_menu_selected = 0;
int codelist_menu_top = 0;
typedef struct{u16 id; char flag; char *citems; int selected; } codelistitems;
codelistitems codelistmenu[64000];

// Code Line Menu
u16 codeline_menu_items = 0;
int codeline_menu_selected = 0;
int codeline_menu_top = 0;
typedef struct{u16 id; int col; char litems[0x12]; int selected; } codelineitems;
codelineitems codelinemenu[512];
int codeline_col = 0;
char codeline_colchar = '0';
char codelineflag = '-';

u32 langselect = 0;
u32 ntscselect = 0;
u32 pal60select = 0;
u32 pal50select = 0;
u32 hooktypeselect = 1;
u32 filepatcherselect = 0;
u32 ocarinaselect = 0;
u32 pausedstartselect = 0;
u32 geckoslotselect = 0;
u32 recoveryselect = 0;
u32 regionfreeselect = 0;
u32 nocopyselect = 0;
u32 buttonskipselect = 0;
u32 videomodespatchselect = 0;
u32 countrystringselect = 0;
u32 aspectratioselect = 0; // 0 = 4:3; 1 = 16:9
u8 config_bytes[no_config_bytes] ATTRIBUTE_ALIGN(32);
u64 channeltoload = 0x0;
char downloadtoload[8] = "";
u32 error_video = 0;
u32 menu_number = 0;
u32 error_sd = 0;
u32 error_geckopaused = 0;
u32 error_video_patch = 0;
u32 confirm_sd = 0;
int config_not_loaded = 0;
int runmenunow = 0;
int rundiscnow = 0;
int rundownloadnow = 0;
int runeditcodenow = 0;

static int dumpCounter = 0;
static char gameName[32];
static char internalName[512];
bool disc = true;
char namecodeline[512];
char notecodeline[512];

char languages[11][22] = 
	{
	{"Default"},
	{"Japanese"},
	{"English"},
	{"German"},
	{"French"},
	{"Spanish"},
	{"Italian"},
	{"Dutch"},
	{"S. Chinese"},
	{"T. Chinese"},
	{"Korean"}
	};

char ntsc_options[2][6] = 
	{{"NO"},
	{"YES"}};

char pal60_options[2][6] = 
	{{"NO"},
	{"YES"}};

char pal50_options[2][6] = 
	{{"NO"},
	{"YES"}};

char hooktype_options[8][14] = 
	{
	{"No Hooks"},
	{"VBI"},
	{"Wii Pad"},
	{"GC Pad"},
	{"GXDraw"},
	{"GXFlush"},
	{"OSSleepThread"},
	{"AXNextFrame"}
	};

char debugger_options[2][6] = 
	{{"NO"},
	{"YES"}};

char filepatcher_options[2][6] = 
	{{"NO"},
	{"YES"}};

char ocarina_options[2][6] = 
	{{"NO"},
	{"YES"}};

char pausedstart_options[2][6] = 
	{{"NO"},
	{"YES"}};
	
char videomodespatch[5][14] = 
	{
	{"No Patch"},
	{"Auto"},
	{"Normal"},
	{"More"},
	{"All"}
	};
	
char countrystring_options[2][6] = 
	{{"NO"},
	{"YES"}};
	
char aspectratiopatch[3][14] = 
	{{"No Patch"},
	{"4:3"},
	{"16:9"}};
	
char recovery_options[2][6] = 
	{{"NO"},
	{"YES"}};

char regionfree_options[2][6] = 
	{{"NO"},
	{"YES"}};

char nocopy_options[2][6] = 
	{{"NO"},
	{"YES"}};

char buttonskip_options[2][6] = 
	{{"NO"},
	{"YES"}};

static u32 menu_process_config_flags() {
	
	if((pal60select == 1 && ntscselect == 1) || 
		(pal50select == 1 && ntscselect == 1) ||
		(pal50select == 1 && pal60select == 1)){
		error_video = 1;
		return 0;
	}
	
	if((pal60select == 1 || pal50select == 1 || ntscselect == 1) &&
	   (videomodespatchselect != 0)) {
	   error_video_patch = 1;
	   return 0;
	}
	
	if(pausedstartselect == 1 && debuggerselect == 0) {
		error_geckopaused = 1;
		return 0;
	}
	
	switch(langselect)
	{
		case 0:
			config_bytes[0] = 0xCD;
		break;

		case 1:
			config_bytes[0] = 0x00;
		break;

		case 2:
			config_bytes[0] = 0x01;
		break;

		case 3:
			config_bytes[0] = 0x02;
		break;

		case 4:
			config_bytes[0] = 0x03;
		break;

		case 5:
			config_bytes[0] = 0x04;
		break;

		case 6:
			config_bytes[0] = 0x05;
		break;

		case 7:
			config_bytes[0] = 0x06;
		break;

		case 8:
			config_bytes[0] = 0x07;
		break;

		case 9:
			config_bytes[0] = 0x08;
		break;
		
		case 10:
			config_bytes[0] = 0x09;
		break;
	}
	
	// Video Modes
	if(pal60select == 0 && ntscselect == 0 && pal50select == 0){ // config[0] 0x00 (apply no patches)
		config_bytes[1] = 0x00;
	}

	if(pal60select == 1){			// 0x01 (Force PAL60)
		config_bytes[1] = 0x01;
	}

	if(pal50select == 1){			// 0x02 (Force PAL50)
		config_bytes[1] = 0x02;
	}

	if(ntscselect == 1){			// 0x02 (Force NTSC)
		config_bytes[1] = 0x03;
	}
	
	// Hook Type
	switch(hooktypeselect)
	{
		case 0:
			config_bytes[2] = 0x00;	// No Hooks
		break;
		
		case 1:
			config_bytes[2] = 0x01;	// VBI
		break;

		case 2:
			config_bytes[2] = 0x02;	// KPAD Read
		break;

		case 3:
			config_bytes[2] = 0x03;	// Joypad Hook
		break;

		case 4:
			config_bytes[2] = 0x04;	// GXDraw Hook
		break;

		case 5:
			config_bytes[2] = 0x05;	// GXFlush Hook
		break;

		case 6:
			config_bytes[2] = 0x06;	// OSSleepThread Hook
		break;

		case 7:
			config_bytes[2] = 0x07;	// AXNextFrame Hook
		break;
	}

	// filepatcher
	switch(filepatcherselect)
	{
		case 0:
			config_bytes[3] = 0x00;	// No file patch
		break;
		
		case 1:
			config_bytes[3] = 0x01;	// File Patch
		break;
	}

	// Ocarina
	switch(ocarinaselect)
	{
		case 0:
			config_bytes[4] = 0x00;	// No Ocarina
		break;
		
		case 1:
			config_bytes[4] = 0x01;	// Ocarina
		break;
	}

	// Gecko Paused Start
	switch(pausedstartselect)
	{
		case 0:
			config_bytes[5] = 0x00;	// No Paused Start
		break;
		
		case 1:
			config_bytes[5] = 0x01;	// Paused Start
		break;
	}

	// Debugger
	switch(debuggerselect)
	{
		case 0:
			config_bytes[7] = 0x00;	// No Debugger
			break;
			
		case 1:
			config_bytes[7] = 0x01;	// Debugger
			break;
	}
	
	// Video Modes Patch
	switch(videomodespatchselect)
	{
		case 0:
			config_bytes[12] = 0x00; // No Patch
			break;
		
		case 1:
			config_bytes[12] = 0x01; // Auto Patch
			break;
		
		case 2:
			config_bytes[12] = 0x02; // Patch Normal(same resolution)
			break;
		
		case 3:
			config_bytes[12] = 0x03; // Patch More (same interlaced or progressive)
			break;
			
		case 4:
			config_bytes[12] = 0x04; // Patch All (any)
			break;
	}

	// Country String
	switch(countrystringselect)
	{
		case 0:
			config_bytes[13] = 0x00;	// No Patch
		break;
		
		case 1:
			config_bytes[13] = 0x01;	// Patch
		break;
	}
	
	// Aspect Ratio
	switch(aspectratioselect)
	{
		case 0:
			config_bytes[14] = 0x00;	// No Patch
		break;
		
		case 1:
			config_bytes[14] = 0x01;	// 4:3
		break;
		
		case 2:
			config_bytes[14] = 0x02;	// 16:9
		break;
	}

	return 1;
}

static u32 menu_process_rebooterconf_flags()
{
	switch(recoveryselect)
	{
		case 0:
			config_bytes[8] = 0x00;
			break;
			
		case 1:
			config_bytes[8] = 0x01;
			break;
	}
	
	switch(regionfreeselect)
	{
		case 0:
			config_bytes[9] = 0x00;
			break;
			
		case 1:
			config_bytes[9] = 0x01;
			break;
	}
	
	switch(nocopyselect)
	{
		case 0:
			config_bytes[10] = 0x00;
			break;
			
		case 1:
			config_bytes[10] = 0x01;
			break;
	}
	
	switch(buttonskipselect)
	{
		case 0:
			config_bytes[11] = 0x00;
			break;
			
		case 1:
			config_bytes[11] = 0x01;
			break;
	}
	
	return 1;
}

void exitme() {
	
	if (loaderhbc) {	
		wifi_printf("menu_exitme: return to loader (HBC)\n");
		if (WII_LaunchTitle(0x00010001AF1BF516ULL) < 0)
			if (WII_LaunchTitle(0x000100014A4F4449ULL) < 0)
				if (WII_LaunchTitle(0x0001000148415858ULL) < 0)
					WII_ReturnToMenu();
	}
	else {
		wifi_printf("menu_exitme: return to Wii menu\n");
		WII_ReturnToMenu();
	}
}
	
static void menu_pad_root() {
	
	int i;
	
	PAD_ScanPads();
    u32 buttonsDown = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);

	WPAD_ScanPads();
	u32 WiibuttonsDown = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
	 
	if(root_menu_selected > root_menu_items-1){
		root_menu_selected = 0;
	}
	
	if(root_menu_selected < 0){
		root_menu_selected = root_menu_items-1;
	}
	
	for(i=0;i<root_menu_items;i++)
	{
		rootmenu[i].selected = 0;
	}

	rootmenu[root_menu_selected].selected = 1;

	if(buttonsDown & PAD_BUTTON_START){
		exitme();
	}
	
	if((buttonsDown & PAD_BUTTON_DOWN) ||
		(WiibuttonsDown & WPAD_BUTTON_DOWN)	||
		(WiibuttonsDown & WPAD_CLASSIC_BUTTON_DOWN)){
		root_menu_selected += 1;
		if (!identifysu && root_menu_selected == 1) root_menu_selected++;
		if (!identifysu && root_menu_selected == 2) root_menu_selected++;
		if (!identifysu && root_menu_selected == 4) root_menu_selected++;
	}

	if((buttonsDown & PAD_BUTTON_UP) ||
		(WiibuttonsDown & WPAD_BUTTON_UP) || 
		(WiibuttonsDown & WPAD_CLASSIC_BUTTON_UP)){
		root_menu_selected -= 1;
		if (!identifysu && root_menu_selected == 4) root_menu_selected--;
		if (!identifysu && root_menu_selected == 2) root_menu_selected--;
		if (!identifysu && root_menu_selected == 1) root_menu_selected--;
		if (!identifysu && root_menu_selected == -1) root_menu_selected=5;
	}

	// Root Actions
	if((root_menu_selected == 0 && buttonsDown & PAD_BUTTON_A) ||
		(root_menu_selected == 0 && WiibuttonsDown & WPAD_BUTTON_A) ||
		(root_menu_selected == 0 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_A)){
		menu_number = 8;
	}
	
	// Launch Channel
	if((root_menu_selected == 1 && buttonsDown & PAD_BUTTON_A) ||
	   (root_menu_selected == 1 && WiibuttonsDown & WPAD_BUTTON_A) ||
	   (root_menu_selected == 1 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_A)){
		if (channellist_menu_items == 0)
		{
			if (!menu_generatechannellist())
			{
				menu_number = 14;
			}
			else
				menu_number = 15;
		}
		else
			menu_number = 15;
	}
	
	// Launch Rebooter
	if((root_menu_selected == 2 && buttonsDown & PAD_BUTTON_A) ||
	   (root_menu_selected == 2 && WiibuttonsDown & WPAD_BUTTON_A) ||
	   (root_menu_selected == 2 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_A)){
		menu_number = 12;
	}

	// Switch to Config Menu
	if((root_menu_selected == 3 && buttonsDown & PAD_BUTTON_A) ||
		(root_menu_selected == 3 && WiibuttonsDown & WPAD_BUTTON_A) ||
		(root_menu_selected == 3 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_A)){
		menu_number = 1;
	}
	
	// Switch to Rebooter Config Menu
	if((root_menu_selected == 4 && buttonsDown & PAD_BUTTON_A) ||
	   (root_menu_selected == 4 && WiibuttonsDown & WPAD_BUTTON_A) ||
	   (root_menu_selected == 4 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_A)){
		menu_number = 13;
	}	
	
	// Download Cheat Codes
	if((root_menu_selected == 5 && buttonsDown & PAD_BUTTON_A) ||
	   (root_menu_selected == 5 && WiibuttonsDown & WPAD_BUTTON_A) ||
	   (root_menu_selected == 5 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_A)){
		if (downloadlist_menu_items == 0)
		{
			if (!menu_generatedownloadlist())
			{
				menu_number = 30;
			}
			else
				menu_number = 31;
		}
		else
			menu_number = 31;
	}	
	
	// Exit
	if((root_menu_selected == 6 && buttonsDown & PAD_BUTTON_A) ||
	   (root_menu_selected == 6 && WiibuttonsDown & WPAD_BUTTON_A) ||
	   (root_menu_selected == 6 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_A)){
		exitme();
	}	

}	
	
static void menu_pad_config() {
	
	int i;
	u32 ret;
	
	PAD_ScanPads();
    u32 buttonsDown = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);

	WPAD_ScanPads();
	u32 WiibuttonsDown = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);

	if(config_menu_selected > config_menu_items-1){
		config_menu_selected = 0;
	}
	
	if(config_menu_selected < 0){
		config_menu_selected = config_menu_items-1;
	}

	for(i=0;i<config_menu_items;i++)
	{
		configmenu[i].selected = 0;
	}
	if(config_menu_selected < 0){
		config_menu_selected = config_menu_items-1;
	}

	configmenu[config_menu_selected].selected = 1;

	if((buttonsDown & PAD_BUTTON_DOWN) ||
		(WiibuttonsDown & WPAD_BUTTON_DOWN) ||
		(WiibuttonsDown & WPAD_CLASSIC_BUTTON_DOWN)){
		config_menu_selected += 1;
	}

	if((buttonsDown & PAD_BUTTON_UP) ||
		(WiibuttonsDown & WPAD_BUTTON_UP) ||
		(WiibuttonsDown & WPAD_CLASSIC_BUTTON_UP)){
		config_menu_selected -= 1;
	}

	// Config Actions
	// Language
	if((config_menu_selected == 0 && buttonsDown & PAD_BUTTON_LEFT) ||
		(config_menu_selected == 0 && WiibuttonsDown & WPAD_BUTTON_LEFT) ||
		(config_menu_selected == 0 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
		if(langselect == 0){
				langselect = 0;
			}
			else {
				langselect -= 1;
			}	
	}
	if((config_menu_selected == 0 && buttonsDown & PAD_BUTTON_RIGHT) ||
		(config_menu_selected == 0 && WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
		(config_menu_selected == 0 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT)){
		if(langselect == 10){
				langselect = 10;
			}
			else {
				langselect += 1;
			}		
	}

	// Force NTSC
	if((config_menu_selected == 1 && buttonsDown & PAD_BUTTON_LEFT) ||
		(config_menu_selected == 1 && WiibuttonsDown & WPAD_BUTTON_LEFT) ||
		(config_menu_selected == 1 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
		if(ntscselect == 0){
			ntscselect = 0;
		}
		else {
			ntscselect -= 1;
		}	
	}

	if((config_menu_selected == 1 && buttonsDown & PAD_BUTTON_RIGHT) ||
		(config_menu_selected == 1 && WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
		(config_menu_selected == 1 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT)){
		if(ntscselect == 1){
			ntscselect = 1;
		}
		else {
			ntscselect += 1;
		}	
	}

	// Force PAL60
	if((config_menu_selected == 2 && buttonsDown & PAD_BUTTON_LEFT) ||
		(config_menu_selected == 2 && WiibuttonsDown & WPAD_BUTTON_LEFT) ||
		(config_menu_selected == 2 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
		if(pal60select == 0){
			pal60select = 0;
		}
		else {
			pal60select -= 1;
		}	
	}

	if((config_menu_selected == 2 && buttonsDown & PAD_BUTTON_RIGHT) ||
		(config_menu_selected == 2 && WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
		(config_menu_selected == 2 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT)){
		if(pal60select == 1){
			pal60select = 1;
		}
		else {
			pal60select += 1;
		}	
	}

	// Force PAL50
	if((config_menu_selected == 3 && buttonsDown & PAD_BUTTON_LEFT) ||
		(config_menu_selected == 3 && WiibuttonsDown & WPAD_BUTTON_LEFT) ||
		(config_menu_selected == 3 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
		if(pal50select == 0){
			pal50select = 0;
		}
		else {
			pal50select -= 1;
		}	
	}

	if((config_menu_selected == 3 && buttonsDown & PAD_BUTTON_RIGHT) ||
		(config_menu_selected == 3 && WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
		(config_menu_selected == 3 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT)){
		if(pal50select == 1){
			pal50select = 1;
		}
		else {
			pal50select += 1;
		}	
	}

	// Hook Type
	if((config_menu_selected == 4 && buttonsDown & PAD_BUTTON_LEFT) ||
		(config_menu_selected == 4 && WiibuttonsDown & WPAD_BUTTON_LEFT) ||
		(config_menu_selected == 4 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
		if(hooktypeselect == 0){
			hooktypeselect = 0;
		}
		else {
			hooktypeselect -= 1;
		}	
	}
	if((config_menu_selected == 4 && buttonsDown & PAD_BUTTON_RIGHT) ||
		(config_menu_selected == 4 && WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
		(config_menu_selected == 4 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT)){
		if(hooktypeselect == 7){
			hooktypeselect = 7;
		}
		else {
			hooktypeselect += 1;
		}	
	}
	
	// Debugger
	if((config_menu_selected == 5 && buttonsDown & PAD_BUTTON_LEFT) ||
	   (config_menu_selected == 5 && WiibuttonsDown & WPAD_BUTTON_LEFT) ||
	   (config_menu_selected == 5 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
		if(debuggerselect == 0){
			debuggerselect = 0;
		}
		else {
			debuggerselect -= 1;
		}	
	}
	
	if((config_menu_selected == 5 && buttonsDown & PAD_BUTTON_RIGHT) ||
	   (config_menu_selected == 5 && WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
	   (config_menu_selected == 5 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT)){
		if(debuggerselect == 1){
			debuggerselect = 1;
		}
		else {
			debuggerselect += 1;
		}	
	}

	// File Patcher
	if((config_menu_selected == 6 && buttonsDown & PAD_BUTTON_LEFT) ||
		(config_menu_selected == 6 && WiibuttonsDown & WPAD_BUTTON_LEFT) ||
		(config_menu_selected == 6 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
		if(filepatcherselect == 0){
			filepatcherselect = 0;
		}
		else {
			filepatcherselect -= 1;
		}	
	}

	if((config_menu_selected == 6 && buttonsDown & PAD_BUTTON_RIGHT) ||
		(config_menu_selected == 6 && WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
		(config_menu_selected == 6 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT)){
		if(filepatcherselect == 1){
			filepatcherselect = 1;
		}
		else {
			filepatcherselect += 1;
		}	
	}
	
	// Ocarina
	if((config_menu_selected == 7 && buttonsDown & PAD_BUTTON_LEFT) ||
		(config_menu_selected == 7 && WiibuttonsDown & WPAD_BUTTON_LEFT) ||
		(config_menu_selected == 7 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
		if(ocarinaselect == 0){
			ocarinaselect = 0;
		}
		else {
			ocarinaselect -= 1;
		}	
	}

	if((config_menu_selected == 7 && buttonsDown & PAD_BUTTON_RIGHT) ||
		(config_menu_selected == 7 && WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
		(config_menu_selected == 7 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT)){
		if(ocarinaselect == 1){
			ocarinaselect = 1;
		}
		else {
			ocarinaselect += 1;
		}	
	}

	// Paused Start
	if((config_menu_selected == 8 && buttonsDown & PAD_BUTTON_LEFT) ||
		(config_menu_selected == 8 && WiibuttonsDown & WPAD_BUTTON_LEFT) ||
		(config_menu_selected == 8 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
		if(pausedstartselect == 0){
			pausedstartselect = 0;
		}
		else {
			pausedstartselect -= 1;
		}	
	}

	if((config_menu_selected == 8 && buttonsDown & PAD_BUTTON_RIGHT) ||
		(config_menu_selected == 8 && WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
		(config_menu_selected == 8 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT) ){
		if(pausedstartselect == 1){
			pausedstartselect = 1;
		}
		else {
			pausedstartselect += 1;
		}	
	}
	
	// Video Modes Patch
	if((config_menu_selected == 9 && buttonsDown & PAD_BUTTON_LEFT) ||
		(config_menu_selected == 9 && WiibuttonsDown & WPAD_BUTTON_LEFT) ||
		(config_menu_selected == 9 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
		if(videomodespatchselect == 0){
				videomodespatchselect = 0;
			}
			else {
				videomodespatchselect -= 1;
			}	
	}
	if((config_menu_selected == 9 && buttonsDown & PAD_BUTTON_RIGHT) ||
		(config_menu_selected == 9 && WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
		(config_menu_selected == 9 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT)){
		if(videomodespatchselect == 4){
				videomodespatchselect = 4;
			}
			else {
				videomodespatchselect += 1;
			}		
	}
	
	// Country String
	if((config_menu_selected == 10 && buttonsDown & PAD_BUTTON_LEFT) ||
		(config_menu_selected == 10 && WiibuttonsDown & WPAD_BUTTON_LEFT) ||
		(config_menu_selected == 10 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
		if(countrystringselect == 0){
			countrystringselect = 0;
		}
		else {
			countrystringselect -= 1;
		}	
	}

	if((config_menu_selected == 10 && buttonsDown & PAD_BUTTON_RIGHT) ||
		(config_menu_selected == 10 && WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
		(config_menu_selected == 10 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT)){
		if(countrystringselect == 1){
			countrystringselect = 1;
		}
		else {
			countrystringselect += 1;
		}	
	}
	
	// Aspect Ratio
	if((config_menu_selected == 11 && buttonsDown & PAD_BUTTON_LEFT) ||
		(config_menu_selected == 11 && WiibuttonsDown & WPAD_BUTTON_LEFT) ||
		(config_menu_selected == 11 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
		if(aspectratioselect == 0){
			aspectratioselect = 0;
		}
		else {
			aspectratioselect -= 1;
		}	
	}
	
	if((config_menu_selected == 11 && buttonsDown & PAD_BUTTON_RIGHT) ||
		(config_menu_selected == 11 && WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
		(config_menu_selected == 11 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT)){
		if(aspectratioselect == 2){
			aspectratioselect = 2;
		}
		else {
			aspectratioselect += 1;
		}	
	}

	// Save config
	if((config_menu_selected == 12 && buttonsDown & PAD_BUTTON_A) ||
		(config_menu_selected == 12 && WiibuttonsDown & WPAD_BUTTON_A) ||
		(config_menu_selected == 12 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_A)){
		ret = menu_process_config_flags();	// save the config bytes in mem
		if(ret){
			if(sd_found == 1){
				sd_save_config();
				confirm_sd = 1;
			}
			else {
				error_sd = 1;
			}
		}		
	}
	
	// Return to Main Menu
	if((buttonsDown & PAD_BUTTON_B) ||
		(WiibuttonsDown & WPAD_BUTTON_B) ||
		(WiibuttonsDown & WPAD_CLASSIC_BUTTON_B)){
		ret = menu_process_config_flags();
		if(ret){	
			menu_number = 0;
		}
	}
}

static void menu_pad_rebooterconf()
{
	int i;
	u32 ret;
	
	PAD_ScanPads();
    u32 buttonsDown = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);
	
	WPAD_ScanPads();
	u32 WiibuttonsDown = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
	
	if(rebooterconf_menu_selected > rebooterconf_menu_items-1){
		rebooterconf_menu_selected = 0;
	}
	
	if(rebooterconf_menu_selected < 0){
		rebooterconf_menu_selected = rebooterconf_menu_items-1;
	}

	for(i=0;i<rebooterconf_menu_items;i++)
	{
		rebooterconfmenu[i].selected = 0;
	}
	if(rebooterconf_menu_selected < 0){
		rebooterconf_menu_selected = rebooterconf_menu_items-1;
	}

	rebooterconfmenu[rebooterconf_menu_selected].selected = 1;
	
	if((buttonsDown & PAD_BUTTON_DOWN) ||
	   (WiibuttonsDown & WPAD_BUTTON_DOWN) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_DOWN)){
		rebooterconf_menu_selected += 1;
	}
	
	if((buttonsDown & PAD_BUTTON_UP) ||
	   (WiibuttonsDown & WPAD_BUTTON_UP) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_UP)){
		rebooterconf_menu_selected -= 1;
	}
	
	// Config Actions
	// recovery
	if((rebooterconf_menu_selected == 0 && buttonsDown & PAD_BUTTON_LEFT) ||
	   (rebooterconf_menu_selected == 0 && WiibuttonsDown & WPAD_BUTTON_LEFT) ||
	   (rebooterconf_menu_selected == 0 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
		if(recoveryselect == 0){
			recoveryselect = 0;
		}
		else {
			recoveryselect -= 1;
		}	
	}
	
	if((rebooterconf_menu_selected == 0 && buttonsDown & PAD_BUTTON_RIGHT) ||
	   (rebooterconf_menu_selected == 0 && WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
	   (rebooterconf_menu_selected == 0 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT)){
		if(recoveryselect == 1){
			recoveryselect = 1;
		}
		else {
			recoveryselect += 1;
		}	
	}
	
	// region free
	if((rebooterconf_menu_selected == 1 && buttonsDown & PAD_BUTTON_LEFT) ||
	   (rebooterconf_menu_selected == 1 && WiibuttonsDown & WPAD_BUTTON_LEFT) ||
	   (rebooterconf_menu_selected == 1 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
		if(regionfreeselect == 0){
			regionfreeselect = 0;
		}
		else {
			regionfreeselect -= 1;
		}	
	}
	
	if((rebooterconf_menu_selected == 1 && buttonsDown & PAD_BUTTON_RIGHT) ||
	   (rebooterconf_menu_selected == 1 && WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
	   (rebooterconf_menu_selected == 1 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT)){
		if(regionfreeselect == 1){
			regionfreeselect = 1;
		}
		else {
			regionfreeselect += 1;
		}	
	}
	
	// no copy
	if((rebooterconf_menu_selected == 2 && buttonsDown & PAD_BUTTON_LEFT) ||
	   (rebooterconf_menu_selected == 2 && WiibuttonsDown & WPAD_BUTTON_LEFT) ||
	   (rebooterconf_menu_selected == 2 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
		if(nocopyselect == 0){
			nocopyselect = 0;
		}
		else {
			nocopyselect -= 1;
		}	
	}
	
	if((rebooterconf_menu_selected == 2 && buttonsDown & PAD_BUTTON_RIGHT) ||
	   (rebooterconf_menu_selected == 2 && WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
	   (rebooterconf_menu_selected == 2 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT)){
		if(nocopyselect == 1){
			nocopyselect = 1;
		}
		else {
			nocopyselect += 1;
		}	
	}
	
	// button skip
	if((rebooterconf_menu_selected == 3 && buttonsDown & PAD_BUTTON_LEFT) ||
	   (rebooterconf_menu_selected == 3 && WiibuttonsDown & WPAD_BUTTON_LEFT) ||
	   (rebooterconf_menu_selected == 3 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
		if(buttonskipselect == 0){
			buttonskipselect = 0;
		}
		else {
			buttonskipselect -= 1;
		}	
	}
	
	if((rebooterconf_menu_selected == 3 && buttonsDown & PAD_BUTTON_RIGHT) ||
	   (rebooterconf_menu_selected == 3 && WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
	   (rebooterconf_menu_selected == 3 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT)){
		if(buttonskipselect == 1){
			buttonskipselect = 1;
		}
		else {
			buttonskipselect += 1;
		}	
	}
	
	// Save config
	if((rebooterconf_menu_selected == 4 && buttonsDown & PAD_BUTTON_A) ||
		(rebooterconf_menu_selected == 4 && WiibuttonsDown & WPAD_BUTTON_A) ||
		(rebooterconf_menu_selected == 4 && WiibuttonsDown & WPAD_CLASSIC_BUTTON_A)){
		ret = menu_process_rebooterconf_flags();	// save the config bytes in mem
		if(ret){
			if(sd_found == 1){
				sd_save_config();
				confirm_sd = 1;
			}
			else {
				error_sd = 1;
			}
		}		
	}
	
	// Return to Main Menu
	if((buttonsDown & PAD_BUTTON_B) ||
	   (WiibuttonsDown & WPAD_BUTTON_B) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_B)){
		ret = menu_process_rebooterconf_flags();
		if(ret){	
			menu_number = 0;
		}
	}
}	
	
static void menu_pad_channellist() {
	
	int i;
	
	PAD_ScanPads();
    u32 buttonsDown = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);
	
	WPAD_ScanPads();
	u32 WiibuttonsDown = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
	
	for(i=0;i<channellist_menu_items;i++)
	{
		channellistmenu[i].selected = 0;
	}
	if(channellist_menu_selected < 0){
		channellist_menu_selected = channellist_menu_items-1;
	}

	channellistmenu[channellist_menu_selected].selected = 1;
	
	if((buttonsDown & PAD_BUTTON_DOWN) ||
	   (WiibuttonsDown & WPAD_BUTTON_DOWN) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_DOWN)){
		channellist_menu_selected += 1;
		if (channellist_menu_selected >= channellist_menu_items)
		{
			channellist_menu_selected = 0;
			channellist_menu_top = 0;
		}
		else if (channellist_menu_selected - channellist_menu_top > 10)
			channellist_menu_top++;
	}
	
	if((buttonsDown & PAD_BUTTON_UP) ||
	   (WiibuttonsDown & WPAD_BUTTON_UP) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_UP)){
		channellist_menu_selected -= 1;
		if (channellist_menu_selected < 0)
		{
			channellist_menu_selected = channellist_menu_items-1;
			channellist_menu_top = (channellist_menu_items > 11) ? channellist_menu_items-11 : 0;
		}
		else if (channellist_menu_top > channellist_menu_selected)
			channellist_menu_top--;
	}
	
	if((buttonsDown & PAD_BUTTON_A) ||
	   (WiibuttonsDown & WPAD_BUTTON_A) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_A)){
		channeltoload = channellistmenu[channellist_menu_selected].titleid;
		menu_number = 0;
	}
	
	if((buttonsDown & PAD_BUTTON_B) ||
	   (WiibuttonsDown & WPAD_BUTTON_B) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_B)){
		menu_number = 0;
	}
}	

static void menu_pad_downloadlist() {
	
	int i;
	
	PAD_ScanPads();
    u32 buttonsDown = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);
	
	WPAD_ScanPads();
	u32 WiibuttonsDown = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
	
	for(i=0;i<downloadlist_menu_items;i++)
	{
		downloadlistmenu[i].selected = 0;
	}
	if(downloadlist_menu_selected < 0){
		downloadlist_menu_selected = downloadlist_menu_items-1;
	}

	downloadlistmenu[downloadlist_menu_selected].selected = 1;
	
	if((buttonsDown & PAD_BUTTON_DOWN) ||
	   (WiibuttonsDown & WPAD_BUTTON_DOWN) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_DOWN)){
		downloadlist_menu_selected += 1;
		if (downloadlist_menu_selected >= downloadlist_menu_items)
		{
			downloadlist_menu_selected = 0;
			downloadlist_menu_top = 0;
		}
		else if (downloadlist_menu_selected - downloadlist_menu_top > 10)
			downloadlist_menu_top++;
	}
	
	if((buttonsDown & PAD_BUTTON_UP) ||
	   (WiibuttonsDown & WPAD_BUTTON_UP) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_UP)){
		downloadlist_menu_selected -= 1;
		if (downloadlist_menu_selected < 0)
		{
			downloadlist_menu_selected = downloadlist_menu_items-1;
			downloadlist_menu_top = (downloadlist_menu_items > 11) ? downloadlist_menu_items-11 : 0;
		}
		else if (downloadlist_menu_top > downloadlist_menu_selected)
			downloadlist_menu_top--;
	}
	
	if((buttonsDown & PAD_BUTTON_A) ||
	   (WiibuttonsDown & WPAD_BUTTON_A) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_A)){
		rundownloadnow = 1;
		memcpy(downloadtoload, (downloadlistmenu+downloadlist_menu_selected)->gameidbuffer, 8);
		wifi_printf("menu_menu_pad_downloadlist: downloadtoload value = %s\n", downloadtoload);
		menu_number = 0;
	}
	
	if((buttonsDown & PAD_BUTTON_B) ||
	   (WiibuttonsDown & WPAD_BUTTON_B) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_B)){
		menu_number = 0;
	}
}	

static void menu_pad_codelist() {
	
	int i;
	s32 retval;
	
	PAD_ScanPads();
    u32 buttonsDown = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);
	u32 buttonsHeld = PAD_ButtonsHeld(0) | PAD_ButtonsHeld(1) | PAD_ButtonsHeld(2) | PAD_ButtonsHeld(3);
	
	WPAD_ScanPads();
	u32 WiibuttonsDown = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
	u32 WiibuttonsHeld = WPAD_ButtonsHeld(0) | WPAD_ButtonsHeld(1) | WPAD_ButtonsHeld(2) | WPAD_ButtonsHeld(3);
	
	for(i=0;i<codelist_menu_items;i++)
	{
		codelistmenu[i].selected = 0;
	}
	if(codelist_menu_selected < 0){
		codelist_menu_selected = codelist_menu_items-1;
	}

	codelistmenu[codelist_menu_selected].selected = 1;
	
	if((buttonsDown & PAD_BUTTON_DOWN) ||
	   (WiibuttonsDown & WPAD_BUTTON_DOWN) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_DOWN)){
		codelist_menu_selected += 1;
		if (codelist_menu_selected >= codelist_menu_items)
		{
			codelist_menu_selected = 0;
			codelist_menu_top = 0;
		}
		else if (codelist_menu_selected - codelist_menu_top > 10)
			codelist_menu_top++;
	}
	
	if((buttonsDown & PAD_BUTTON_UP) ||
	   (WiibuttonsDown & WPAD_BUTTON_UP) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_UP)){
		codelist_menu_selected -= 1;
		if (codelist_menu_selected < 0)
		{
			codelist_menu_selected = codelist_menu_items-1;
			codelist_menu_top = (codelist_menu_items > 11) ? codelist_menu_items-11 : 0;
		}
		else if (codelist_menu_top > codelist_menu_selected)
			codelist_menu_top--;
	}
	
	// Move +/- One Page
	if((buttonsDown & PAD_BUTTON_RIGHT) ||
	   (WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT)){
		codelist_menu_top += 11;
		if (codelist_menu_top >= codelist_menu_items)
		{
			codelist_menu_selected = 0;
			codelist_menu_top = 0;
		}
		else { 
			codelist_menu_selected += 11;
			if (codelist_menu_selected >= codelist_menu_items)
			{
				codelist_menu_selected = codelist_menu_top;
			}
		}
	}
	
	if((buttonsDown & PAD_BUTTON_LEFT) ||
	   (WiibuttonsDown & WPAD_BUTTON_LEFT) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
	    if (codelist_menu_top == 0)
		{
			codelist_menu_top = (codelist_menu_items > 11) ? codelist_menu_items-11 : 0;
			codelist_menu_selected = codelist_menu_top;
		}
		else 
		{
			codelist_menu_top -= 11;
			if (codelist_menu_top < 0)
			{
				codelist_menu_top = 0;
				codelist_menu_selected = 0;
			} else
			{
				codelist_menu_selected -= 11;
				if (codelist_menu_selected < 0)
				{
					codelist_menu_selected = codelist_menu_top;
				}
			}
		}
	}
	
	// Scroll over Pages
	if((buttonsHeld & PAD_TRIGGER_R) ||
	   (WiibuttonsHeld & WPAD_BUTTON_1) ||
	   (WiibuttonsHeld & WPAD_CLASSIC_BUTTON_FULL_R)){   
		codelist_menu_top += 11;
		if (codelist_menu_top >= codelist_menu_items)
		{
			codelist_menu_selected = 0;
			codelist_menu_top = 0;
		}
		else { 
			codelist_menu_selected += 11;
			if (codelist_menu_selected >= codelist_menu_items)
			{
				codelist_menu_selected = codelist_menu_top;
			}
		}
	}
	
	if((buttonsHeld & PAD_TRIGGER_L) ||
	   (WiibuttonsHeld & WPAD_BUTTON_2) ||
	   (WiibuttonsHeld & WPAD_CLASSIC_BUTTON_FULL_L)){
		if (codelist_menu_top == 0)
		{
			codelist_menu_top = (codelist_menu_items > 11) ? codelist_menu_items-11 : 0;
			codelist_menu_selected = codelist_menu_top;
		}
		else 
		{
			codelist_menu_top -= 11;
			if (codelist_menu_top < 0)
			{
				codelist_menu_top = 0;
				codelist_menu_selected = 0;
			} else
			{
				codelist_menu_selected -= 11;
				if (codelist_menu_selected < 0)
				{
					codelist_menu_selected = codelist_menu_top;
				}
			}
		}
	}	
	
	// Change Flag
	if((buttonsDown & PAD_BUTTON_X) ||
	   (WiibuttonsDown & WPAD_BUTTON_MINUS) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_MINUS)){
		if(codelistmenu[codelist_menu_selected].flag != '?'){
			codelistmenu[codelist_menu_selected].flag = '-';
		}
	}
	
	if((buttonsDown & PAD_BUTTON_Y) ||
	   (WiibuttonsDown & WPAD_BUTTON_PLUS) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_PLUS)){
		if(codelistmenu[codelist_menu_selected].flag != '?'){
			codelistmenu[codelist_menu_selected].flag = '+';
		}
	}
	
	// Create and Update CHT and GCT files
	if((buttonsDown & PAD_BUTTON_START) ||
	   (WiibuttonsDown & WPAD_BUTTON_HOME) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_HOME)){
		menu_number = 60;
	}
	
	if((buttonsDown & PAD_BUTTON_A) ||
	   (WiibuttonsDown & WPAD_BUTTON_A) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_A)){
		if (codeline_menu_items == 0) {	
			if (menu_generatecodeline(codelist_menu_selected))
				menu_number = 50;
			else
				menu_number = 51;
		} else {
			menu_number = 50;
		}
	}
	
	if((buttonsDown & PAD_BUTTON_B) ||
	   (WiibuttonsDown & WPAD_BUTTON_B) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_B)){
		retval = dealloccht(head);
		wifi_printf("menu_menu_pad_codelist: retval value dealloc cht = %d\n", retval);
		i= 0;
		while (i < codelist_menu_items) {
			free(codelistmenu[i].citems);
			i++;
		}
		codelist_menu_items = 0;
		codelist_menu_selected = 0;
		codelist_menu_top = 0;
		menu_number = 0;
	}
}

char plusminus(char input, bool plus)
{
	char retval = '0';
	
	if(input >= '0' && input <= '9') {
		if(plus)
			retval = input + 1; 
		else
			retval = input - 1; 
		if(retval < '0') 
			retval = 'F';
		else if(retval > '9') 
			retval = 'A';
	} else if (input >= 'A' && input <= 'F') {
		if(plus)
			retval = input + 1;
		else
			retval = input - 1;
		if(retval < 'A')
			retval = '9';
		else if(retval > 'F')
			retval = '0';
	} else if (input >= 'a' && input <= 'f') {
		if(plus)
			retval = input + 1;
		else
			retval = input - 1;
		if(retval < 'a')
			retval = '9';
		else if(retval > 'f')
			retval = '0';
	}

	return retval;	
}

char saveedit()
{
	struct CodeNode *temp = NULL;
	codelistitems *templist = NULL;
	codelineitems *templine = NULL;
	u16 i = 0, id = 0;
	u32 codesize;
	char *code = NULL;
	char flag = '!';
	
	templist = &codelistmenu[codelist_menu_selected];
	id = templist->id + 1;
	i = 0;
	temp = head;
	if (temp->next == NULL)
		return '!';
	while (temp->next != NULL && i < id)
	{
		temp = temp->next;
		i++;
	}
	code = temp->code;
	codesize = temp->codesize;
	if (codesize != codeline_menu_items*0x12)
		return '!';
	for (i = 0; i < codeline_menu_items; i++)
	{
		templine = &codelinemenu[i];
		templine->litems[0x11] = 0xA;
		memcpy(code+i*0x12, templine->litems, 0x12);
		templine->litems[0x11] = 0;
	}
	switch(temp->flag)
	{
		case '?':
			flag = checkflag(code, codesize);
		break;
		case '-':
			flag = '-';
		break;
		case '+':
			flag = '+';
		break;
	}
	temp->flag = flag;
	templist->flag = flag;
	
	return flag;
}

static void menu_pad_codeline() {
	
	int i;
	
	PAD_ScanPads();
    u32 buttonsDown = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);
	
	WPAD_ScanPads();
	u32 WiibuttonsDown = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
	
	for(i=0;i<codeline_menu_items;i++)
	{
		codelinemenu[i].selected = 0;
	}
	if(codeline_menu_selected < 0){
		codeline_menu_selected = codeline_menu_items-1;
	}

	codelinemenu[codeline_menu_selected].selected = 1;
	codeline_col = codelinemenu[codeline_menu_selected].col;
	codeline_colchar = codelinemenu[codeline_menu_selected].litems[codeline_col];
	
	if((buttonsDown & PAD_BUTTON_DOWN) ||
	   (WiibuttonsDown & WPAD_BUTTON_DOWN) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_DOWN)){
		codeline_menu_selected += 1;
		if (codeline_menu_selected >= codeline_menu_items)
		{
			codeline_menu_selected = 0;
			codeline_menu_top = 0;
		}
		else if (codeline_menu_selected - codeline_menu_top > 5)
			codeline_menu_top++;
		codeline_col = codelinemenu[codeline_menu_selected].col;
	}
	
	if((buttonsDown & PAD_BUTTON_UP) ||
	   (WiibuttonsDown & WPAD_BUTTON_UP) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_UP)){
		codeline_menu_selected -= 1;
		if (codeline_menu_selected < 0)
		{
			codeline_menu_selected = codeline_menu_items-1;
			codeline_menu_top = (codeline_menu_items > 6) ? codeline_menu_items-6 : 0;
		}
		else if (codeline_menu_top > codeline_menu_selected)
			codeline_menu_top--;
		codeline_col = codelinemenu[codeline_menu_selected].col;
	}
	
	// Move Cursor +/- One Column
	if((buttonsDown & PAD_BUTTON_RIGHT) ||
	   (WiibuttonsDown & WPAD_BUTTON_RIGHT) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_RIGHT)){
		codelinemenu[codeline_menu_selected].col++;
		if (codelinemenu[codeline_menu_selected].col > 0x10)
			codelinemenu[codeline_menu_selected].col = 0;
		if (codelinemenu[codeline_menu_selected].col == 0x8)
			codelinemenu[codeline_menu_selected].col++;
		codeline_col = codelinemenu[codeline_menu_selected].col;
	}
	
	if((buttonsDown & PAD_BUTTON_LEFT) ||
	   (WiibuttonsDown & WPAD_BUTTON_LEFT) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_LEFT)){
	    codelinemenu[codeline_menu_selected].col--;
		if (codelinemenu[codeline_menu_selected].col < 0)
			codelinemenu[codeline_menu_selected].col = 0x10;
		if (codelinemenu[codeline_menu_selected].col == 0x8)
			codelinemenu[codeline_menu_selected].col--;
		codeline_col = codelinemenu[codeline_menu_selected].col;
	}
	
	// decrease number at codeline_col
	if((buttonsDown & PAD_BUTTON_X) ||
	   (WiibuttonsDown & WPAD_BUTTON_MINUS) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_MINUS)){
		codeline_colchar = codelinemenu[codeline_menu_selected].litems[codeline_col];
		codeline_colchar = plusminus(codeline_colchar, false);
		codelinemenu[codeline_menu_selected].litems[codeline_col] = codeline_colchar;
		codelineflag = '!';
	}
	
	// increase number at codeline_col
	if((buttonsDown & PAD_BUTTON_Y) ||
	   (WiibuttonsDown & WPAD_BUTTON_PLUS) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_PLUS)){
		codeline_colchar = codelinemenu[codeline_menu_selected].litems[codeline_col];
		codeline_colchar = plusminus(codeline_colchar, true);
		codelinemenu[codeline_menu_selected].litems[codeline_col] = codeline_colchar;
		codelineflag = '!';
	}
	
	// save
	if((buttonsDown & PAD_BUTTON_A) ||
	   (WiibuttonsDown & WPAD_BUTTON_A) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_A)){
		codelineflag = saveedit();
	}
	
	if((buttonsDown & PAD_BUTTON_B) ||
	   (WiibuttonsDown & WPAD_BUTTON_B) ||
	   (WiibuttonsDown & WPAD_CLASSIC_BUTTON_B)){
	    codeline_menu_items = 0;
		codeline_menu_selected = 0;
		codeline_menu_top = 0;
		menu_number = 40;
	}
}	
	
void menu_drawroot() {
	
	int i;

	resetscreen();
	menu_pad_root();

	for(i=0;i<root_menu_items;i++)
	{
		if (identifysu || (i != 1 && i != 2 && i != 4))
		{
			if(rootmenu[i].selected == 1){
				printf("\n==> %s", rootmenu[i].ritems);
			}
			else {
				printf("\n    %s", rootmenu[i].ritems);
			}
		}
	}
}

void menu_load_config() {

	switch(config_bytes[0])
	{
		case 0xCD:
			langselect  = 0;
		break;

		case 0x00:
			langselect  = 1;
		break;

		case 0x01:
			langselect  = 2;
		break;

		case 0x02:
			langselect  = 3;
		break;

		case 0x03:
			langselect  = 4;
		break;

		case 0x04:
			langselect  = 5;
		break;

		case 0x05:
			langselect  = 6;
		break;
		
		case 0x06:
			langselect  = 7;
		break;
		
		case 0x07:
			langselect  = 8;
		break;
		
		case 0x08:
			langselect  = 9;
		break;
		
		case 0x09:
			langselect  = 10;
		break;
	}

	switch(config_bytes[1])
	{
		case 0x00:	// apply no patches, disabled
			pal60select = 0;
			ntscselect = 0;
			pal50select = 0;
		break;

		case 0x01:	// 0x01 (Force PAL60)
			pal60select = 1;
		break;

		case 0x02:	// 0x02 (Force PAL50)
			pal50select = 1;
		break;

		case 0x03:	// 0x03 (Force NTSC)
			ntscselect = 1;
		break;
	}

	// Hook Type
	switch(config_bytes[2])
	{
		case 0x00:
			hooktypeselect = 0;	// No Hooks
		break;
		
		case 0x01:
			hooktypeselect = 1;	// VBI
		break;

		case 0x02:
			hooktypeselect = 2;	// KPAD Read
		break;

		case 0x03:
			hooktypeselect = 3;	// Joypad Hook
		break;
			
		case 0x04:
			hooktypeselect = 4;	// GXDraw Hook
		break;
			
		case 0x05:
			hooktypeselect = 5;	// GXFlush Hook
		break;
			
		case 0x06:
			hooktypeselect = 6;	// OSSleepThread Hook
		break;
			
		case 0x07:
			hooktypeselect = 7;	// AXNextFrame Hook
		break;
	}
	
	// filepatcher
	switch(config_bytes[3])
	{
		case 0x00:
			filepatcherselect = 0;	// No file patch
		break;
		
		case 0x01:
			filepatcherselect = 1;	// File Patch
		break;
	}
	
	// Ocarina
	switch(config_bytes[4])
	{
		case 0x00:
			ocarinaselect = 0;	// No Ocarina
		break;
		
		case 0x01:
			ocarinaselect = 1;	// Ocarina
		break;
	}

	// Paused Start
	switch(config_bytes[5])
	{
		case 0x00:
			pausedstartselect = 0;	// No Paused Start
		break;
		
		case 0x01:
			pausedstartselect = 1;	// Paused Start
		break;
	}

	// Gecko Slot
	switch(config_bytes[6])
	{
		case 0x00:
			geckoslotselect = 0;	// Slot B
		break;
		
		case 0x01:
			geckoslotselect = 1;	// Slot A
		break;
	}
	
	// Debugger
	switch(config_bytes[7])
	{
		case 0x00:
			debuggerselect = 0;	// No Debugger
			break;
			
		case 0x01:
			debuggerselect = 1;	// Debugger
			break;
	}
	
	// recovery
	switch(config_bytes[8])
	{
		case 0x00:
			recoveryselect = 0;
			break;
			
		case 0x01:
			recoveryselect = 1;
			break;
	}
	
	// region free
	switch(config_bytes[9])
	{
		case 0x00:
			regionfreeselect = 0;
			break;
			
		case 0x01:
			regionfreeselect = 1;
			break;
	}
	
	// no copy
	switch(config_bytes[10])
	{
		case 0x00:
			nocopyselect = 0;
			break;
			
		case 0x01:
			nocopyselect = 1;
			break;
	}
	
	// button skip
	switch(config_bytes[11])
	{
		case 0x00:
			buttonskipselect = 0;
			break;
			
		case 0x01:
			buttonskipselect = 1;
			break;
	}

	// Video Modes Patch
	switch(config_bytes[12])
	{
		case 0x00:
			videomodespatchselect = 0;	// No Patch
		break;
		
		case 0x01:
			videomodespatchselect = 1;	// Auto Patch
		break;

		case 0x02:
			videomodespatchselect = 2;	// Patch Normal
		break;

		case 0x03:
			videomodespatchselect = 3;	// Patch More
		break;
			
		case 0x04:
			videomodespatchselect = 4;	// Patch All
		break;
	}
	
	// Country String
	switch(config_bytes[13])
	{
		case 0x00:
			countrystringselect = 0;	// No Patch
		break;
		
		case 0x01:
			countrystringselect = 1;	// Patch
		break;
	}
	
	// Aspect Ratio
	switch(config_bytes[14])
	{
		case 0x00:
			aspectratioselect = 0;	// No Patch
		break;
		
		case 0x01:
			aspectratioselect = 1;	// 4:3
		break;
		
		case 0x02:
			aspectratioselect = 2;	// 16:9
		break;
	}
}

static void menu_drawconfig() {
	
	int i;
	char textbuffer[255];

	resetscreen();
	menu_pad_config();
	
	for(i=0;i<config_menu_items;i++)
	{
		memset(textbuffer, 0, 255);
		switch (i)
		{
			case 0:
				sprintf(textbuffer, "%s", languages[langselect]);
			break;
			case 1:
				sprintf(textbuffer, "%s", ntsc_options[ntscselect]);
			break;
			case 2:
				sprintf(textbuffer, "%s", pal60_options[pal60select]);
			break;
			case 3:
				sprintf(textbuffer, "%s", pal50_options[pal50select]);
			break;
			case 4:
				sprintf(textbuffer, "%s", hooktype_options[hooktypeselect]);
			break;
			case 5:
				sprintf(textbuffer, "%s", debugger_options[debuggerselect]);
			break;
			case 6:
				sprintf(textbuffer, "%s", filepatcher_options[filepatcherselect]);
			break;
			case 7:
				sprintf(textbuffer, "%s", ocarina_options[ocarinaselect]);
			break;
			case 8:
				sprintf(textbuffer, "%s", pausedstart_options[pausedstartselect]);
			break;
			case 9:
				sprintf(textbuffer, "%s", videomodespatch[videomodespatchselect]);
			break;
			case 10:
				sprintf(textbuffer, "%s", countrystring_options[countrystringselect]);
			break;
			case 11:
				sprintf(textbuffer, "%s", aspectratiopatch[aspectratioselect]);
			break;
		}
		if(configmenu[i].selected == 1) {
			printf("\n==> %s %s", configmenu[i].citems, textbuffer);
		}
		else {
			printf("\n    %s %s", configmenu[i].citems, textbuffer);
		}
	}
		
	printf("\n\n    Press B to return to menu");

	if(error_video) {
		printf("\n\n    Can't force two video modes");
		error_video = 0;
		sleep(1);
	}
	
	if(error_video_patch) {
		printf("\n\n    Can't force and patch video modes");
		error_video_patch = 0;
		sleep(1);	
	}

	if(error_sd) {
		printf("\n\n    No SD card found");
		error_sd = 0;
		sleep(1);
	}
	
	if(error_geckopaused) {
		printf("\n\n    Debugger is off, can't pause Gecko");
		error_geckopaused = 0;
		sleep(1);	
	}

	if(confirm_sd) {
		printf("\n\n    Config saved");
		confirm_sd = 0;
		sleep(1);
	}

	if(config_not_loaded == 0) {
		menu_load_config();
		config_not_loaded = 1;
	}
}

static void menu_drawrebooterconf()
{
	int i;
	char textbuffer[255];
	
	resetscreen();
	menu_pad_rebooterconf();
	
	for(i=0;i<rebooterconf_menu_items;i++)
	{
		memset(textbuffer, 0, 255);
		switch (i)
		{
			case 0:
				sprintf(textbuffer, "%s", recovery_options[recoveryselect]);
			break;
			case 1:
				sprintf(textbuffer, "%s", regionfree_options[regionfreeselect]);
			break;
			case 2:
				sprintf(textbuffer, "%s", nocopy_options[nocopyselect]);
			break;
			case 3:
				sprintf(textbuffer, "%s", buttonskip_options[buttonskipselect]);
			break;
		}	
		if(rebooterconfmenu[i].selected == 1) {
			printf("\n==> %s %s", rebooterconfmenu[i].rcitems, textbuffer);
		}
		else {
			printf("\n    %s %s", rebooterconfmenu[i].rcitems, textbuffer);
		}
	}

	printf("\n\n    Press B to return to menu");

	if(error_sd) {
		printf("\n\n    No SD card found");
		error_sd = 0;
		sleep(1);
	}

	if(confirm_sd) {
		printf("\n\n    Config saved");
		confirm_sd = 0;
		sleep(1);
	}
	
	if(config_not_loaded == 0) {
		menu_load_config();
		config_not_loaded = 1;
	}
}

static void menu_drawnochannellist() {
	
	resetscreen();
	
	if (channellist_menu_items == 0)
		printf("\n    There is no channel in the list");
	
	printf("\n    Return to main menu...");
	
	sleep(2);
	menu_number = 0;
}

static void menu_drawchannellist() {
	
	int i;
	
	resetscreen();
	menu_pad_channellist();
	
	for(i=channellist_menu_top;i<((channellist_menu_top+11 > channellist_menu_items) ? channellist_menu_items : channellist_menu_top+11);i++)
	{
		if(channellistmenu[i].selected == 1){
			printf("\n==> %s", channellistmenu[i].clitems);
		}
		else {
			printf("\n    %s", channellistmenu[i].clitems);
		}
	}
	
	printf("\n\n\n\n");
	printf("\n\n    Press B to Return");
}

static void menu_drawrebooter()
{
	resetscreen();

	if(config_bytes[2] == 0 || recoveryselect == 1)
		printf("\n    Rebooting without Hook");
	else
		printf("\n    Rebooting with Hook: VBI");

	sleep(2);
	runmenunow = 1;
}

static void menu_drawdisc()
{
	resetscreen();

	switch (config_bytes[2])
	{
		case 0:
			printf("\n    Launch Game without Hook");
		break;
	
		case 1:
			printf("\n    Launch Game with Hook: VBI");
		break;

		case 2:
			printf("\n    Launch Game with Hook: Wii Pad");
		break;
		
		case 3:
			printf("\n    Launch Game with Hook: GC Pad");
		break;
		
		case 4:
			printf("\n    Launch Game with Hook: GXDraw");
		break;
		
		case 5:
			printf("\n    Launch Game with Hook: GXFlush");
		break;
		
		case 6:
			printf("\n    Launch Game with Hook: OSSleepThread");
		break;
		
		case 7:
			printf("\n    Launch Game with Hook: AXNextFrame");
		break;	
	}
	
	rundiscnow = 1;
}

static void menu_drawnodownloadlist() {
	
	resetscreen();
	
	if (downloadlist_menu_items == 0)
		printf("\n    There is no download items:");
	if (!disc)
		printf("\n    There is no disc");
	if (channellist_menu_items == 0)
		printf("\n    There is no channel in the list");
	
	printf("\n    Return to main menu...");
	
	sleep(2);
	menu_number = 0;
}

static void menu_drawdownloadlist() {
	
	int i;
	
	resetscreen();
	menu_pad_downloadlist();
	
	for(i=downloadlist_menu_top;i<((downloadlist_menu_top+11 > downloadlist_menu_items) ? downloadlist_menu_items : downloadlist_menu_top+11);i++)
	{
		if(downloadlistmenu[i].selected == 1){
			printf("\n==> %s", downloadlistmenu[i].dlitems);
		}
		else {
			printf("\n    %s", downloadlistmenu[i].dlitems);
		}
	}
	
	printf("\n\n\n\n");
	printf("\n\n    Press B to Return");
}

static void menu_drawcodelist() {
	
	int i;
	
	resetscreen();
	printf("\n    Game ID: %s", downloadtoload);
	menu_pad_codelist();
	
	for(i=codelist_menu_top;i<((codelist_menu_top+11 > codelist_menu_items) ? codelist_menu_items : codelist_menu_top+11);i++)
	{
		if(codelistmenu[i].selected == 1){
			printf("\n==> %d [%c] %s", i+1, codelistmenu[i].flag, codelistmenu[i].citems);
		}
		else {
			printf("\n    %d [%c] %s", i+1, codelistmenu[i].flag, codelistmenu[i].citems);
		}
	}
	
	printf("\n\n\n\n");
	printf("\n\n    Press Home to Create GCT File, or Press B to Return");
}

static void menu_drawnocodelist() {
	
	resetscreen();
	printf("\n    Game ID: %s", downloadtoload);
	
	if (codelist_menu_items == 0)
		printf("\n    There is no downloaded code");
	
	printf("\n    Return to main menu...");
	
	sleep(2);
	menu_number = 0;
}

static void menu_drawupdatechtgct() {

	u32 wcodes;

	resetscreen();
	
	wcodes = menu_updatechtgct(downloadtoload);
	if (wcodes > 0)
		printf("\n    Created %s.gct file for %d code(s)", downloadtoload, wcodes);
	sleep(2);
	menu_number = 40;
}

static void menu_drawcodeline() {
	
	int i;
	
	resetscreeneditheader(namecodeline, downloadtoload, codelineflag);
	menu_pad_codeline();
	
	printf("\n\n         01234567 89ABCDEF");
	printf("\n         ");
	for(i=0;i<codeline_col;i++) printf(" ");
	printf("*");
	for(i=codeline_menu_top;i<((codeline_menu_top+6 > codeline_menu_items) ? codeline_menu_items : codeline_menu_top+6);i++)
	{
		if(codelinemenu[i].selected == 1){
			printf("\n==> %4d %s", codelinemenu[i].id+1, codelinemenu[i].litems);
		}
		else {
			printf("\n    %4d %s", codelinemenu[i].id+1, codelinemenu[i].litems);
		}
	}
	
	resetscreeneditfooter(notecodeline);
	printf("\n\n    Press A to Save Any Edits, or Press B to Return");
}

void menu_draw() {	
	
	switch (menu_number)
	{
		case 0:
			menu_drawroot();
		break;

		case 1:
			menu_drawconfig();
		break;
		
		case 8:
			menu_drawdisc();
		break;
		
		case 12:
			menu_drawrebooter();
		break;
			
		case 13:
			menu_drawrebooterconf();
		break;
		
		case 14:
			menu_drawnochannellist();
		break;
			
		case 15:
			menu_drawchannellist();
		break;
		
		case 30:
			menu_drawnodownloadlist();
		break;
		
		case 31:
			menu_drawdownloadlist();
		break;
		
		case 40:
			menu_drawcodelist();
		break;
		
		case 41:
			menu_drawnocodelist();
		break;
		
		case 50:
			menu_drawcodeline();
		break;
		
		case 60:
			menu_drawupdatechtgct();
		break;
	}
}

void menu_drawbootdisc() {

	switch (codes_state)
	{
		case 0:
			break;
		case 1:
			printf("\n    Game ID: %s", gameidbuffer);
			printf("\n    No SD Codes Found");
			break;
		case 2:
			printf("\n    Game ID: %s", gameidbuffer);
			printf("\n    SD Codes Found. Applying");
			break;
		case 3:
			printf("\n    Game ID: %s", gameidbuffer);
			printf("\n    No Hook, Not Applying Codes");
			break;
		case 4:
			printf("\n    Game ID: %s", gameidbuffer);
			printf("\n    Codes Error: Too Many Codes");
			printf("\n    %u Lines Found, %u Lines Allowed", (codelistsize / 8) - 2, ((codelistend - codelist) / 8) - 2);
			break;
	}
	
	__io_wiisd.shutdown();
	sleep(2);
}

void menu_drawbootchannel() {
		
	resetscreen();

	switch (codes_state)
	{
		case 0:
			break;
		case 1:
			printf("\n    Channel ID: %s", gameidbuffer);
			printf("\n    No SD Codes Found");
			break;
		case 2:
			printf("\n    Channel ID: %s", gameidbuffer);
			printf("\n    SD Codes Found. Applying");
			break;
		case 3:
			printf("\n    Channel ID: %s", gameidbuffer);
			printf("\n    No Hook, Not Applying Codes");
			break;
		case 4:
			printf("\n    Channel ID: %s", gameidbuffer);
			printf("\n    Codes Error: Too Many Codes");
			printf("\n    %u Lines Found, %u Lines Allowed", (codelistsize / 8) - 2, ((codelistend - codelist) / 8) - 2);
			break;
	}
	
	__io_wiisd.shutdown();
	sleep(2);
}
	
static bool displaychan(u64 chantitle, u64 *title_list, u32 count) {
	u32 i;
	
	if ((chantitle & 0xff) == 0x41)
		for (i = 0; i < count; i++)
			if ((chantitle & 0xffffff00) == (title_list[i] & 0xffffff00) &&
				(title_list[i] & 0xff) != 0x41)
				return FALSE;
	if ((chantitle & 0xffffff00) >> 8 == 0x484141)
		for (i = 0; i < count; i++)
			if ((title_list[i] & 0xffffff00) >> 8 == 0x484159 &&
				(chantitle & 0xff) == (title_list[i] & 0xff))
				return FALSE;
	if (TITLE_UPPER(chantitle) == 0x00010001 || TITLE_UPPER(chantitle) == 0x00010002 ||
		TITLE_UPPER(chantitle) == 0x00010004)
		if ((chantitle & 0xffffff00) >> 8 != 0x484347)
			return TRUE;
	return FALSE;
}

static char* chanidtoname(char *chanid, u64 chantitle, char *menuname) {
	s32 ret, specialchar = 0; 
	u32 i, numletters = 0, numlettersmenu = 0;
	u8 addcolon = 0;
	char *newname;
	char *chanBanner, *namedata;
	
	if(ES_SetUID(chantitle) < 0)
	{
		newname = NULL;
		goto checkmenuname;
	}
	
	chanBanner = (char *)memalign(32, 256);
	if (chanBanner == NULL)
	{
		newname = NULL;
		goto checkmenuname;
	}
	memset(chanBanner, 0, 256);
	
	if(ES_GetDataDir(chantitle, chanBanner) < 0)
	{
		free(chanBanner);
		newname = NULL;
		goto checkmenuname;
	}
	
	namedata = (char *) memalign(32, 0x140);
	if (namedata == NULL)
	{
		free(chanBanner);
		newname = NULL;
		goto checkmenuname;
	}
	memset(namedata, 0, 0x140);
	
	strcat(chanBanner, "/banner.bin");
	
	ret = ISFS_Open(chanBanner, ISFS_OPEN_READ);
	if (ret < 0)
	{
		free(chanBanner);
		free(namedata);
		newname = NULL;
		goto checkmenuname;
	}
	
	if (ISFS_Read(ret, (u8 *) namedata, 0x140) < 0)
	{
		ISFS_Close(ret);
		free(chanBanner);
		free(namedata);
		newname = NULL;
		goto checkmenuname;
	}
	
	ISFS_Close(ret);
	
	newname = malloc(87);
	memset(newname, 0, 87);
	newname[79] = 0;
	for (i = 0; (i + specialchar) < 80 && (i < 40 || (addcolon == 2 && i < 80)); i++)
	{
		if (namedata[i * 2 + 0x20] != 0 || namedata[i * 2 + 0x21] != 0)
		{
			if (addcolon == 1 && (i + specialchar) < 77 && namedata[i * 2 + 0x20] == 0 && namedata[i * 2 + 0x21] >= 'A' && namedata[i * 2 + 0x21] <= 'Z')
			{
				addcolon = 2;
				newname[i+specialchar] = ':';
				specialchar++;
				newname[i+specialchar] = ' ';
				specialchar++;
			}
			else if (addcolon == 1)
				addcolon = 2;
			if (namedata[i * 2 + 0x20] == 0x21 && namedata[i * 2 + 0x21] == 0x61 && (i + specialchar) < 78)
			{
				newname[i+specialchar] = 'I';
				specialchar++;
				newname[i+specialchar] = 'I';
			}
			else if (namedata[i * 2 + 0x20] == 0 && namedata[i * 2 + 0x21] >= 32 && namedata[i * 2 + 0x21] <= 126)
				newname[i+specialchar] = namedata[i * 2 + 0x21];
			else
				specialchar--;
		}
		else
		{
			if (addcolon != 2)
			{
				addcolon = 1;
				specialchar--;
			}
			else
			{
				newname[i+specialchar] = 0;
				break;
			}
		}
	}
	newname[80+specialchar] = 0;
	
	free(chanBanner);
	free(namedata);
	
	for (i = 0; i < 50; i++)
	{
		if ((newname[i] >= 'A' && newname[i] <= 'Z') ||
			(newname[i] >= 'a' && newname[i] <= 'z'))
			numletters++;
		else if (newname[i] == 0)
			break;
	}
	
checkmenuname:
	
	if (menuname != NULL)
	{
		for (i = 0; i < 50; i++)
		{
			if ((menuname[i] >= 'A' && menuname[i] <= 'Z') ||
				(menuname[i] >= 'a' && menuname[i] <= 'z'))
				numlettersmenu++;
			else if (menuname[i] == 0)
				break;
		}
	}
	
	if (numletters > numlettersmenu && numletters >= 3)
	{
		if (menuname != NULL) free(menuname);
		goto addregion;
	}
	else if (numlettersmenu >= 3)
	{
		if (newname != NULL) free(newname);
		newname = menuname;
		goto addregion;
	}
	
	if (newname != NULL) free(newname);
	if (menuname != NULL) free(menuname);
	return chanid;
	
addregion:
	i = strlen(newname);
	while (i > 0 && newname[i-1] == ' ') i--;
	newname[i] = 0;
	if (chanid[3] == 'J')
		strcat(newname, " (JP)");
	else if (chanid[3] == 'P')
		strcat(newname, " (EU)");
	else if (chanid[3] == 'E')
		strcat(newname, " (US)");
	else if (chanid[3] == 'D')
		strcat(newname, " (DE)");
	else if (chanid[3] == 'F')
		strcat(newname, " (FR)");
	else
		sprintf(newname, "%s (%c)", newname, chanid[3]);
	free(chanid);
	return newname;
}

#define rets(a,m,t,x)	(a==m)?x:t
#define retm(a,s,e,t,x)	(a>=s && a<=e)?x:t

u8 mapextascii(u8 ascii)
{
	u8 temp;
	
	temp = ascii;
	temp = rets(ascii,138,temp,83);
	temp = retm(ascii,145,146,temp,39);
	temp = retm(ascii,147,148,temp,34);
	temp = rets(ascii,154,temp,115);
	temp = rets(ascii,158,temp,122);
	temp = rets(ascii,159,temp,89);
	temp = retm(ascii,192,197,temp,65);
	temp = rets(ascii,199,temp,67);
	temp = retm(ascii,200,203,temp,69);
	temp = retm(ascii,204,207,temp,73);
	temp = rets(ascii,209,temp,78);	
	temp = retm(ascii,210,214,temp,79);
	temp = rets(ascii,216,temp,79);	
	temp = retm(ascii,217,220,temp,85);
	temp = rets(ascii,221,temp,89);	
	temp = retm(ascii,224,229,temp,97);
	temp = rets(ascii,231,temp,99);	
	temp = retm(ascii,232,235,temp,101);		
	temp = retm(ascii,236,239,temp,105);
	temp = rets(ascii,241,temp,110);
	temp = retm(ascii,242,246,temp,111);
	temp = rets(ascii,248,temp,111);
	temp = retm(ascii,249,252,temp,117);
	temp = rets(ascii,253,temp,121);
	temp = rets(ascii,255,temp,121);
	
	return temp;	
}

static int identify_disc() {
        
	char readbuf[2048] __attribute__((aligned(32)));
		
    memset(&internalName[0],0,512);
    // Read the header
    DVD_LowRead64(readbuf, 2048, 0ULL);
    if (readbuf[0]) {
        strncpy(&gameName[0], readbuf, 6);
        gameName[6] = 0;
        // Multi Disc identifier support
        if (readbuf[6]) {
            sprintf(&gameName[0], "%s-disc%i", &gameName[0],
                (readbuf[6]) + 1);
        }
        strncpy(&internalName[0],&readbuf[32],512);
    } else {
        sprintf(&gameName[0], "disc%i", dumpCounter);
    }
	
	if ((*(volatile unsigned int*) (readbuf+0x1C)) == NGC_MAGIC) {
        return IS_NGC_DISC;
    }
    if ((*(volatile unsigned int*) (readbuf+0x18)) == WII_MAGIC) {
        return IS_WII_DISC;
    } else {
        return IS_UNK_DISC;
    }
}

s32 menu_generatedownloadlist() {
	
	int retd, disc_type;
	downloadlistitems *tempitem = NULL;
	channellistitems *tempchan = NULL;
	char truncatestring[4] = "...";
	u16 i = 0;
	u64 chantitle;
	char displayName[512];
	
	retd = init_dvd();
	wifi_printf("menu_menu_generatedownloadlist: retd value = %d\n", retd);
	if (retd == NO_DISC) 
		disc = false;
		
	if (disc) 
	{
		memset(gameName, 0, 32);
		memset(internalName, 0, 512);
		disc_type = identify_disc();
		wifi_printf("menu_menu_generatedownloadlist: disc_type value = %d\n", disc_type);
		dvd_motor_off();
		wifi_printf("menu_menu_generatedownloadlist: gameName value = %s\n%s\n", 
			gameName, internalName);
		
		if (strlen(internalName) > 36)
			memcpy(internalName+33,truncatestring,4);

		tempitem = &downloadlistmenu[downloadlist_menu_items];
		tempitem->dlitems = (char *)malloc(strlen(internalName)+
				strlen(gameName)+20);
		switch (disc_type)
		{
			case IS_WII_DISC:
				sprintf(displayName,"{Wii Disc}-[%s] %s", gameName, internalName);
			break;
			case IS_NGC_DISC:
				sprintf(displayName,"{GC Disc}-[%s] %s", gameName, internalName);
			break;
			case IS_UNK_DISC:
				sprintf(displayName,"{Unknown Disc}-[%s] %s", gameName, internalName);
			break;
		}
		memcpy(tempitem->dlitems, displayName, strlen(displayName)+1);
		tempitem->selected = 0;
		memcpy(tempitem->gameidbuffer, gameName, 8);
		wifi_printf("menu_menu_generatedownloadlist:\ndisc gameid value = %s\nselected value = %d\ndlitems = %s\n",
			downloadlistmenu[downloadlist_menu_items].gameidbuffer, downloadlistmenu[downloadlist_menu_items].selected,
			downloadlistmenu[downloadlist_menu_items].dlitems);
		downloadlist_menu_items++;
	}
	
	if (channellist_menu_items == 0)
		if (!menu_generatechannellist())
			wifi_printf("menu_menu_generatedownloadlist: menu_generatechannellist() = 0\n");

	while (i < channellist_menu_items)
	{
		memset(gameName, 0, 32);
		memset(internalName, 0, 512);
		tempitem = &downloadlistmenu[downloadlist_menu_items];
		tempchan = &channellistmenu[i];
		memcpy(internalName, tempchan->clitems, strlen(tempchan->clitems));
		if (strlen(internalName) > 36)
			memcpy(internalName+33,truncatestring,4);
		chantitle = tempchan->titleid;
		gameName[0] = (chantitle & 0xff000000) >> 24;
		gameName[1] = (chantitle & 0x00ff0000) >> 16;
		gameName[2] = (chantitle & 0x0000ff00) >> 8;
		gameName[3] = chantitle & 0x000000ff;
		gameName[4] = 0;
		tempitem->dlitems = (char *)malloc(strlen(internalName)+
				strlen(gameName)+20);
		sprintf(displayName,"{Channel}-[%s] %s", gameName, internalName);
		memcpy(tempitem->dlitems, displayName, strlen(displayName)+1);
		tempitem->selected = 0;
		memcpy(tempitem->gameidbuffer, gameName, 8);
		
		downloadlist_menu_items++;
		i++;
	}
	
	wifi_printf("menu_menu_generatedownloadlist: downloadlist_menu_items value = %d\n",
		downloadlist_menu_items);

	return 1;
}

s32 menu_generatechannellist() {

	s32 ret, specialchar;
	u32 count = 0, removecount = 0;
	u8 addcolon;
	static u8 tmd_buf[MAX_SIGNED_TMD_SIZE] ATTRIBUTE_ALIGN(32);
	static u64 title_list[256] ATTRIBUTE_ALIGN(32);
	u32 contentbannerlist[256];
	signed_blob *s_tmd;
	u16 bootindex;
	u32 tmd_size, bootid, i, j;
	u64 chantitle;
	char *channames[256], *databuf;
	char *filename;
	u8 tempdatabuf;
	
	if (ISFS_Initialize())
		return 0;
	
	channellist_menu_items = 0;
	
	ret = ES_GetNumTitles(&count);
	if (ret) {
		return 0;
	}
	
	ret = ES_GetTitles(title_list, count);
	if (ret) {
		return 0;
	}
	
	filename = (char *)memalign(32, 256);
	if (filename == NULL) return 0;
	
	for (i=0; i < count; i++)
	{
		ret = ES_GetStoredTMDSize(title_list[i], &tmd_size);
		s_tmd = (signed_blob *)tmd_buf;
		ret = ES_GetStoredTMD(title_list[i], s_tmd, tmd_size);
		bootindex = ((tmd *)SIGNATURE_PAYLOAD(s_tmd))->boot_index;
		for (j = 0; j < ((tmd *)SIGNATURE_PAYLOAD(s_tmd))->num_contents; j++)
		{
			if (((tmd *)SIGNATURE_PAYLOAD(s_tmd))->contents[j].index == 0)
				contentbannerlist[i] = ((tmd *)SIGNATURE_PAYLOAD(s_tmd))->contents[j].cid;
			if (((tmd *)SIGNATURE_PAYLOAD(s_tmd))->contents[j].index == bootindex)
				bootid = ((tmd *)SIGNATURE_PAYLOAD(s_tmd))->contents[j].cid;
		}
		snprintf(filename, 256, "/title/%08x/%08x/content/%08x.app", TITLE_UPPER(title_list[i]), TITLE_LOWER(title_list[i]), bootid);
		ret = ISFS_Open(filename, ISFS_OPEN_READ);
		if (ret >= 0)
		{
			contentbannerlist[i-removecount] = contentbannerlist[i];
			title_list[i-removecount] = title_list[i];
			ISFS_Close(ret);
		}
		else
			removecount++;
	}
	count -= removecount;
	
	databuf = (char *) memalign(32, 0x140);
	
	for (i=0; i < count; i++)
	{
		chantitle = title_list[i];
		snprintf(filename, 256, "/title/%08x/%08x/content/%08x.app", TITLE_UPPER(chantitle), TITLE_LOWER(chantitle), contentbannerlist[i]);
		if (databuf == NULL)
			channames[i] = NULL;
		else
		{
			ret = ISFS_Open(filename, ISFS_OPEN_READ);
			if (ret < 0)
				channames[i] = NULL;
			else
			{
				if (ISFS_Read(ret, (u8 *) databuf, 0x140) < 0)
				{
					ISFS_Close(ret);
					free(databuf);
					channames[i] = NULL;
				}
				else
				{
					ISFS_Close(ret);
					channames[i] = (char *) malloc(87);
					memset(channames[i], 0, 87);
					if (channames[i] != NULL)
					{
						(channames[i])[79] = 0;
						specialchar = 0;
						addcolon = 0;
						for (j = 0; (j + specialchar) < 80 && (j < 40 || (addcolon == 2 && j < 80)); j++)
						{
							if (databuf[j * 2 + 0xF0] != 0 || databuf[j * 2 + 0xF1] != 0)
							{
								if (addcolon == 1 && (j + specialchar) < 77 && databuf[j * 2 + 0xF0] == 0 && databuf[j * 2 + 0xF1] >= 'A' && databuf[j * 2 + 0xF1] <= 'Z')
								{
									addcolon = 2;
									(channames[i])[j+specialchar] = ':';
									specialchar++;
									(channames[i])[j+specialchar] = ' ';
									specialchar++;
								}
								else if (addcolon == 1)
									addcolon = 2;
								tempdatabuf = mapextascii((u8) databuf[j * 2 + 0xF1]);
								if (databuf[j * 2 + 0xF0] == 0x21 && databuf[j * 2 + 0xF1] == 0x61 && (j + specialchar) < 78)
								{
									(channames[i])[j+specialchar] = 'I';
									specialchar++;
									(channames[i])[j+specialchar] = 'I';
								}
								else if (databuf[j * 2 + 0xF0] == 0 && 
											(
												(tempdatabuf >= 32 && tempdatabuf <= 126)
											))
									(channames[i])[j+specialchar] = tempdatabuf;
								else
									specialchar--;
							}
							else
							{
								if (addcolon != 2)
								{
									addcolon = 1;
									specialchar--;
								}
								else
								{
									(channames[i])[j+specialchar] = 0;
									break;
								}
							}
						}
						(channames[i])[80+specialchar] = 0;
					}
				}
			}
		}
	}
	
	free(filename);
	if (databuf != NULL) free(databuf);
	ISFS_Deinitialize();

	ISFS_Initialize();
	
	for (i=0; i < count; i++) {
		chantitle = title_list[i];
		wifi_printf("menu_menu_generatechannellist: channel[%d] = %s, titleid = %016llX\n", i+1, 
			channames[i], chantitle);
		if (displaychan(chantitle, title_list, count))
		{
			channellistmenu[channellist_menu_items].titleid = chantitle;
			channellistmenu[channellist_menu_items].selected = 0;
			channellistmenu[channellist_menu_items].clitems = malloc(5);
			*(channellistmenu[channellist_menu_items].clitems) = (chantitle & 0xff000000) >> 24;
			*(channellistmenu[channellist_menu_items].clitems + 1) = (chantitle & 0x00ff0000) >> 16;
			*(channellistmenu[channellist_menu_items].clitems + 2) = (chantitle & 0x0000ff00) >> 8;
			*(channellistmenu[channellist_menu_items].clitems + 3) = chantitle & 0x000000ff;
			*(channellistmenu[channellist_menu_items].clitems + 4) = 0;
			channellistmenu[channellist_menu_items].clitems = chanidtoname(channellistmenu[channellist_menu_items].clitems, chantitle, channames[i]);
			channellist_menu_items++;
		}
		else
			if (channames[i] != NULL) free(channames[i]);
	}
	
	ISFS_Deinitialize();
	wifi_printf("menu_menu_generatechannellist: number of displayed channel = %d\n", channellist_menu_items);
	int starti = 0;
	for (i=starti; i < channellist_menu_items; i++) {
		wifi_printf("menu_menu_generatechannellist: displayed channel[%d] = %s, titleid = %016llX\n", i+1, 
			channellistmenu[i].clitems, channellistmenu[i].titleid);
	}
		
	return 1;
}

s32 menu_generatecodelist() {

	char flag;
	char name[64];
	u32 namesize = 0;
	struct CodeNode *temp = NULL;
	codelistitems *tempitem = NULL;
	char truncatestring[4] = "...";
	
	temp = head;
	if (temp->next == NULL)
		return 0;
	while (temp->next != NULL)
	{
		temp = temp->next;
		flag = temp->flag;
		memset(name,0,60);
		namesize = temp->namesize;
		if (namesize > 60) {
			memcpy(name,temp->name,60);
			memcpy(name+57,truncatestring,4);
		} else {
			memcpy(name,temp->name,namesize);
		}

		tempitem = &codelistmenu[codelist_menu_items];
		tempitem->id = codelist_menu_items;
		tempitem->flag = flag;
		tempitem->citems = (char *)malloc(strlen(name)+1);
		memcpy(tempitem->citems, name, strlen(name)+1);
		codelist_menu_items++;
	}
	wifi_printf("menu_menu_generatecodelist: codelist_menu_items = %d\n",
		codelist_menu_items);

	return 1;
}

u32 menu_updatechtgct(char *gameidbuffer) {

	char flag;
	char *code;
	char coderow[16];
	struct CodeNode *temp = NULL;
	codelistitems *tempitem = NULL;
	u64 header = 0x00D0C0DE00D0C0DEULL;
	u64 footer = 0xF000000000000000ULL;
	u64 code64 = 0x0000000000000000ULL;
	int i = 0, j = 0, nlines = 0;
	u32 wcodes = 0, codesize = 0;
	
	FILE* fp;
	char filepath[MAX_FILEPATH_LEN];
	
	if (sd_found != 1) {
		printf("\n    Cannot find SD card");
		sleep(2);
		return 0;
	}
	
	DIR *pdir = opendir ("/codes/");
	if(pdir == NULL) {
		printf("\n    Creating /codes/ folder...");
		if(mkdir ("/codes",0777) == -1) {
			printf("Failed");
			sleep(2);
			return 0;
		} else {
			printf("Done");
			pdir = opendir ("/codes/");
		}
	}
	
	closedir(pdir);
	fflush(stdout);
	
	sprintf(filepath, CODEDIR "/%s.gct", gameidbuffer);
		
	fp = fopen(filepath, "rb");
	if (!fp) { 
		printf("\n    Creating %s.gct file...", gameidbuffer); 
	} else {
		printf("\n    Overwriting %s.gct file...", gameidbuffer);
		fclose(fp);
	}
	
	fp = fopen(filepath, "wb");
	if (!fp) {
		printf("\n    Cannot write %s.gct file", gameidbuffer);
		sleep(2);
		return 0;
	}
	fwrite(&header, sizeof(header), 1, fp);
	
	temp = head;
	i = 0;
	wcodes = 0;
	while (i < codelist_menu_items)
	{
		if (temp->next == NULL)
			break;
		temp = temp->next;
		tempitem = &codelistmenu[i++];
		flag = tempitem->flag;
		if (flag == '+')
		{
			codesize = temp->codesize;
			code = (char *)malloc(codesize);
			memcpy(code,temp->code,codesize);
			if (checkflag(code, codesize) != '?')
			{
				temp->flag = '+';
				nlines = (int) codesize / 0x12;
				for (j = 0; j < nlines; j++)
				{
					memset(coderow, 0, 16);
					memcpy(coderow, code+(j*0x12), 8);
					memcpy(coderow+8, code+(j*0x12+9), 8);
					sscanf(coderow,"%016llX", &code64);
					fwrite(&code64, sizeof(code64), 1, fp);
				}
			}
			wcodes++;
		}
	}
	fwrite(&footer, sizeof(footer), 1, fp);
	fclose(fp);
	wifi_printf("menu_menu_updatechtgct: write %d codes to %s.gct file\n",
		wcodes, gameidbuffer);
	s32 retval = sd_write_cht(head);
	wifi_printf("menu_menu_updatechtgct: retval value = %d\n", retval);
	
	return wcodes;
}


s32 menu_generatecodeline(int selected) {

	u16 id, i;
	u32 namesize = 0;
	u32 codesize = 0;
	u32 notesize = 0;
	struct CodeNode *temp = NULL;
	char *code = NULL;
	codelistitems *tempitem = NULL;
	codelineitems *templine = NULL;
	char truncatestring[4] = "...";
	int nlines = 0, j = 0;
	char *val;
	
	tempitem = &codelistmenu[selected];
	id = tempitem->id + 1;
	codelineflag = tempitem->flag;
	i = 0;
	temp = head;
	if (temp->next == NULL)
		return 0;
	while (temp->next != NULL && i < id)
	{
		temp = temp->next;
		i++;
	}
	
	namesize = temp->namesize;
	memset(namecodeline, 0, 80);
	if (namesize > 72) {
		memcpy(namecodeline, temp->name, 72);
		memcpy(namecodeline+69, truncatestring, 4);
	} else {
		memcpy(namecodeline,temp->name,namesize);
	}
	while((val=strchr(namecodeline,'\n')) != NULL) *val=' ';
	
	notesize = temp->notesize;
	memset(notecodeline, 0, 512);
	if (notesize > 510) {
		memcpy(notecodeline, temp->note, 510);
		memcpy(notecodeline+507, truncatestring, 4);
	} else {
		memcpy(notecodeline,temp->note,notesize);
	}	
	while((val=strchr(notecodeline,'\n')) != NULL) *val=' ';

	codesize = temp->codesize;
	code = (char *)malloc(codesize);
	memcpy(code,temp->code,codesize);
	nlines = (int) codesize / 0x12;
	codeline_menu_items = 0;
	for (j = 0; j < nlines; j++)
	{
		templine = &codelinemenu[codeline_menu_items++];
		templine->id = j;
		memcpy(templine->litems, code+(j*0x12), 0x12);
		templine->litems[0x11] = 0;
		templine->selected = 0;
		templine->col = 0;
	}

	free(code);
	return 1;
}

