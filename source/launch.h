
#ifndef _LAUNCH_H_
#define _LAUNCH_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern u8 geckoattached;
extern int wifigecko;
extern u8 loaderhbc;
extern u8 identifysu;
extern char gameidbuffer[8];
extern u8 vidMode;
extern int patchVidModes;
extern u8 vipatchselect;
extern u8 countryString;
extern int aspectRatio;
extern GXRModeObj *vmode;

void prepare();
s32 runchannel(u64 title);
s32 runmenu();
s32 rundisc();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _LAUNCH_H_ */
