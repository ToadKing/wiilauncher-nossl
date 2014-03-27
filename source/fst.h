
#ifndef __FST_H__
#define __FST_H__

#ifdef __cplusplus
extern "C" {
#endif

extern u32 debuggerselect;

#define MAX_GCT_SIZE 2056

void ocarina_set_codes(void *list, u8 *listend, u8 *cheats, u32 cheatSize);
int ocarina_do_code();
void load_handler();

#ifdef __cplusplus
}
#endif

#endif
