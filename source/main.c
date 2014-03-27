
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <network.h>

#include "menu.h"
#include "wifi_gecko.h"
#include "launch.h"
#include "download.h"
#include "sd.h"
#include "main.h"

char ipaddress[255];
int portnumber;

int main(int argc, char *argv[])
{
	sprintf(ipaddress,"%s",DESTINATION_IP);
	portnumber = DESTINATION_PORT;
	pagenumber = 1;
	while ((argc > 1) && (argv[1][0] == '-'))
	{
		switch (argv[1][1])
		{
			case 'a':
				sprintf(ipaddress,"%s",&argv[1][2]);
				break;

			case 'p':
				portnumber = atoi(&argv[1][2]);
				break;
				
			case 'd':
				pagenumber = atoi(&argv[1][2]);
				break;
		}
		++argv;
		--argc;
	}

	prepare();
	
	while(1) {
	
		menu_draw();
		VIDEO_WaitVSync();
		
		if (runmenunow != 0x0) {
			wifi_printf("main_main: passing to launch_runmenu\n");
			runmenu();
		}
		
		if (rundiscnow != 0x0) {
			wifi_printf("main_main: passing to launch_rundisc\n");			
			rundisc();
		}
		
		if (channeltoload != 0x0) {
			wifi_printf("main_main: passing to launch_runchannel\n");
			runchannel(channeltoload);
		}
		
		if (rundownloadnow != 0x0) {
			wifi_printf("main_main: passing to download_rundownload\n");
			rundownload(downloadtoload);
		}
	}
	return 0;
}

