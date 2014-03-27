#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define CODEROWSIZE 18

extern int pagenumber;
extern u32 ncodes;

struct NoteLine{
	char *noterow;
	struct NoteLine *next;
};

struct CodeLine{
	char *coderow;
	struct CodeLine *next;
};

struct CodeNode{
	char flag; // '+', '-', '?'
	u32 namesize;
	char *name;
	u32 codesize;
	char *code;
	u32 notesize;
	char *note;
	struct CodeNode *next;
};

extern struct CodeNode *head;
extern struct CodeNode *curr;
extern struct CodeNode *tail;

u32 getipbyname(char *domain);
s32 server_connect(char *hostname);
s32 request_file(s32 server, FILE *f);
s32 write_http_reply(s32 server, char *msg);
bool tcp_write (const s32 s, char *buffer, const u32 length);
s32 sd_write_download(char *hostname, char *gameidbuffer, int pagenumber);
s32 sd_read_download();
s32 parse_download();
u32 size(char *ptr);
s32 sd_write_cht(struct CodeNode *head);
s32 dealloccht(struct CodeNode *head);
s32 sd_read_cht(struct CodeNode **head);
char checkflag(char *code, u32 codesize);
s32 rundownload(char *downloadtoload);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
