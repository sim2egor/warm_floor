#include <mqx.h>
#include <bsp.h> 
#include <fio.h>
#include <string.h>
#include "dsdriver_vera.h"
#include  "config.h" 


unsigned char Temp_data;
int Temp;

void task_onewire(uint32_t);

/* Functions */

void Get_temp();

int DS2480_detect(void);
int OWReset(void);
int OWLevel(int new_level);
int ReadScratch(void);
int OWWriteBytePower(int sendbyte);
int OWWriteByte(int sendbyte);
int OWWriteBit(char sendbit);
int OWBlock(unsigned char *tran_buf, int tran_len,  bool temp);
int OWSearch(void);
int OWFirst(void); 
int OWNext(void);
void OWTargetSetup(unsigned char family_code);

void flush(char *in_buf , int tran_len);
void flush_lib(unsigned char *in_buf , int tran_len);

int bitacc(int op, int state, int loc, unsigned char *buf);
unsigned char docrc8(unsigned char value); 

int ReadCOM(int inlen, char *inbuf);
int ReadCOM_lib(int inlen, unsigned char *inbuf);
/* Global variables */
LWSEM_STRUCT RX_sem,TX_sem;
int flag = 0;

// DS2480B state
int ULevel; // 1-Wire level
int UBaud;  // baud rate
int UMode;  // command or data mode state
int USpeed; // 1-Wire communication speed
int ALARM_RESET_COMPLIANCE = FALSE; // flag for DS1994/DS2404 'special' reset  

// search state
unsigned char ROM_NO[8];
int LastDiscrepancy;
int LastFamilyDiscrepancy;
int LastDeviceFlag;
unsigned char crc8;

int c_tmp, pulse;
float  		Temp_Current;

void Check_test(void); 

int num = 0;

void task_onewire
   (
      uint32_t initial_data
   )
{
	int i, cnt;
	DS2480_detect();
	while(1) {
		Get_temp(1);
		_time_delay(5000);
		num = 0;
		printf("\n Read cnt: %d\n", cnt++);
	}
}


void Check_test(void) {
	int res, i, cnt;
	res = DS2480_detect();
	
	res = OWFirst();
	while (res)
	{
		// print device found
		for (i = 7; i >= 0; i--)
			 printf("\n%02X", ROM_NO[i]);
		printf("  %d",++cnt);

		res = OWNext();
	}
}

