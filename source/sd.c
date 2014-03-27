
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <fat.h>
#include <sys/unistd.h>
#include <sys/dir.h>
#include <ogc/exi.h>
#include <sdcard/wiisd_io.h>

#include "sd.h"
#include "menu.h"
#include "gecko.h"
#include "wifi_gecko.h"
#include "launch.h"
#include "defaultgameconfig.h"

#define PATCHDIR		"sd:/patch"
#define GECKOPATCHDIR	"sd:/data/gecko/patch"
#define GAMECONFIG		"sd:/gameconfig.txt"
#define GECKOGAMECONFIG	"sd:/data/gecko/gameconfig.txt"

u32 sd_found = 0;

int codes_state = 0;
u8 *tempcodelist = (u8 *) sdbuffer;
u8 *codelist = NULL;
u8 *codelistend = (u8 *) 0x80003000;
u32 codelistsize = 0;

int patch_state = 0;
char *tempgameconf = (char *) sdbuffer;
u32 *gameconf = NULL;
u32 tempgameconfsize = 0;
u32 gameconfsize = 0;

u8 configwarn = 0;

void sd_refresh() {
	
	s32 ret = 0;
	
	if (!__io_wiisd.startup()) {
		wifi_printf("sd_sd_refresh: SD card error\n");
	}
	else {
		ret = sd_init();
		if(ret){
			sd_found = 1;
		}
		else {
			sd_found = 0;
		}
		sd_load_config();
	}
}

u32 sd_init() {
	
	return fatInitDefault();
}

u32 sd_load_config() {
	
	char filepath[MAX_FILEPATH_LEN];
	int ret = 0;
	FILE *fp = NULL;
	
	// Set defaults, will get ovewritten if wiilauncherconfig.dat exists
	config_bytes[0] = 0xCD; // language
	config_bytes[1] = 0x00; // video mode
	config_bytes[2] = 0x01; // hook type
	config_bytes[3] = 0x00; // file patcher
	config_bytes[5] = 0x00;	// no paused start
	config_bytes[6] = 0x00; // gecko slot b	
	config_bytes[8] = 0x00; // recovery hook
	config_bytes[9] = 0x00; // region free
	config_bytes[10] = 0x00; // no copy
	config_bytes[11] = 0x00; // button skip
	config_bytes[12] = 0x00; // video modes patch
	config_bytes[13] = 0x00; // country string patch
	config_bytes[14] = 0x00; // aspect ratio patch
	config_bytes[15] = 0x00; // reserved
	if (geckoattached)
	{
		config_bytes[4] = 0x00; // cheat code
		config_bytes[7] = 0x01; // debugger
	}
	else
	{
		config_bytes[4] = 0x01;
		config_bytes[7] = 0x00;
	}
	
	if (sd_found == 0)
		return 0;
	
	sprintf (filepath, "sd:/data/gecko/wiilauncherconfig.dat");
	fp = fopen(filepath, "rb");
	if (!fp){
		sprintf (filepath, "sd:/wiilauncherconfig.dat");
		fp = fopen(filepath, "rb");
		if (!fp){
			return 0;
		}
	};

	ret = fread(config_bytes, 1, no_config_bytes, fp);
	DCFlushRange(config_bytes, no_config_bytes);

	if(ret != no_config_bytes){
		fclose(fp);
		return 0;
	}

	fclose(fp);
	return 1;
}

u32 sd_save_config() {

	char filepath[MAX_FILEPATH_LEN];
	int ret;
	
	sprintf (filepath, "sd:/wiilauncherconfig.dat");
	FILE *fp = fopen(filepath, "wb");
	if (!fp){
		return 0;
	};

	ret = fwrite(config_bytes, 1, no_config_bytes, fp);
	DCFlushRange(config_bytes, no_config_bytes);
	wifi_printf("sd_sd_save_config: number of config_bytes saved = %d\n", ret);

	if(ret != no_config_bytes){
		fflush(fp);
		fclose(fp);
		return 0;
	}

	fflush(fp);
	fclose(fp);

	return 1;
}

