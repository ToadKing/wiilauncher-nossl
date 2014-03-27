
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <fat.h>
#include <sys/unistd.h>
#include <network.h>
#include <errno.h>
#include <math.h>
#include <ogc/lwp_watchdog.h>

#include "wifi_gecko.h"
#include "sd.h"
#include "download.h"
#include "menu.h"
#include "main.h"

#define NET_BUFFER_SIZE 1024

long bytes_count = 0;
char *tempdownload = (char *) sdbuffer;
u32 tempdownloadsize = 0;
int pagenumber;

struct CodeNode *head=NULL;
struct CodeNode *curr=NULL;
struct CodeNode *tail=NULL;

char flag;
u32 namesize, codesize, notesize;
u32 ncodes = 0;
char *name;
char *code;
char *note;

u32 getipbyname(char *domain)
{
        struct hostent *host = net_gethostbyname(domain);

        if(host == NULL) {
                return 0;
        }

        u32 *ip = (u32*)host->h_addr_list[0];
        return *ip;
}

s32 server_connect(char *hostname) {
	
	struct sockaddr_in connect_addr;
	u32 iphost;
	s32 ret;
	
	if (strncmp(ipaddress,DESTINATION_IP, 15) == 0) net_init();
	s32 server = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (server < 0) {
		wifi_printf("download_server_connect: Error creating socket, exiting\n");
		return server;
	}
	
	iphost = getipbyname(hostname);
	
	wifi_printf("download_server_connect: iphost = %08X\n", iphost);
	
	memset(&connect_addr, 0, sizeof(connect_addr));
	connect_addr.sin_family = AF_INET;
	connect_addr.sin_port = htons(80);
	connect_addr.sin_addr.s_addr= iphost;
	
	ret = net_connect(server, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
	if (ret == -1) {
		net_close(server);
		wifi_printf("download_server_connect: Failed to connect to the remote server, exiting.\n");
		return ret;
	}
	
	return server;
}

bool tcp_write (const s32 s, char *buffer, const u32 length) {
    char *p;
    u32 step, left, block, sent;
    //s64 t;
    s32 res;
  
    step = 0;
    p = buffer;
    left = length;
    sent = 0;
  
    while (left) {
     
        block = left;
        if (block > 2048)
            block = 2048;
      
        res = net_write (s, p, block);
      
        if ((res == 0) || (res == -56)) {
            usleep (20 * 1000);
            continue;
        }
      
        if (res < 0) {
            break;
        }
      
        sent += res;
        left -= res;
        p += res;
      
        if ((sent / NET_BUFFER_SIZE) > step) {
            step++;
        }
    }
  
    return left == 0;
}

s32 write_http_reply(s32 server, char *msg) {
    
	u32 msglen = strlen(msg);
    char msgbuf[msglen + 1];
    
	if (msgbuf == NULL) return -ENOMEM;
    strcpy(msgbuf, msg);
  
    tcp_write (server, msgbuf, msglen);

    return 1;
}

s32 request_file(s32 server, FILE *f) {
  
    char message[NET_BUFFER_SIZE];
    s32 bytes_read = net_read(server, message, sizeof(message));

    int length_til_data = 0;
    int tok_count = 2;
    char *temp_tok;
    if (bytes_read == 0) { return -1; }
    temp_tok = strtok (message,"\n");
  
    while (temp_tok != NULL) {
    
        if (strstr(temp_tok, "HTTP/1.1 4") || strstr(temp_tok, "HTTP/1.1 5")) {
            wifi_printf("The server appears to be having an issue. \nRetrying...");
            return -1;
        }
      
        if (strlen(temp_tok) == 1) {
            break;
        }
      
        length_til_data += strlen(temp_tok);
        tok_count++;  
        temp_tok = strtok (NULL, "\n");
    }
	
    char store_data[NET_BUFFER_SIZE];
    int q;
    int i = 0;
    for (q = length_til_data + tok_count; q < bytes_read; q++) {
        store_data[i] = message[q];
        i++;
    }
  
    if (store_data != NULL) {
        s32 bytes_written = fwrite(store_data, 1, i, f);
        if (bytes_written < i) {
            wifi_printf("download_request_file: fwrite error: [%i] %s\n", ferror(f), strerror(ferror(f)));
            sleep(1);
            return -1;
        }
    }
  
    while (bytes_read > 0) {      
        bytes_read = net_read(server, message, sizeof(message));
		
		bytes_count += bytes_read;
		if (bytes_count >= 2000) {
			bytes_count = 0;
		}
		
        s32 bytes_written = fwrite(message, 1, bytes_read, f);
        if (bytes_written < bytes_read) {
            wifi_printf("download_request_file: fwrite error: [%i] %s\n", ferror(f), strerror(ferror(f)));
            sleep(1);
            return -1;
        }
    }
  
    return 1;
}

s32 sd_write_download(char *hostname, char *gameidbuffer, int pagenumber) {
		
	char filepath[MAX_FILEPATH_LEN];
	char http_request[1000];
	char getfilestr[MAX_FILEPATH_LEN];
	char url[MAX_FILEPATH_LEN];
	s32 result;
	
	s32 main_server = server_connect(hostname);
	if (main_server < 0) {
		wifi_printf("cannot connect server = %s\n", hostname);
		return -1;
	}
	sprintf (filepath, "sd:/wiilauncher.tmp");
	FILE *fp = fopen(filepath, "wb");
	if (!fp){
		return 0;
	};
	
	sprintf(url, "/index.php?c=%s&page=%d", gameidbuffer, pagenumber);
	sprintf(getfilestr , "GET http://%s HTTP/1.0\r\nHost: %s\r\nCache-Control: no-cache\r\n\r\n", url, hostname);
	strcpy(http_request, getfilestr);
	write_http_reply(main_server, http_request);
	result = request_file(main_server, fp);

	net_close(main_server);
	fflush(fp);
	fclose(fp);

	return result;
}

s32 sd_read_download()
{
	FILE* fp;
	u32 ret;
	u32 filesize = 0;

	if (sd_found == 1)
	{
		fp = fopen("sd:/wiilauncher.tmp", "rb");
		
		if (fp) {
			fseek(fp, 0, SEEK_END);
			filesize = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			
			tempdownloadsize = 0;
			ret = fread((void*)tempdownload, 1, filesize, fp);
			fclose(fp);
			if (ret == filesize)
				tempdownloadsize += filesize;
		}
	}
	
	wifi_printf("download_sd_read_download: tempdownloadsize = %08X\n",
		tempdownloadsize);
	
	return tempdownloadsize;
}

u32 size(char *ptr)
{
    u32 offset = 0;

    while (*(ptr + offset++) != 0x0A);
 
    return offset;
}

struct NoteLine *CreateNoteLine(char *noterow)
{
	struct NoteLine *temp;
	temp=(struct NoteLine *)malloc(sizeof(struct NoteLine));
	temp->next = NULL;
	temp->noterow = NULL;
	if (noterow != NULL)
	{
		temp->noterow = (char *)malloc(size(noterow));
		memcpy(temp->noterow, noterow, size(noterow));
	}
	return(temp);	
}

struct CodeLine *CreateCodeLine(char *coderow)
{
	struct CodeLine *temp;
	temp=(struct CodeLine *)malloc(sizeof(struct CodeLine));
	temp->next = NULL;
	temp->coderow = NULL;
	if (coderow != NULL)
	{
		temp->coderow = (char *)malloc(CODEROWSIZE);
		memcpy(temp->coderow, coderow, CODEROWSIZE);
	}
	return(temp);	
}

struct CodeNode *CreateCodeNode(char flag, u32 namesize, char *name, u32 codesize, 
	char *code, u32 notesize, char *note)
{
	struct CodeNode *temp;
	temp=(struct CodeNode *)malloc(sizeof(struct CodeNode));
	temp->next = NULL;
	temp->name = NULL;
	temp->code = NULL;
	temp->note = NULL;
	temp->flag = flag;

	temp->namesize = namesize;
	if (temp->namesize > 0) {
		temp->name = (char *)malloc(temp->namesize + 1);
		if (temp->name != NULL)
			memcpy(temp->name,name,temp->namesize);
		temp->name[temp->namesize] = 0;
	}

	temp->codesize = codesize;
	if (temp->codesize > 0) {
		temp->code = (char *)malloc(temp->codesize + 1);
		if (temp->code != NULL)
			memcpy(temp->code,code,temp->codesize);
		temp->code[temp->codesize] = 0;
	}

	temp->notesize = notesize;
	if (temp->notesize > 0) {
		temp->note = (char *)malloc(temp->notesize + 1);
		if (temp->note != NULL)
			memcpy(temp->note,note,temp->notesize);
		temp->note[temp->notesize] = 0;
	}

	return(temp);	
}

u32 readname(char *buf, u32 pos, char **name, u32 maxpos)
{
	u32 namesize = 0;
	char stop = '[';
	u32 startpos, endpos;
	bool endname;
	
	if (pos > maxpos)
	{
		*name = NULL;
		return namesize;
	}
	pos++; //skip '>'

	startpos = pos;
	endpos = pos;
	endname = false;
	while (!endname && buf[pos] != stop && pos < maxpos) 
	{ 
		if (pos < maxpos - 2)
		{
			if (buf[pos] == '<' && buf[pos+1] == '/')
				endname = true;
		}
		pos++;
	}

	if (pos > startpos) endpos = pos - 1;
	if (endpos > startpos)
	{
		namesize = endpos - startpos + 1;
		*name = (char *)malloc(namesize  + 1);
		memcpy(*name, buf+startpos, namesize);
	
		if (namesize > 1) {
			if ((*name)[namesize-1] == ' ') {
				(*name)[namesize-1] = 0;
			}
			else {
				(*name)[namesize] = 0;
			}
		}				
	}
	
	return namesize;
}

u32 readcode(char *buf, u32 pos, char **code, u32 maxpos, char *flag)
{
	u32 codesize = 0;
	char check = '\0';
	char stop = '<';
	char eoc = '/';
	char eor = 'b';
	u32 codecol = 0;
	bool endcode = false;
	char coderow[CODEROWSIZE];
	char coderowprob[CODEROWSIZE] = "???????? ????????";
	struct CodeLine *head=NULL;
	struct CodeLine *curr=NULL;
	struct CodeLine *tail=NULL;
	u32 count = 0;
	char *codebuf;
	bool codeprob = false;
	
	if (pos > maxpos)
	{
		*code = NULL;
		*flag = check;
		return codesize;
	}
		
	head = CreateCodeLine(NULL);
	curr = head;
	
	(*flag) = check;
	
	pos++; // skip '>'
	while (!endcode)
	{
		codeprob = false;
		while (buf[pos] != stop && buf[pos] != 0x0A  && pos < maxpos) 
		{ 
			if (codecol < CODEROWSIZE - 1) {
				coderow[codecol++] = buf[pos++];
			} else {
				codeprob = true;
				break;
			}
		}
		
		if (codeprob)
			break;
		
		if (pos < maxpos - 3)
		{
			if (buf[pos] == stop && buf[pos+1] == eor)
			{
				pos += 4;
				codecol = 0;
			}
		}
		
		if (pos < maxpos - 5)
		{
			if (buf[pos] == stop && buf[pos+1] == eoc)
			{
				pos += 6;
				endcode = true;
			}
		}
		
		if (buf[pos] == 0x0A && pos < maxpos) pos++;
		if (!endcode)
		{
			coderow[CODEROWSIZE - 1] = 0x0A;
			curr->next = CreateCodeLine(coderow);
			curr = curr->next;
			count++;
		}
	}
	
	if (count == 0 || codeprob)
	{
		count = 1;
		memcpy(coderow,coderowprob,CODEROWSIZE);
		curr = head;
		curr->next = CreateCodeLine(coderow);
	}
	codesize = count*CODEROWSIZE;
	(*code) = (char *)malloc(codesize);
	codebuf = *code;
	curr = head;
	while (curr->next != NULL)
	{
		curr =  curr->next;
		memcpy(codebuf, curr->coderow, CODEROWSIZE);
		if (curr->next != NULL)
			codebuf += CODEROWSIZE;
	}
	while (head)
	{
		tail=head->next;
		free(head);
		head=tail;
	}

	return codesize;
}

u32 readnote(char *buf, u32 pos, char **note, u32 maxpos)
{
	u32 notesize = 0;
	char stop = '<';
	char eoc = '/';
	char eor = 'b';
	bool endnote = false;
	char *noterow = NULL;
	struct NoteLine *head=NULL;
	struct NoteLine *curr=NULL;
	struct NoteLine *tail=NULL;
	u32 count = 0;
	char *startbuf;
	u32 linesize = 0;
	char *notebuf;
	u32 startlinepos, endlinepos;
	u32 memsize = 0;
	
	if (pos > maxpos)
	{
		*note = NULL;
		return notesize;
	}
		
	head = CreateNoteLine(NULL);
	curr = head;
	
	pos++; // skip '>'
	while (!endnote)
	{
		startlinepos = pos;
		endlinepos = pos;
		startbuf = buf+pos;
		while (buf[pos] != stop && buf[pos] != 0x0A  && pos < maxpos) 
		{ 
			endlinepos = pos++;
		}
		
		if (pos < maxpos - 3)
		{
			if (buf[pos] == stop && buf[pos+1] == eor)
			{
				pos += 4;
				linesize = 0;
			}
		}
		
		if (pos < maxpos - 5)
		{
			if (buf[pos] == stop && buf[pos+1] == eoc)
			{
				pos += 6;
				endnote = true;
			}
		}
		
		if (buf[pos] == 0x0A && pos < maxpos) pos++;
		if (!endnote)
		{
			linesize = endlinepos - startlinepos + 2;
			notesize += linesize;
			noterow = (char *)malloc(linesize);
			memcpy(noterow, startbuf, linesize-1);
			noterow[linesize - 1] = 0x0A;
			curr->next = CreateNoteLine(noterow);
			curr = curr->next;
			count++;
			free(noterow);
		}
	}
		
	(*note) = (char *)malloc(notesize);
	notebuf = *note;
	curr = head;
	while (curr->next != NULL)
	{
		curr =  curr->next;
		noterow = curr->noterow;
		memsize = size(noterow);
		memcpy(notebuf, curr->noterow, memsize);
		if (curr->next != NULL)
			notebuf += memsize;
	}
	while (head)
	{
		tail=head->next;
		free(head);
		head=tail;
	}

	return notesize;
}

s32 parse_download()
{
	u32 i, numnonascii, parsebufpos;
	char parsebuf[18];
	bool startline, getname, getcode, getnote;
	
	numnonascii = 0;
	wifi_printf("download_parse_download: tempdownloadsize = %08X\n",
		tempdownloadsize);
	for (i = 0; i < tempdownloadsize; i++)
	{
		if (tempdownload[i] < 9 || tempdownload[i] > 126)
			numnonascii++;
		else
			tempdownload[i-numnonascii] = tempdownload[i];
	}
	tempdownloadsize -= numnonascii;

	i = 0;
	startline = false;
	ncodes = 0;
	flag = '\0';
	namesize = 0;
	codesize = 0;
	notesize = 0;
	name = NULL;
	code = NULL;
	note = NULL;
	getname = false;
	getcode = false;
	getnote = false;
	head = CreateCodeNode(flag, namesize, name, codesize, code, notesize, note);
	curr = head;
	bool stopall = false;
	while (i < tempdownloadsize && !stopall)
	{
		while (tempdownload[i] != 0x0A) i++;
		if (tempdownload[i] == 0x0A) { startline = true; i++; }
		if (i >= tempdownloadsize) break;
		if (startline)
		{
			parsebufpos = 0;
			if (tempdownload[i] == 0x0A) { startline = 1; i++; } // skip empty line
			else {
				while (tempdownload[i] != 0x0A && tempdownload[i] != '>')
				{
					parsebuf[parsebufpos++] = tempdownload[i++];
					if (parsebufpos == 17) break;
				}
				parsebuf[parsebufpos] = 0;
			}
		}
		
		if (getname && getcode && strncasecmp("<DIV CLASS=NAME", parsebuf, strlen(parsebuf)) == 0 && strlen(parsebuf) == 15)
		{
			if(i > 17) i = i-18;
			flag = checkflag(code, codesize);
			curr->next = CreateCodeNode(flag, namesize, name, codesize, code, notesize, note);
			curr = curr->next;
			ncodes++;
			getname = false;
			getcode = false;
			getnote = false;
			if (name != NULL) free(name);
			if (code != NULL) free(code);
			if (note != NULL) free(note);
			parsebuf[0] = 0;
		}	
		
		if (!getname && strncasecmp("<DIV CLASS=NAME", parsebuf, strlen(parsebuf)) == 0 && strlen(parsebuf) == 15)
		{ 
			namesize = 0;
			codesize = 0;
			notesize = 0;
			name = NULL;
			code = NULL;
			note = NULL;
			getname = false;
			getcode = false;
			getnote = false;
			flag = '-';
			namesize = readname(tempdownload, i, &name, tempdownloadsize);
			getname = true;
			parsebuf[0] = 0;
		}
	
		if (getname && strncasecmp("<DIV CLASS=CODE", parsebuf, strlen(parsebuf)) == 0 && strlen(parsebuf) == 15)
		{ 			
			codesize = readcode(tempdownload, i, &code, tempdownloadsize, &flag);
			code[codesize-1] = 0;
			getcode = true;
			parsebuf[0] = 0;
		}
		
		if (getname && strncasecmp("<DIV CLASS=NOTE", parsebuf, strlen(parsebuf)) == 0 && strlen(parsebuf) == 15)
		{
			notesize = readnote(tempdownload, i, &note, tempdownloadsize);
			note[notesize-1] = 0;
			getnote = true;
			parsebuf[0] = 0;
		}
		
		i++;
		startline = false;	
	}
	
	if (getname && getcode) // last one
	{
		flag = checkflag(code, codesize);
		curr->next = CreateCodeNode(flag, namesize, name, codesize, code, notesize, note);
		curr = curr->next;
		ncodes++;
		getname = false;
		getcode = false;
		getnote = false;
		if (name != NULL) free(name);
		if (code != NULL) free(code);
		if (note != NULL) free(note);	
	}
	
	curr = head;
	if (curr->next != NULL)
	{
		curr = curr->next;
		wifi_printf("download_parse_download: \nflag = %c\n%s\n%s\n%s\n", 
			curr->flag, curr->name, curr->code, curr->note);
	}
	while (curr->next != NULL)
	{
		curr =  curr->next;
	}
	
	wifi_printf("download_parse_download: \nflag = %c\n%s\n%s\n%s\n", 
		curr->flag, curr->name, curr->code, curr->note);
	
	return ncodes;
}

s32 sd_write_cht(struct CodeNode *head)
{
	char filepath[MAX_FILEPATH_LEN];
	int ret;
	char *buf = (char *) sdbuffer;
	int bufsize = 0;
	u32 count = 0;
	int posoffset = 0;
	struct CodeNode *tempnode=NULL;
	char flag;
	u32 namesize, codesize, notesize;
	char *name, *code, *note;
	
	if (sd_found == 0)
		return 0;
	
	sprintf (filepath, "sd:/wiilauncher.cht");
	FILE *fp = fopen(filepath, "wb");
	if (!fp){
		return 0;
	};
	
	tempnode = head;
	posoffset += sizeof(count);
	while (tempnode->next != NULL)
	{
		tempnode = tempnode->next;
		count++;
		
		flag = tempnode->flag;
		namesize = tempnode->namesize;
		name = tempnode->name;
		codesize = tempnode->codesize;
		code = tempnode->code;
		notesize = tempnode->notesize;
		note = tempnode->note;
		
		memcpy(buf+posoffset,&flag,sizeof(flag));
		posoffset += sizeof(flag);
		memcpy(buf+posoffset,&namesize,sizeof(namesize));
		posoffset += sizeof(namesize);
		memcpy(buf+posoffset,name,namesize);
		posoffset += namesize;
		memcpy(buf+posoffset,&codesize,sizeof(codesize));
		posoffset += sizeof(codesize);
		memcpy(buf+posoffset,code,codesize);
		posoffset += codesize;
		memcpy(buf+posoffset,&notesize,sizeof(notesize));
		posoffset += sizeof(notesize);
		memcpy(buf+posoffset,note,notesize);
		posoffset += notesize;
	}
	memcpy(buf,&count,sizeof(count));
	bufsize = posoffset;
	
	ret = fwrite(buf, sizeof(char), bufsize, fp);
	wifi_printf("download_sd_write_cht: number of bytes of cht file = %d\n", ret);

	if(ret != posoffset){
		fflush(fp);
		fclose(fp);
		return 0;
	}

	fflush(fp);
	fclose(fp);

	return 1;
}

s32 dealloccht(struct CodeNode *head)
{
	struct CodeNode *tail;
	
	while (head)
	{
		tail=head->next;
		free(head);
		head=tail;
	}
	
	if(head != NULL) 
	{
		wifi_printf("download_dealloccht: not empty\n");
		return 0;
	} 
		
	wifi_printf("download_dealloccht: empty\n");
		
	return 1;
}

u32 cbe32(char *p) {
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

s32 sd_read_cht(struct CodeNode **head)
{
	char filepath[MAX_FILEPATH_LEN];
	u32 ret = 0;
	u32 filesize = 0;
	FILE *fp = NULL;
	struct CodeNode *temphead=NULL;
	struct CodeNode *tempnode=NULL;
	char *buf = (char *) sdbuffer;
	u32 bufsize = 0;
	u32 count = 0, i = 0;
	char read32[4];
	char flag;
	u32 namesize = 0;
	char *name = NULL;
	u32 codesize = 0;
	char *code = NULL;
	u32 notesize = 0;
	char *note = NULL;
	u32 posoffset = 0;
	
	if (sd_found == 0)
		return 0;
	
	sprintf (filepath, "sd:/wiilauncher.cht");
	fp = fopen(filepath, "rb");
	if (!fp)
		return 0;
		
	fseek(fp, 0, SEEK_END);
	filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
			
	ret = fread((void*)buf, 1, filesize, fp);
	fclose(fp);
	if (ret == filesize) {
		bufsize += filesize;
	} else {
		return 0;
	}
	
	flag = '\0';
	namesize = 0;
	codesize = 0;
	notesize = 0;
	name = NULL;
	code = NULL;
	note = NULL;
	temphead = CreateCodeNode(flag, namesize, name, codesize, code, notesize, note);
	tempnode = temphead;
	wifi_printf("download_sd_read_cht: create head\n");
	
	memcpy(read32, buf+posoffset, sizeof(count));
	count = cbe32(read32);
	wifi_printf("download_sd_read_cht: count value = %d\n", count);
	
	posoffset += sizeof(count);
	while (i < count)
	{	
		memcpy(&flag, buf+posoffset, sizeof(flag));
		posoffset += sizeof(flag);
		
		memcpy(read32, buf+posoffset, sizeof(namesize));
		namesize = cbe32(read32); 
		posoffset += sizeof(namesize);
		name = (char *)malloc(namesize);
		memcpy(name, buf+posoffset, namesize);
		posoffset += namesize;
		
		memcpy(read32, buf+posoffset, sizeof(codesize));
		codesize = cbe32(read32); 
		posoffset += sizeof(codesize);
		code = (char *)malloc(codesize);
		memcpy(code, buf+posoffset, codesize);
		posoffset += codesize;
		
		memcpy(read32, buf+posoffset, sizeof(notesize));
		notesize = cbe32(read32); 
		posoffset += sizeof(notesize);
		note = (char *)malloc(notesize);
		memcpy(note, buf+posoffset, notesize);
		posoffset += notesize;

		tempnode->next = CreateCodeNode(flag, namesize, name, codesize, code, notesize, note);
		tempnode = tempnode->next;
		
		if (i == 0 || i == count - 1)  {
			wifi_printf("download_sd_read_cht: flag = %c\n", tempnode->flag);
			wifi_printf("download_sd_read_cht: namesize = %d, codesize = %d, notesize = %d\n", 
				tempnode->namesize, tempnode->codesize, tempnode->notesize);
			wifi_printf("download_sd_read_cht: \n%s\n%s\n%s\n", 
				tempnode->name, tempnode->code, tempnode->note);
		}
		
		free(name);
		free(code);
		free(note);
		i++;
	}
	
	(*head) = temphead;

	return 1;
}

s32 rundownload(char *downloadtoload)
{
	char hostname[80] = "geckocodes.org";
	
	resetscreen();
	
	if (sd_found == 1) {
		printf("\n    Downloading page = %d of %s from %s...", pagenumber, downloadtoload, hostname);
		s32 retval = sd_write_download(hostname, downloadtoload, pagenumber);
		if(retval <= 0) {
			printf("Failed");
			rundownloadnow = 0;
			menu_number = 0;
			sleep(2);
			return 0;
		} else {
			printf("Done");
			wifi_printf("download_rundownload: retval value write = %08X\n", retval);
			printf("\n    Reading wiilauncher.tmp file...");
			retval = sd_read_download();
			printf("Done");
			wifi_printf("download_rundownload: retval value read = %08X\n", retval);
			printf("\n    Parsing wiilauncher.tmp file...");
			ncodes = parse_download();
			printf("Done");
			printf("\n    Getting %d code(s)", ncodes);
			wifi_printf("download_rundownload: retval value parse = %d\n", ncodes);
			printf("\n    Saving to wiilauncher.cht file...");
			retval = sd_write_cht(head);
			printf("Done");
			wifi_printf("download_rundownload: retval value write cht = %d\n", retval);
			sleep(2);
		}
	}
	else {
		printf("\n    Cannot find SD card, aborting download process");
		rundownloadnow = 0;
		menu_number = 0;
		sleep(2);
		return 0;
	}
	
	rundownloadnow = 0;
	if (ncodes > 0) {
		if (codelist_menu_items == 0) {
			if(!menu_generatecodelist()) {
				menu_number = 41;
			}
			else {
				wifi_printf("download_rundownload: after generating code list\n");
				menu_number = 40;
			}
		} else {
			menu_number = 40;
		}
	} else {
		menu_number = 41;
	}
	
	return 1;
}

char checkflag(char *code, u32 codesize) {

	char flag = '-';
	int i = 0, j = 0;
	int nlines = 0;
	char read;
	
	nlines = (int) codesize / 0x12;
	if (codesize != (u32) (nlines*0x12))
		return '?';
	for (i = 0; i < nlines; i++)
	{
		j = 0;
		while (j < 8)
		{
			read = code[i*0x12+j];
			if ((read < 0x30) || (read > 0x3A && read < 0x41) ||
				(read >  0x46 && read < 0x61) ||
				(read > 0x66))
				flag = '?';
			j++;
		}
		j++;
		while (j < 17)
		{
			read = code[i*0x12+j];
			if ((read < 0x30) || (read > 0x3A && read < 0x41) ||
				(read >  0x46 && read < 0x61) ||
				(read > 0x66))
				flag = '?';
			j++;
		}
	}

	return flag;
}