void Get_temp(){
	int res, i, cnt;
	unsigned char sendpacket[10];
	int sendlen=0;
	TIME_STRUCT time;
  DATE_STRUCT date;
	
	char *temp_string = "{\"id_s\":\"%02x%02x%02x%02x%02x%02x%02x%02x\","
												"\"datetime\":\"%02d-%02d-%02d %02d:%02d:%02d\","
												"\"data\":\"%d.%d\"}";
		
	OWTargetSetup(0x28);
	res = OWFirst();	
	if (ROM_NO[0] == 0x28) {
		OWWriteBytePower(0x44);
		_time_delay(1000);
		OWLevel(MODE_NORMAL); 
		sendpacket[0] = 0x55; // match command
		
		for (i = 0; i < 8; i++) {
			sendpacket[i+1] = ROM_NO[i];
			//printf("%02X", ROM_NO[i]);
		}				
		if (OWReset()) {
			// MATCH ROM sequence
			OWBlock(sendpacket,9, 0);
			// Read Scratch pad
			sendlen = 0;
			sendpacket[sendlen++] = 0xBE;
			for (i = 0; i < 9; i++){
				sendpacket[sendlen++] = 0xFF;
			}
			OWBlock(sendpacket,sendlen, 1);	
		}
	}
	// обнулить строку
	  memset(my_data.MSG_STR,'\0',strlen(my_data.MSG_STR));
	  _time_get(&time);
	  _time_to_date(&time,&date);
	  sprintf(my_data.MSG_STR, temp_string, ROM_NO[7],ROM_NO[6],ROM_NO[5],ROM_NO[4],ROM_NO[3],ROM_NO[2],ROM_NO[1],ROM_NO[0], 
			date.YEAR,date.MONTH,date.DAY,date.HOUR+3,date.MINUTE,date.SECOND,
			c_tmp, pulse);
		printf("\n%s", my_data.MSG_STR);
		_lwsem_post(&my_data.READ_SEM);
		_lwsem_wait(&my_data.WRITE_SEM);
	while (OWNext()) {
		if (ROM_NO[0] == 0x28) {
			OWWriteBytePower(0x44);
			_time_delay(1000);
			OWLevel(MODE_NORMAL); 
			sendpacket[0] = 0x55; // match command
			for (i = 0; i < 8; i++) {
				sendpacket[i+1] = ROM_NO[i];	
				//printf("%02X", ROM_NO[i]);
			}				
			if (OWReset()) {
				// MATCH ROM sequence
				OWBlock(sendpacket,9, 0);
				// Read Scratch pad
				sendlen = 0;
				sendpacket[sendlen++] = 0xBE;
				for (i = 0; i < 9; i++){
					sendpacket[sendlen++] = 0xFF;
				}
				OWBlock(sendpacket,sendlen, 1);
			}
		}
		
	  memset(my_data.MSG_STR,'\0',strlen(my_data.MSG_STR));
	  _time_get(&time);
	  _time_to_date(&time,&date);
	  sprintf(my_data.MSG_STR, temp_string, ROM_NO[7],ROM_NO[6],ROM_NO[5],ROM_NO[4],ROM_NO[3],ROM_NO[2],ROM_NO[1],ROM_NO[0], 
			date.YEAR,date.MONTH,date.DAY,date.HOUR+3,date.MINUTE,date.SECOND,
			c_tmp, pulse);
		printf("\n%s", my_data.MSG_STR);
		_lwsem_post(&my_data.READ_SEM);
		_lwsem_wait(&my_data.WRITE_SEM);
		
	}
}

/* Функция переделяет наличие микросхемы DS2480 на линии */
int DS2480_detect(){
	MQX_FILE_PTR rs = NULL;
	char sendpacket[10],readbuffer[10];
//	bool disable_rx = TRUE;
	uint32_t result;
	int param;
	unsigned char sendlen = 0;
	
	UMode = MODSEL_COMMAND;
	UBaud = PARMSET_9600;
	USpeed = SPEEDSEL_FLEX;
	
	flush(sendpacket, 10);
	
	rs  = fopen( RS232_CHANNEL, NULL );                      
	if( rs == NULL ) {
		printf("\n Cannot open file rs func:detect");
		_task_block();
	}
	/* Set baudrate */
	param = 9600;
	result =  ioctl (rs, IO_IOCTL_SERIAL_SET_BAUD, &param);
	/* Set break */
	result =  ioctl(rs, IO_IOCTL_SERIAL_START_BREAK, NULL);
	_time_delay(2);
	result =  ioctl(rs, IO_IOCTL_SERIAL_STOP_BREAK, NULL);	
	_time_delay(2);
	
	sendpacket[0] = 0xC1;
	
	write( rs, sendpacket, strlen(sendpacket) );
	fflush( rs );
	_time_delay(2);	
	   // set the FLEX configuration parameters
	// default PDSRC = 1.37Vus
	sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_SLEW | PARMSET_Slew1p37Vus;
	// default W1LT = 10us
	sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_WRITE1LOW | PARMSET_Write10us;
	// default DSO/WORT = 8us
	sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_SAMPLEOFFSET | PARMSET_SampOff8us;

	// construct the command to read the baud rate (to test command block)
	sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_PARMREAD | (PARMSEL_BAUDRATE >> 3);

	// also do 1 bit operation (to test 1-Wire block)
	sendpacket[sendlen++] = CMD_COMM | FUNCTSEL_BIT | UBaud | BITPOL_ONE;
	
	write( rs, sendpacket, strlen(sendpacket) );
	fflush( rs );
	
	_mem_zero(readbuffer, 10);	
	flush(sendpacket, 10);
	//проверка валидности ответа - недопилена
	result = ReadCOM(5, readbuffer);
	
	if (result == 5) {
		 if (((readbuffer[3] & 0xF1) == 0x00) &&
		 ((readbuffer[3] & 0x0E) == UBaud) &&
		 ((readbuffer[4] & 0xF0) == 0x90) &&
		 ((readbuffer[4] & 0x0C) == UBaud))
		 {
				goto EXIT;
		 }
	}
	EXIT: 
	return TRUE;
	//return FALSE;
	
}

