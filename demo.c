#include <mqx.h>
#include <bsp.h>
#include <rtcs.h>
#include <shell.h>
#include <sh_rtcs.h>
#include <ipcfg.h>
#include "config.h"

#include "MQTTPacket.h"



const SHELL_COMMAND_STRUCT Shell_commands[] = {
   /* RTCS commands */ 
   { "arpadd",    Shell_arpadd },
   { "arpdel",    Shell_arpdel },
   { "arpdisp",   Shell_arpdisp },
   { "exit",      Shell_exit },      
   { "gate",      Shell_gate },
   { "gethbn",    Shell_get_host_by_name }, 
   { "getrt",     Shell_getroute },
   { "help",      Shell_help }, 
   { "ipconfig",  Shell_ipconfig },      
   { "netstat",   Shell_netstat },   
#if RTCSCFG_ENABLE_ICMP      
   { "ping",      Shell_ping },      
#endif
   { "walkrt",    Shell_walkroute },
   { "?",         Shell_command_list },
   { "dns",       Shell_get_host_by_name }, /* DNS Resolver.*/ 
   { NULL,        NULL } 
};

TASK_TEMPLATE_STRUCT MQX_template_list[] =
{
/*  Task number , Entry point,      Stack  , Pri,  Name      ,          Attribute ,Param, Time */
   {MAIN_TASK   ,   Main_task      ,   3000,  9 ,   "main"   , MQX_AUTO_START_TASK,    0,   0},
	 {MQTT_TASK,   MQTT_task        ,   3000,  8 ,   "MQTTRun",                   0,    0,   0},
	 {ONE_WIRE_TASK,   task_onewire        ,   3000,  8 ,   "ONE WIRE",                   0,    0,   0},
   {0}
};
int  g_init = 0;
MY_DATA my_data;

void Main_task(uint32_t initial_data)
{
		uint32_t error;
	IPCFG_IP_ADDRESS_DATA auto_ip_data;
	_enet_address enet_address;
	auto_ip_data.ip = ENET_IPADDR;
	auto_ip_data.mask = ENET_IPMASK;
	auto_ip_data.gateway = ENET_IPGATEWAY;
	int retval =-1;
	struct addrinfo *addrinfo_result;
	
  DATE_STRUCT days;
	uint32_t rtc_time;
	TIME_STRUCT mqx_time;
	//сначала инит семафоры:
	
	error= _lwsem_create(&my_data.READ_SEM,0);
	if (error != MQX_OK)
	{
		printf("\n Creating sem read error : 0x%X", error);
		_mqx_exit(0);
	}
	error= _lwsem_create(&my_data.WRITE_SEM,0);
	if (error != MQX_OK)
	{
		printf("\n Creating sem write error : 0x%X", error);
		_mqx_exit(0);
	}
	
	  _int_install_unexpected_isr();
	
	error = RTCS_create();
	ENET_get_mac_address (BSP_DEFAULT_ENET_DEVICE, ENET_IPADDR, enet_address);
	error = ipcfg_init_device(BSP_DEFAULT_ENET_DEVICE, enet_address);
	error = ipcfg_bind_dhcp_wait(BSP_DEFAULT_ENET_DEVICE, TRUE, &auto_ip_data);
	if (error != RTCS_OK) 
        {
            printf("\nIPCFG: Failed to bind IP address. Error = 0x%lX", (uint32_t)error);
            _task_block();
        };
				ipcfg_add_dns_ip(BSP_DEFAULT_ENET_DEVICE,0x08080808);
				Shell_ipconfig(0,0);
				//как то найдем IP адрес
				//retval = getaddrinfo("m21.cloudmqtt.com", NULL, NULL, &addrinfo_result);
//				if (retval != 0)
//				{
//					freeaddrinfo(addrinfo_result);
//					printf("Not resolve name\n");
//				}
// попробуем запустить эхо
				_rtc_get_time(&rtc_time);
				mqx_time.SECONDS =rtc_time; 
				_time_set(&mqx_time);
				_time_to_date(&mqx_time,&days);
        printf("\nCurrent Time: %02d/%02d/%02d %02d:%02d:%02d\n",
        days.YEAR,days.MONTH,days.DAY,days.HOUR+3,days.MINUTE,days.SECOND);
	 
				//SEC_GetTime();
				error= _task_create(0,MQTT_TASK,0);
				error= _task_create(0,ONE_WIRE_TASK,0);

	for (;;)  
   {
      /* Run the shell */
      Shell(Shell_commands, NULL);
		 _time_delay(2000);
   }
}

void MQTT_task(uint32_t initial_data)
{
	uint32_t error;
	while(g_init !=0)
	{
		_time_delay(100);
	}
	while ( 1)
	{
		error =_lwsem_wait(&my_data.READ_SEM);
			MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
			int rc = 0;
			char buf[200];
			MQTTString topicString = MQTTString_initializer;
			char* payload = "mypayload";
			int payloadlen = strlen(payload);int buflen = sizeof(buf);

//			data.clientID.cstring = "USER2";
		
			data.username.cstring = "ricbrwgn";
			data.password.cstring = "Qr6OkH6z4SHg";
		  data.keepAliveInterval = 120;
			data.cleansession = 1;
			int len = MQTTSerialize_connect(buf, buflen, &data); /* 1 */

			topicString.cstring = "KT/HotFloor/level1/temp1";
			len += MQTTSerialize_publish(buf + len, buflen - len, 0, 0, 0, 0, topicString, my_data.MSG_STR,strlen(my_data.MSG_STR) ); /* 2 */

			len += MQTTSerialize_disconnect(buf + len, buflen - len); /* 3 */
			uint32_t mysock;
			
			mysock = socket (AF_INET,SOCK_STREAM,0);
		
			sockaddr_in	addr;
			addr.sin_family = AF_INET;
			addr.sin_port = 17607;
			addr.sin_addr.s_addr = 0x34328925; //0xC6291EF1;
			
			connect(mysock,(struct sockaddr *) &addr,sizeof(addr));
			send(mysock,buf,len,NULL);
			closesocket(mysock);
//			rc = Socket_new("127.0.0.1", 1883, &mysock);
//			rc = write(mysock, buf, len);
//			rc = close(mysock);
		error = _lwsem_post(&my_data.WRITE_SEM);
//		_time_delay(10);
	}
		
}

bool SEC_GetTime(void)
{
    bool res = FALSE;
    

   _ip_address  ipaddr;
   TIME_STRUCT time;
   DATE_STRUCT date;
	
	 uint32_t  rtc_time;
   char tries = 0;

   /* Try three times to get time */
   while(tries<3)
   {
     _time_delay(1000);
      printf("\nGetting time from time server ... ");

      if (RTCS_resolve_ip_address(SNTP_SERVER,&ipaddr,NULL,0)) {
         /* Contact SNTP server and update time */
         if(SNTP_oneshot(ipaddr,1000)==RTCS_OK)
         {
            printf("Succeeded\n");
            res = TRUE;
            break;
         }
         else
         {
            printf("Failed\n");
         }

      }
      else
      {
         printf("Failed - address not resolved\n");
         break;
      }
    tries++;
   }
   /* Get current time */
   _time_get(&time);
	 _rtc_set_time(time.SECONDS);
   _time_to_date(&time,&date);
   printf("\nCurrent Time: %02d/%02d/%02d %02d:%02d:%02d\n",
      date.YEAR,date.MONTH,date.DAY,date.HOUR+3,date.MINUTE,date.SECOND);
	 
		printf("\n %d \n",time.SECONDS);

   return res;
}