void sd_copy_codes(char *filename) {
	
	if (config_bytes[2] == 0x00)
	{
		codes_state = 3;
		return;
	}
	
	FILE* fp;
	u32 ret;
	u32 filesize;
	char filepath[MAX_FILEPATH_LEN];
	
	DIR *pdir = opendir ("/data/gecko/codes/");
	if(pdir == NULL){
		pdir = opendir ("/codes/");
		if(pdir == NULL){
			codes_state = 1;	// dir not found
			return;
		}
	}
	
	closedir(pdir);
	fflush(stdout);
	
	sprintf(filepath, GECKOCODEDIR "/%s.gct", filename);
	
	fp = fopen(filepath, "rb");
	if (!fp) {
		sprintf(filepath, CODEDIR "/%s.gct", filename);
		
		fp = fopen(filepath, "rb");
		if (!fp) {
			codes_state = 1;	// codes not found
			return;
		}
	}
	
	fseek(fp, 0, SEEK_END);
    filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
	
	codelistsize = filesize;
	wifi_printf("sd_sd_copy_codes: codelistsize value = %08X\n", codelistsize);
	wifi_printf("sd_sd_copy_codes: (codelist + codelistsize) value = %08X\n", (codelist + codelistsize));
	wifi_printf("sd_sd_copy_codes: codelistend value = %08X\n", codelistend);
	
	if ((codelist + codelistsize) > codelistend)
	{
		fclose(fp);
		codes_state = 4;
		return;
	}
	
	ret = fread((void*)tempcodelist, 1, filesize, fp);
	if(ret != filesize){	
		fclose(fp);
		codelistsize = 0;
		codes_state = 1;	// codes not found
		return;
	}
	
	fclose(fp);
	DCFlushRange((void*)tempcodelist, filesize);
	
	codes_state = 2;
}

void sd_copy_patch(char *filename) {
	
	FILE* fp;
	u32 ret;
	u32 filesize;
	char filepath[MAX_FILEPATH_LEN];

	DIR *pdir = opendir ("/data/gecko/patch/");
	if(pdir == NULL){
		pdir = opendir ("/patch/");
		if(pdir == NULL){
			codes_state = 0;
			return;
		}
	}

	closedir(pdir);
	fflush(stdout);
	
	sprintf(filepath, GECKOPATCHDIR "/%s.gpf", filename);
	
	fp = fopen(filepath, "rb");
	if (!fp) {
		sprintf(filepath, PATCHDIR "/%s.gpf", filename);		
		fp = fopen(filepath, "rb");
		if (!fp) {
			codes_state = 0;
			return;
		}
	}

	fseek(fp, 0, SEEK_END);
    filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
	
	wifi_printf("sd_sd_copy_patch: filesize value = %08X\n", filesize);

	ret = fread((void*)sdbuffer, 1, filesize, fp);
	if(ret != filesize){	
		fclose(fp);
		codes_state = 0;
		return;
	}
	tempcodelist += filesize;
	tempgameconf = (char *)tempcodelist;

	DCFlushRange((void*)sdbuffer, filesize);
	patch_state = 1;
}

