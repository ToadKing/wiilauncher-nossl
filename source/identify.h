
#ifndef __IDENTIFY_H__
#define __IDENTIFY_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _dolheader
{
	u32 section_pos[18];
	u32 section_start[18];
	u32 section_size[18];
	u32 bss_start;
	u32 bss_size;
	u32 entry_point;
	u32 padding[7];
} __attribute__((packed)) dolheader;

extern u8 channelios;
extern u8 menuios;
extern u16 bootindex;
extern u32 bootid;
extern u32 bootentrypoint;
extern bool hookpatched;

s32 Channel_identify(u64 channeltoload);
s32 ES_Load_dol(u16 index);
s32 Menu_identify();
s32 ES_Load_dol_menu(u16 index);
void boot_menu();
void apply_pokevalues();
void apply_patch();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __Identify_H__

