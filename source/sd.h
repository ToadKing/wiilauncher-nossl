#ifndef _SD_H_
#define _SD_H_

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_FILENAME_LEN	128
#define MAX_FILEPATH_LEN	256

#define CODEDIR			"sd:/codes"
#define GECKOCODEDIR	"sd:/data/gecko/codes"

#define sdbuffer 0x90080000

extern u32 sd_found;
extern int codes_state;
extern u8 *codelist;
extern u8 *codelistend;
extern u32 codelistsize;
extern u8 *tempcodelist;

extern int patch_state;
extern char *tempgameconf;
extern u32 *gameconf;
extern u32 tempgameconfsize;
extern u32 gameconfsize;

extern u8 configwarn;

void sd_refresh();
u32 sd_init();
u32 sd_load_config();
u32 sd_save_config();
void sd_copy_codes(char *filename);
void sd_copy_patch(char *filename);
void sd_copy_gameconfig(char *gameid);

#ifdef __cplusplus
}
#endif

#endif