/* Функция посылает сигнал ResetPulse для сброса датчиков */
bool first_flag = 0;
int OWReset(void) {
	unsigned	char sendpacket[10],readbuffer[10];
	char sendlen=0;
	uint32_t result;
	MQX_FILE_PTR rs = NULL;

	flush_lib(sendpacket, 10);
	
  OWLevel(MODE_NORMAL);
	
	if (UMode != MODSEL_COMMAND)
	{
		UMode = MODSEL_COMMAND;
		sendpacket[sendlen++] = MODE_COMMAND;
	}
	
  sendpacket[sendlen++] = (unsigned char)(CMD_COMM | FUNCTSEL_RESET | SPEEDSEL_FLEX); // проверить последний аргумент
	
	rs  = fopen( RS232_CHANNEL, NULL );                      
	if( rs == NULL ) {
		printf("\n Cannot open file rs func: Reset");
		_task_block();
	}
	if (first_flag < 1) {
	/* Set baudrate */
		int param = 9600;
		result =  ioctl (rs, IO_IOCTL_SERIAL_SET_BAUD, &param);	
		first_flag = 1;
	}
	write( rs, sendpacket, sendlen);
	fflush( rs );
	//close
	fclose(rs);

	flush_lib(sendpacket, 10);
	result = ReadCOM_lib(1, readbuffer);

	return 1;
}

/* Функция высовляет режим работы канала (?) */
int OWLevel(int new_level) {
	unsigned char sendpacket[10],readbuffer[10];
	unsigned char rt = FALSE;
	char sendlen=0;
	uint32_t result;
	MQX_FILE_PTR rs = NULL;	

	if (new_level != ULevel) {
		
		if (UMode != MODSEL_COMMAND) {
			 UMode = MODSEL_COMMAND;
			 sendpacket[sendlen++] = MODE_COMMAND;
		}
		if (new_level == MODE_NORMAL)
		{
			flush_lib(sendpacket, 10);
			// stop pulse command
			sendpacket[sendlen++] = MODE_STOP_PULSE;

			// add the command to begin the pulse WITHOUT prime
			sendpacket[sendlen++] = CMD_COMM | FUNCTSEL_CHMOD | SPEEDSEL_PULSE | BITPOL_5V | PRIME5V_FALSE;

			// stop pulse command
			sendpacket[sendlen++] = MODE_STOP_PULSE;
		
			rs  = fopen( RS232_CHANNEL, NULL );                      
			if( rs == NULL ) {
				printf("\n Cannot open file rs func: Level");
				_task_block();
			}
			/* Set baudrate */
			int param = 9600;
			result =  ioctl (rs, IO_IOCTL_SERIAL_SET_BAUD, &param);	
			write( rs, sendpacket, sendlen); //strlen(sendpacket)
			fflush( rs );
			//close
			fclose(rs);
			flush_lib(sendpacket, 10);
		
			result = ReadCOM_lib(2, readbuffer);
			if (result) {
				if (((readbuffer[0] & 0xE0) == 0xE0) && ((readbuffer[1] & 0xE0) == 0xE0))	{
					rt = TRUE;
					ULevel = MODE_NORMAL;
				}
			}
		}
		else {
			// set the SPUD time value
			sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_5VPULSE | PARMSET_infinite;
			// add the command to begin the pulse
			sendpacket[sendlen++] = CMD_COMM | FUNCTSEL_CHMOD | SPEEDSEL_PULSE | BITPOL_5V;	
			
			rs  = fopen( RS232_CHANNEL, NULL );                      
			if( rs == NULL ) {
				printf("\n Cannot open file rs func: Level");
				_task_block();
			}
			/* Set baudrate */
			int param = 9600;
			result =  ioctl (rs, IO_IOCTL_SERIAL_SET_BAUD, &param);	
			write( rs, sendpacket, sendlen); //strlen(sendpacket)
			fflush( rs );
			//close
			fclose(rs);
			flush_lib(sendpacket, 10);
			
			result = ReadCOM_lib(1, readbuffer);
			if (result) {
				if ((readbuffer[0] & 0x81) == 0) {
					ULevel = new_level;
					rt = TRUE;
				}
			}
		}
		if (rt != TRUE)
			 DS2480_detect();
	}
	return ULevel;
}

