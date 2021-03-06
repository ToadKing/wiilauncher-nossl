
#ifndef _DISC_H_
#define _DISC_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern int disc_type;

s32 Disc_Open();
s32 Disc_FindPartition(u32 *outbuf);
void Disc_SetLowMemPre();
void Disc_SetLowMem(u32 IOS);
void Disc_SetTime();
GXRModeObj *Disc_SelectVMode(u8 videoselected, u32 *rmode_reg);
void Disc_SetVMode(GXRModeObj *rmode, u32 rmode_reg);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
