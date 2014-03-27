#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "gecko.h"

bool geckoinit = false;
char gprintfBuffer[256];

#ifdef GECKODEBUG
void gprintf(const char *format, ...)
{
	va_list va;
	if(geckoinit)
	{
		va_start(va, format);
		int len = vsnprintf(gprintfBuffer, 255, format, va);
		u32 level = IRQ_Disable();
		usb_sendbuffer_safe(1, gprintfBuffer, len);
		IRQ_Restore(level);
		va_end(va);
	}
}
#else
void gprintf(const char *format, ...)
{
}	
#endif

bool InitGecko()
{
	return geckoinit;
	memset(gprintfBuffer, 0, 256);
	u32 geckoattached = usb_isgeckoalive(EXI_CHANNEL_1);
	if(geckoattached)
	{
		geckoinit = true;
		usb_flush(EXI_CHANNEL_1);
	}
	return geckoinit;
}