/* Функция для отправлки пакета байтм (блока) */
int OWBlock(unsigned char *tran_buf, int tran_len,  bool temp){
	unsigned char sendpacket[320],readbuffer[320];
	char sendlen=0, pos, i;
	MQX_FILE_PTR rs = NULL;	
	uint32_t result;
	
	flush_lib(sendpacket, 10);	
	
	if (UMode != MODSEL_DATA)
	{
		UMode = MODSEL_DATA;
		sendpacket[sendlen++] = MODE_DATA;
	}
	
	pos = sendlen;
	for (i = 0; i < tran_len; i++) {
		sendpacket[sendlen++] = tran_buf[i];
		if (tran_buf[i] == MODE_COMMAND) {
			sendpacket[sendlen++] = tran_buf[i];
		}		
	}
	_time_delay(100);
	rs  = fopen( RS232_CHANNEL, NULL );                      
	if( rs == NULL ) {
		printf("\n Cannot open file rs func: Block");
		_task_block();
	}	
	write( rs, sendpacket, sendlen);
	fflush( rs );
	//close
	fclose(rs);
	
	result = ReadCOM_lib(tran_len, readbuffer);

	flush_lib(sendpacket, tran_len);
	if (temp == 1) {
		_time_delay(50);
//		for (i = 0; i < sendlen; i++) {
//			printf("%02x",readbuffer[i]);
//		}
//		printf("\n");
		unsigned char temperature;
		temperature = (readbuffer[1]>>4)|((readbuffer[2]&7)<<4);
		
	//Temp= temperature*100;
		c_tmp= (int)temperature;
		Temp_Current = (float)temperature;
		temperature = (readbuffer[1]&15);
		temperature = (temperature<<1) + (temperature<<3);
		temperature = (temperature>>4);
		
	//	printf("%d", (int)temperature);		
		pulse= (int)temperature;
		Temp_Current = Temp_Current + ((float)temperature / 10);
	
	}
	return 1;
}

/* Функция для прочтения памяти датчика */
int ReadScratch(void) {
	char sendpacket[10],readbuffer[10];
	char sendlen=0;
	uint32_t result;
	MQX_FILE_PTR rs = NULL;	
	int i;
	
	flush(sendpacket, 10);
	
	sendpacket[sendlen++] = 0xCC;
	sendpacket[sendlen++] = 0xBE;
	for (i = 0; i < 9; i++)
		sendpacket[sendlen++] = 0xFF;
	
	rs  = fopen( RS232_CHANNEL, NULL );                      
	if( rs == NULL ) {
		printf("\n Cannot open file rs Scratch");
		_task_block();
	}
	/* Set baudrate */
	int param = 9600;
	result =  ioctl (rs, IO_IOCTL_SERIAL_SET_BAUD, &param);	
	
	write( rs, sendpacket, strlen(sendpacket));
	fflush( rs );
	//close
		fclose(rs);
	flush(sendpacket, 10);
	return 1;
}

