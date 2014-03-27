#ifndef _WDVD_H_
#define _WDVD_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

s32 WDVD_Init(void);
s32 WDVD_Close(void);
s32 WDVD_Reset(void);
s32 WDVD_Seek(u32 offset);
s32 WDVD_ReadDiskId(void *id);
s32 WDVD_Read(void *buf, u32 len, u32 offset);
s32 WDVD_UnencryptedRead(void *buf, u32 len, u32 offset);
s32 WDVD_OpenPartition(u32 offset, u32 *IOS);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
