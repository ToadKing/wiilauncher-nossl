#ifndef _MENU_H_
#define _MENU_H_

#ifdef __cplusplus
extern "C" {
#endif

#define no_config_bytes 16

extern u8 config_bytes[no_config_bytes] ATTRIBUTE_ALIGN(32);
extern u64 channeltoload;
extern char downloadtoload[8];
extern u32 error_video;
extern u32 menu_number;
extern int runmenunow;
extern int rundiscnow;
extern int rundownloadnow;
extern u16 codelist_menu_items;

extern GXRModeObj *rmode;
extern u32 *xfb;

#define WII_MAGIC 0x5D1C9EA3
#define NGC_MAGIC 0xC2339F3D

enum discTypes
{
	IS_NGC_DISC=0,
	IS_WII_DISC,
	IS_UNK_DISC
};

s32 menu_generatechannellist();
s32 menu_generatedownloadlist();
s32 menu_generatecodelist();
u32 menu_updatechtgct(char *gameidbuffer);
s32 menu_generatecodeline(int selected);
void menu_draw();
void sys_init();
void resetscreen();
void menu_drawbootchannel();
void menu_drawbootdisc();
void exitme();

#ifdef __cplusplus
}
#endif

#endif