/* Функция для записи байта, служит для выставления команды датчикам (?!) */
int OWWriteBytePower(int sendbyte){
	char sendpacket[10],readbuffer[10];
	char sendlen=0;
	uint32_t result;
	MQX_FILE_PTR rs = NULL;	
	unsigned char i, temp_byte;

	flush(sendpacket, 10);
	
	sendpacket[sendlen++] = MODE_COMMAND;	
	
	sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_5VPULSE | PARMSET_infinite;
	
	temp_byte = sendbyte;
	for (i = 0; i < 8; i++) {
		sendpacket[sendlen++] = ((temp_byte & 0x01) ? BITPOL_ONE : BITPOL_ZERO)
														| CMD_COMM | FUNCTSEL_BIT | SPEEDSEL_FLEX |
														((i == 7) ? PRIME5V_TRUE : PRIME5V_FALSE);
		temp_byte >>= 1;
	}
	
	rs  = fopen( RS232_CHANNEL, NULL );                      
	if( rs == NULL ) {
		printf("\n Cannot open file rs WryteByte");
		_task_block();
	}
	/* Set baudrate */
	int param = 9600;
	result =  ioctl (rs, IO_IOCTL_SERIAL_SET_BAUD, &param);	
	
	write( rs, sendpacket, strlen(sendpacket));
	fflush( rs );
	
	flush(sendpacket, 10);	
	//close
	fclose(rs);

	result = ReadCOM(9, readbuffer);
	
	unsigned char rt=FALSE;

	if (result == 9) {
		//temp 
		ULevel = MODE_STRONG5;
		temp_byte   = 0 ;
		for (i = 0; i < 8; i++)
		{
			 temp_byte >>= 1;
			 temp_byte |= (readbuffer[i + 1] & 0x01) ? 0x80 : 0;
		}
		if (temp_byte == sendbyte)
			 rt = TRUE;
	}
	return rt;
}

int OWWriteByte(int sendbyte){
	char sendpacket[10],readbuffer[10];
	char sendlen=0;
	MQX_FILE_PTR rs = NULL;	
	uint32_t result;
	
	OWLevel(MODE_NORMAL);
//	sendpacket[sendlen++] = MODE_DATA;

	flush(sendpacket, 10);
	
	sendpacket[sendlen++] = (char)sendbyte;
	
	if (sendbyte ==(int)MODE_COMMAND)
		sendpacket[sendlen++] = (unsigned char)sendbyte;
	
	rs  = fopen( RS232_CHANNEL, NULL );                      
	if( rs == NULL ) {
		printf("\n Cannot open file rs WrByte");
		_task_block();
	}
	write( rs, sendpacket, strlen(sendpacket));
	fflush( rs );
	
	flush(sendpacket, 10);	
	//close
	fclose(rs);
	
	result = ReadCOM(1, readbuffer);
	return 1;
}

int OWWriteBit(char sendbit){
	char sendpacket[10],readbuffer[10];
	char sendlen=0;
	MQX_FILE_PTR rs = NULL;	
	uint32_t result;
	
	OWLevel(MODE_NORMAL);

	flush(sendpacket, 10);
	
	int param = 9600;
	sendpacket[sendlen++] = MODE_COMMAND;
	sendpacket[sendlen] = (sendbit != 0) ? BITPOL_ONE : BITPOL_ZERO;
	sendpacket[sendlen++] |= CMD_COMM | FUNCTSEL_BIT | USpeed;
	
	rs  = fopen( RS232_CHANNEL, NULL );                      
	if( rs == NULL ) {
		printf("\n Cannot open file rs");
		_task_block();
	}
	write( rs, sendpacket, strlen(sendpacket));
	fflush( rs );
	
	flush(sendpacket, 10);	
	//	close
	fclose(rs);
	result = ReadCOM(1, readbuffer);
	return 1;
}

void flush(char *in_buf , int tran_len) {
	int i, sendlen = 0;
	for (i = 0; i < tran_len; i++) {
	in_buf[sendlen++] = 0x00;
	}
}

// Find the 'next' devices on the 1-Wire bus
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
int OWNext()
{
   // leave the search state alone
   return OWSearch();
}

