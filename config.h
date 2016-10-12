/*HEADER**********************************************************************
*
* Copyright 2008 Freescale Semiconductor, Inc.
* Copyright 2004-2008 Embedded Access Inc.
*
* This software is owned or controlled by Freescale Semiconductor.
* Use of this software is governed by the Freescale MQX RTOS License
* distributed with this Material.
* See the MQX_RTOS_LICENSE file distributed for more details.
*
* Brief License Summary:
* This software is provided in source form for you to use free of charge,
* but it is not open source software. You are allowed to use this software
* but you cannot redistribute it or derivative works of it in source form.
* The software may be used only in connection with a product containing
* a Freescale microprocessor, microcontroller, or digital signal processor.
* See license agreement file for full license terms including other
* restrictions.
*****************************************************************************
*
* Comments:
*
*   Configurable information for the RTCS examples.
*
*
*END************************************************************************/


/*
** Define IP address and IP network mask
*/
#define MAIN_TASK   1
#define MQTT_TASK   2
#define ONE_WIRE_TASK   3

#define ENET_IPADDR  IPADDR(192,168,1,202) 
#define ENET_IPMASK  IPADDR(255,255,255,0)

#define ENET_IPGATEWAY  IPADDR(0,0,0,0)
#define ENET_MAC  {0x00,0x04,0x9F,0x01,0x5D,0xD1}
extern  void prvMQTTEchoTask(uint32_t);
extern  void Main_task (uint32_t);
extern  void MQTT_task(uint32_t);
extern  void task_onewire(uint32_t);
extern  bool SEC_GetTime(void);

typedef struct My_temp
{
		char MSG_STR[78];
		LWSEM_STRUCT READ_SEM;
		LWSEM_STRUCT WRITE_SEM;
} MY_DATA, * MY_DATA_PTR;

extern MY_DATA my_data;

#define  SNTP_SERVER "1.debian.pool.ntp.org" 

/* EOF */