void sd_copy_gameconfig(char *gameid) {
	
	FILE* fp;
	u32 ret;
	u32 filesize = 0;
	s32 gameidmatch, maxgameidmatch = -1, maxgameidmatch2 = -1;
	u32 i, j, numnonascii, parsebufpos;
	u32 codeaddr, codeval, codeaddr2, codeval2, codeoffset;
	u32 temp, tempoffset;
	char parsebuffer[18];
		
	memcpy(tempgameconf, defaultgameconfig, defaultgameconfig_size);
	tempgameconf[defaultgameconfig_size] = '\n';
	tempgameconfsize = defaultgameconfig_size + 1;
	
	wifi_printf("sd_sd_copy_gameconfig: defaultgameconfig_size value = %08X\n", defaultgameconfig_size);
	wifi_printf("sd_sd_copy_gameconfig: tempgameconfsize value = %08X\n", tempgameconfsize);
	
	if (sd_found == 1)
	{
		fp = fopen(GECKOGAMECONFIG, "rb");
		
		if (!fp) fp = fopen(GAMECONFIG, "rb");
		
		if (fp) {
			fseek(fp, 0, SEEK_END);
			filesize = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			
			ret = fread((void*)tempgameconf + tempgameconfsize, 1, filesize, fp);
			fclose(fp);
			if (ret == filesize)
				tempgameconfsize += filesize;
		}
	}
	
	wifi_printf("sd_sd_copy_gameconfig: filesize value = %08X\n", filesize);
	wifi_printf("sd_sd_copy_gameconfig: tempgameconfsize value = %08X\n", tempgameconfsize);
	
	// Remove non-ASCII characters
	numnonascii = 0;
	for (i = 0; i < tempgameconfsize; i++)
	{
		if (tempgameconf[i] < 9 || tempgameconf[i] > 126)
			numnonascii++;
		else
			tempgameconf[i-numnonascii] = tempgameconf[i];
	}
	tempgameconfsize -= numnonascii;
	
	wifi_printf("sd_sd_copy_gameconfig: tempgameconfsize value = %08X\n", tempgameconfsize);
	
	*(tempgameconf + tempgameconfsize) = 0;
	gameconf = (u32 *)((tempgameconf + tempgameconfsize) + (4 - (((u32) (tempgameconf + tempgameconfsize)) % 4)));
	
	for (maxgameidmatch = 0; maxgameidmatch <= 6; maxgameidmatch++)
	{
		i = 0;
		while (i < tempgameconfsize)
		{
			maxgameidmatch2 = -1;
			while (maxgameidmatch != maxgameidmatch2)
			{
				while (i != tempgameconfsize && tempgameconf[i] != ':') i++;
				if (i == tempgameconfsize) break;
				while ((tempgameconf[i] != 10 && tempgameconf[i] != 13) && (i != 0)) i--;
				if (i != 0) i++;
				parsebufpos = 0;
				gameidmatch = 0;
				while (tempgameconf[i] != ':')
				{
					if (tempgameconf[i] == '?')
					{
						parsebuffer[parsebufpos] = gameid[parsebufpos];
						parsebufpos++;
						gameidmatch--;
						i++;
					}
					else if (tempgameconf[i] != 0 && tempgameconf[i] != ' ')
						parsebuffer[parsebufpos++] = tempgameconf[i++];
					else if (tempgameconf[i] == ' ')
						break;
					else
						i++;
					if (parsebufpos == 8) break;
				}
				parsebuffer[parsebufpos] = 0;
				if (strncasecmp("DEFAULT", parsebuffer, strlen(parsebuffer)) == 0 && strlen(parsebuffer) == 7)
				{
					gameidmatch = 0;
					wifi_printf("sd_sd_copy_gameconfig: parsebuffer value = %s\n", parsebuffer);
					goto idmatch;
				}
				if (strncmp(gameid, parsebuffer, strlen(parsebuffer)) == 0)
				{
					gameidmatch += strlen(parsebuffer);
					wifi_printf("sd_sd_copy_gameconfig: gameid value = %s, parsebuffer value = %s\n", gameid, parsebuffer);
				idmatch:
					if (gameidmatch > maxgameidmatch2)
					{
						maxgameidmatch2 = gameidmatch;
					}
				}
				while ((i != tempgameconfsize) && (tempgameconf[i] != 10 && tempgameconf[i] != 13)) i++;
			}
			while (i != tempgameconfsize && tempgameconf[i] != ':')
			{
				parsebufpos = 0;
				while ((i != tempgameconfsize) && (tempgameconf[i] != 10 && tempgameconf[i] != 13))
				{
					if (tempgameconf[i] != 0 && tempgameconf[i] != ' ' && tempgameconf[i] != '(' && tempgameconf[i] != ':')
						parsebuffer[parsebufpos++] = tempgameconf[i++];
					else if (tempgameconf[i] == ' ' || tempgameconf[i] == '(' || tempgameconf[i] == ':')
						break;
					else
						i++;
					if (parsebufpos == 17) break;
				}
				parsebuffer[parsebufpos] = 0;

				if (strncasecmp("codeliststart", parsebuffer, strlen(parsebuffer)) == 0 && strlen(parsebuffer) == 13)
				{
					sscanf(tempgameconf + i, " = %x", (u32 *)&codelist);
					wifi_printf("sd_sd_copy_gameconfig: parsebuffer value = %s, codelist value = %08X\n", parsebuffer, codelist);
				}
				if (strncasecmp("codelistend", parsebuffer, strlen(parsebuffer)) == 0 && strlen(parsebuffer) == 11)
				{
					sscanf(tempgameconf + i, " = %x", (u32 *)&codelistend);
					wifi_printf("sd_sd_copy_gameconfig: parsebuffer value = %s, codelistend value = %08X\n", parsebuffer, codelistend);
				}
				if (strncasecmp("hooktype", parsebuffer, strlen(parsebuffer)) == 0 && strlen(parsebuffer) == 8)
				{
					ret = sscanf(tempgameconf + i, " = %u", &temp);
					wifi_printf("sd_sd_copy_gameconfig: parsebuffer value = %s, ret value = %d\n", parsebuffer, ret);
					if (ret == 1)
						if (temp <= 7)
							config_bytes[2] = temp;
					wifi_printf("sd_sd_copy_gameconfig: parsebuffer value = %s, temp value = %d\n", parsebuffer, temp);
					
				}
				if (strncasecmp("poke", parsebuffer, strlen(parsebuffer)) == 0 && strlen(parsebuffer) == 4)
				{
					ret = sscanf(tempgameconf + i, "( %x , %x", &codeaddr, &codeval);
					wifi_printf("sd_sd_copy_gameconfig: parsebuffer value = %s, ret value = %d\n", parsebuffer, ret);
					if (ret == 2)
					{
						*(gameconf + (gameconfsize / 4)) = 0;
						gameconfsize += 4;
						*(gameconf + (gameconfsize / 4)) = (u32)NULL;
						gameconfsize += 8;
						*(gameconf + (gameconfsize / 4)) = codeaddr;
						gameconfsize += 4;
						*(gameconf + (gameconfsize / 4)) = codeval;
						gameconfsize += 4;
						DCFlushRange((void *) (gameconf + (gameconfsize / 4) - 5), 20);
						wifi_printf("sd_sd_copy_gameconfig: 0x%08X = %08X\n",(u32)(gameconf + (gameconfsize / 4) - 5),
							(u32)(*(gameconf + (gameconfsize / 4) - 5)));
						wifi_printf("sd_sd_copy_gameconfig: 0x%08X = %08X\n",(u32)(gameconf + (gameconfsize / 4) - 4),
							(u32)(*(gameconf + (gameconfsize / 4) - 4)));
						wifi_printf("sd_sd_copy_gameconfig: 0x%08X = %08X\n",(u32)(gameconf + (gameconfsize / 4) - 3),
							(u32)(*(gameconf + (gameconfsize / 4) - 3)));
						wifi_printf("sd_sd_copy_gameconfig: 0x%08X = %08X (codeaddr)\n",(u32)(gameconf + (gameconfsize / 4) - 2),
							(u32)(*(gameconf + (gameconfsize / 4) - 2)));
						wifi_printf("sd_sd_copy_gameconfig: 0x%08X = %08X (codeval)\n",(u32)(gameconf + (gameconfsize / 4) - 1),
							(u32)(*(gameconf + (gameconfsize / 4) - 1)));
					}
				}
				if (strncasecmp("pokeifequal", parsebuffer, strlen(parsebuffer)) == 0 && strlen(parsebuffer) == 11)
				{
					ret = sscanf(tempgameconf + i, "( %x , %x , %x , %x", &codeaddr, &codeval, &codeaddr2, &codeval2);
					wifi_printf("sd_sd_copy_gameconfig: parsebuffer value = %s, ret value = %d\n", parsebuffer, ret);
					if (ret == 4)
					{
						*(gameconf + (gameconfsize / 4)) = 0;
						gameconfsize += 4;
						*(gameconf + (gameconfsize / 4)) = codeaddr;
						gameconfsize += 4;
						*(gameconf + (gameconfsize / 4)) = codeval;
						gameconfsize += 4;
						*(gameconf + (gameconfsize / 4)) = codeaddr2;
						gameconfsize += 4;
						*(gameconf + (gameconfsize / 4)) = codeval2;
						gameconfsize += 4;
						DCFlushRange((void *) (gameconf + (gameconfsize / 4) - 5), 20);
						wifi_printf("sd_sd_copy_gameconfig: 0x%08X = %08X\n",(u32)(gameconf + (gameconfsize / 4) - 5),
							(u32)(*(gameconf + (gameconfsize / 4) - 5)));
						wifi_printf("sd_sd_copy_gameconfig: 0x%08X = %08X (codeaddr)\n",(u32)(gameconf + (gameconfsize / 4) - 4),
							(u32)(*(gameconf + (gameconfsize / 4) - 4)));
						wifi_printf("sd_sd_copy_gameconfig: 0x%08X = %08X (codeval)\n",(u32)(gameconf + (gameconfsize / 4) - 3),
							(u32)(*(gameconf + (gameconfsize / 4) - 3)));
						wifi_printf("sd_sd_copy_gameconfig: 0x%08X = %08X (codeaddr2)\n",(u32)(gameconf + (gameconfsize / 4) - 2),
							(u32)(*(gameconf + (gameconfsize / 4) - 2)));
						wifi_printf("sd_sd_copy_gameconfig: 0x%08X = %08X (codeval2)\n",(u32)(gameconf + (gameconfsize / 4) - 1),
							(u32)(*(gameconf + (gameconfsize / 4) - 1)));						
					}
				}
				if (strncasecmp("searchandpoke", parsebuffer, strlen(parsebuffer)) == 0 && strlen(parsebuffer) == 13)
				{
					ret = sscanf(tempgameconf + i, "( %x%n", &codeval, &tempoffset);
					wifi_printf("sd_sd_copy_gameconfig: parsebuffer value = %s, ret value = %d\n", parsebuffer, ret);
					if (ret == 1)
					{
						gameconfsize += 4;
						temp = 0;
						while (ret == 1)
						{
							*(gameconf + (gameconfsize / 4)) = codeval;
							gameconfsize += 4;
							temp++;
							i += tempoffset;
							ret = sscanf(tempgameconf + i, " %x%n", &codeval, &tempoffset);
						}
						*(gameconf + (gameconfsize / 4) - temp - 1) = temp;
						ret = sscanf(tempgameconf + i, " , %x , %x , %x , %x", &codeaddr, &codeaddr2, &codeoffset, &codeval2);
						if (ret == 4)
						{
							*(gameconf + (gameconfsize / 4)) = codeaddr;
							gameconfsize += 4;
							*(gameconf + (gameconfsize / 4)) = codeaddr2;
							gameconfsize += 4;
							*(gameconf + (gameconfsize / 4)) = codeoffset;
							gameconfsize += 4;
							*(gameconf + (gameconfsize / 4)) = codeval2;
							gameconfsize += 4;
							DCFlushRange((void *) (gameconf + (gameconfsize / 4) - temp - 5), temp * 4 + 20);
							wifi_printf("sd_sd_copy_gameconfig: 0x%08X = %08X (temp)\n",(u32)(gameconf + (gameconfsize / 4) - temp - 5),
								(u32)(*(gameconf + (gameconfsize / 4) - temp - 5)));
							wifi_printf("sd_sd_copy_gameconfig: start at 0x%08X = ",(u32)(gameconf + (gameconfsize / 4) - temp - 4));
							for (j=temp; j>0; j--) {
								wifi_printf("%08X ",(u32)(*(gameconf + (gameconfsize / 4) - j - 4)));
							}
							wifi_printf("(codeval)\n");
							wifi_printf("sd_sd_copy_gameconfig: 0x%08X = %08X (codeaddr)\n",(u32)(gameconf + (gameconfsize / 4) - 4),
								(u32)(*(gameconf + (gameconfsize / 4) - 4)));
							wifi_printf("sd_sd_copy_gameconfig: 0x%08X = %08X (codeaddr2)\n",(u32)(gameconf + (gameconfsize / 4) - 3),
								(u32)(*(gameconf + (gameconfsize / 4) - 3)));
							wifi_printf("sd_sd_copy_gameconfig: 0x%08X = %08X (codeoffset)\n",(u32)(gameconf + (gameconfsize / 4) - 2),
								(u32)(*(gameconf + (gameconfsize / 4) - 2)));
							wifi_printf("sd_sd_copy_gameconfig: 0x%08X = %08X (codeval2)\n",(u32)(gameconf + (gameconfsize / 4) - 1),
								(u32)(*(gameconf + (gameconfsize / 4) - 1)));
						}
						else
							gameconfsize -= temp * 4 + 4;
					}
						
				}
				if (strncasecmp("videomode", parsebuffer, strlen(parsebuffer)) == 0 && strlen(parsebuffer) == 9)
				{
					ret = sscanf(tempgameconf + i, " = %u", &temp);
					wifi_printf("sd_sd_copy_gameconfig: parsebuffer value = %s, ret value = %d\n", parsebuffer, ret);
					if (ret == 1)
					{
						if (temp == 0)
						{
							if (config_bytes[1] != 0x00)
								configwarn |= 1;
							config_bytes[1] = 0x00;
						}
						else if (temp == 1)
						{
							if (config_bytes[1] != 0x01)
								configwarn |= 1;
							config_bytes[1] = 0x01;
						}
						else if (temp == 2)
						{
							if (config_bytes[1] != 0x02)
								configwarn |= 1;
							config_bytes[1] = 0x02;
						}
						else if (temp == 3)
						{
							if (config_bytes[1] != 0x03)
								configwarn |= 1;
							config_bytes[1] = 0x03;
						}
					}
					wifi_printf("sd_sd_copy_gameconfig: parsebuffer value = %s, temp value = %d\n", parsebuffer, temp);
				}
				if (strncasecmp("language", parsebuffer, strlen(parsebuffer)) == 0 && strlen(parsebuffer) == 8)
				{
					ret = sscanf(tempgameconf + i, " = %u", &temp);
					wifi_printf("sd_sd_copy_gameconfig: parsebuffer value = %s, ret value = %d\n", parsebuffer, ret);
					if (ret == 1)
					{
						if (temp == 0)
						{
							if (config_bytes[0] != 0xCD)
								configwarn |= 2;
							config_bytes[0] = 0xCD;
						}
						else if (temp > 0 && temp <= 10)
						{
							if (config_bytes[0] != temp-1)
								configwarn |= 2;
							config_bytes[0] = temp-1;
						}
					}
					wifi_printf("sd_sd_copy_gameconfig: parsebuffer value = %s, temp value = %d\n", parsebuffer, temp);
				}
				if (tempgameconf[i] != ':')
				{
					while ((i != tempgameconfsize) && (tempgameconf[i] != 10 && tempgameconf[i] != 13)) i++;
					if (i != tempgameconfsize) i++;
				}
			}
			if (i != tempgameconfsize) while ((tempgameconf[i] != 10 && tempgameconf[i] != 13) && (i != 0)) i--;
		}
	}
	
	wifi_printf("sd_sd_copy_gameconfig: gameconfsize value = %08X\n", gameconfsize);	
	tempcodelist = ((u8 *) gameconf) + gameconfsize;
}