int OWFirst()
{
   // reset the search state
   LastDiscrepancy = 0;
   LastDeviceFlag = FALSE;
   LastFamilyDiscrepancy = 0;

   return OWSearch();
}

/*Общий поиск устройств*/
int OWSearch(void) {
	unsigned char sendpacket[40];
	unsigned char readbuffer[20];
	MQX_FILE_PTR rs = NULL;	
	unsigned char last_zero,pos;
	unsigned char tmp_rom[8];
  unsigned char i,sendlen=0;
	uint32_t result;
	
	if (LastDeviceFlag)
	{
		// reset the search
		LastDiscrepancy = 0;
		LastDeviceFlag = FALSE;
		LastFamilyDiscrepancy = 0;
		return FALSE;
	}
	
	OWReset();
	
	 // check for correct mode
	if (UMode != MODSEL_DATA)
	{
		UMode = MODSEL_DATA;
		sendpacket[sendlen++] = MODE_DATA;
	}

	// search command
	sendpacket[sendlen++] = 0xF0;

	// change back to command mode
	UMode = MODSEL_COMMAND;
	sendpacket[sendlen++] = MODE_COMMAND;

	// search mode on
	sendpacket[sendlen++] = (unsigned char)(CMD_COMM | FUNCTSEL_SEARCHON | USpeed);

	// change back to data mode
	UMode = MODSEL_DATA;
	sendpacket[sendlen++] = MODE_DATA;
	
	last_zero = 0;
	
	pos = sendlen;
	for (i = 0; i < 16; i++) {
		sendpacket[sendlen++] = 0;
	}
	// only modify bits if not the first search
	if (LastDiscrepancy != 0)
	{
		// set the bits in the added buffer
		for (i = 0; i < 64; i++)
		{
			 // before last discrepancy
			 if (i < (LastDiscrepancy - 1))
						 bitacc(WRITE_FUNCTION,
								 bitacc(READ_FUNCTION,0,i,&ROM_NO[0]),
								 (short)(i * 2 + 1),
								 &sendpacket[pos]);
			 // at last discrepancy
			 else if (i == (LastDiscrepancy - 1))
							bitacc(WRITE_FUNCTION,1,(short)(i * 2 + 1),
								 &sendpacket[pos]);
			 // after last discrepancy so leave zeros
		}
	}
	// change back to command mode
	UMode = MODSEL_COMMAND;
	sendpacket[sendlen++] = MODE_COMMAND;

	// search OFF command
	sendpacket[sendlen++] = (unsigned char)(CMD_COMM | FUNCTSEL_SEARCHOFF | USpeed);
	
	flush_lib(readbuffer, 20);	
	
	_time_delay(100);
	
	rs  = fopen( RS232_CHANNEL, NULL );                      
	if( rs == NULL ) {
		printf("\n Cannot open file rs func: Search");
		_task_block();
	}
	write( rs, sendpacket, sendlen);
	fflush( rs );
		fclose(rs);
	result = ReadCOM_lib(17, readbuffer);

	if (result == 17) {
		 // interpret the bit stream
		 for (i = 0; i < 64; i++)
		 {
				// get the ROM bit
				bitacc(WRITE_FUNCTION,
							 bitacc(READ_FUNCTION,0,(short)(i * 2 + 1),&readbuffer[1]),i,
							 &tmp_rom[0]);
				// check LastDiscrepancy
				if ((bitacc(READ_FUNCTION,0,(short)(i * 2),&readbuffer[1]) == 1) &&
						(bitacc(READ_FUNCTION,0,(short)(i * 2 + 1),&readbuffer[1]) == 0))
				{
					 last_zero = i + 1;
					 // check LastFamilyDiscrepancy
					 if (i < 8)
							LastFamilyDiscrepancy = i + 1;
				}
		 }
		// do dowcrc
		 crc8 = 0;
		 for (i = 0; i < 8; i++)
				docrc8(tmp_rom[i]);
		 
		 // check results
		 if ((crc8 != 0) || (LastDiscrepancy == 63) || (tmp_rom[0] == 0))
		 {
				// error during search
				// reset the search
				LastDiscrepancy = 0;
				LastDeviceFlag = FALSE;
				LastFamilyDiscrepancy = 0;
			 
				_mem_zero(sendpacket, 40);
				return FALSE;
		 }
		 // successful search
		 else
		 {
				// set the last discrepancy
				LastDiscrepancy = last_zero;

				// check for last device
				if (LastDiscrepancy == 0)
					 LastDeviceFlag = TRUE;

				// copy the ROM to the buffer
				for (i = 0; i < 8; i++)
					 ROM_NO[i] = tmp_rom[i];
				
				_mem_zero(sendpacket, 40);
				return TRUE;
		 }
		 
	}
	
}

int ReadCOM(int inlen, char *inbuf){
	MQX_FILE_PTR read_rs = NULL;
	uint32_t ser_opts =	 IO_SERIAL_NON_BLOCKING;
	_mqx_int param = 9600;
	uint32_t result, i;
	
	read_rs  = fopen( RS232_CHANNEL, NULL );
	if( read_rs == NULL ) {
		printf("\n Cannot open file read_rs");
		_task_block();
	}
		result =  ioctl (read_rs, IO_IOCTL_SERIAL_SET_BAUD, &param);
	
	if (result != MQX_OK)
	{
		fputs("TXtask Fatal Error: Unable to setup non-blocking mode.", stderr);
		_task_block();
	}
	_mem_zero( inbuf, inlen);
	result = fread(inbuf, 1, inlen, read_rs); 
	fclose(read_rs);
	return result;
}

int ReadCOM_lib(int inlen, unsigned char *inbuf){
	MQX_FILE_PTR read_rs = NULL;
	uint32_t ser_opts =	 IO_SERIAL_NON_BLOCKING;
	_mqx_int param = 9600;
	uint32_t result;
	
	read_rs  = fopen( RS232_CHANNEL, NULL );
		if( read_rs == NULL ) {
			printf("\n Cannot open file read_rs");
			_task_block();
		}
		_mem_zero( inbuf, inlen);
		result = fread(inbuf, 1, inlen, read_rs); 
		//close
	fclose(read_rs);
	
	return result;
}

int bitacc(int op, int state, int loc, unsigned char *buf)
{
   int nbyt,nbit;

   nbyt = (loc / 8);
   nbit = loc - (nbyt * 8);

   if (op == WRITE_FUNCTION)
   {
      if (state)
         buf[nbyt] |= (0x01 << nbit);
      else
         buf[nbyt] &= ~(0x01 << nbit);

      return 1;
   }
   else
      return ((buf[nbyt] >> nbit) & 0x01);
}


void flush_lib(unsigned char *in_buf , int tran_len) {
	int i, sendlen = 0;
	for (i = 0; i < tran_len; i++) {
	in_buf[sendlen++] = 0x00;
	}
}

//--------------------------------------------------------------------------
// Setup the search to find the device type 'family_code' on the next call
// to OWNext() if it is present.
//
void OWTargetSetup(unsigned char family_code)
{
   int i;

   // set the search state to find SearchFamily type devices
   ROM_NO[0] = family_code;
   for (i = 1; i < 8; i++){
      ROM_NO[i] = 0;
	 }
   LastDiscrepancy = 64;
   LastFamilyDiscrepancy = 0;
   LastDeviceFlag = FALSE;
}


// TEST BUILD
static unsigned char dscrc_table[] = {
        0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
      157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
       35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
      190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
       70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
      219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
      101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
      248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
      140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
       17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
      175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
       50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
      202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
       87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
      233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
      116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};

//--------------------------------------------------------------------------
// Calculate the CRC8 of the byte value provided with the current 
// global 'crc8' value. 
// Returns current global crc8 value
//
unsigned char docrc8(unsigned char value)
{
   // See Application Note 27
   
   // TEST BUILD
   crc8 = dscrc_table[crc8 ^ value];
   return crc8;
}

/* EOF */
