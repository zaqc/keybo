/* MAIN.C file
 * Серег, одобрение получено. Телефон Лены 9O3-68шесть-40-6 четыре
 * Copyright (c) 2002-2005 STMicroelectronics
 */
 /*
	Tumi Bhaja re Mana
	Tumi Japa re Mana
	Om Shri Ram Jaya Ram
	Japa re Mana
*/

#include <STM8S003K3.h>

#define	DISP_CS_n(a)	if(!a) PA_ODR &= ~0x08; else PA_ODR |= 0x08
#define	DISP_WR_n(a)	if(!a) PA_ODR &= ~0x02; else PA_ODR |= 0x02
#define	DISP_RD_n(a)	if(!a) PA_ODR &= ~0x04; else PA_ODR |= 0x04
#define	DISP_OUT			PB_ODR
#define DISP_IN				PB_IDR
#define DISP_CMD			PF_ODR |= 0x10
#define DISP_DATA			PF_ODR &= ~0x10
#define DLE						_asm("NOP"); _asm("NOP"); _asm("NOP"); _asm("NOP"); _asm("NOP")
#define	DELAY					_asm("NOP")	//DLE; DLE; DLE; DLE; DLE; DLE; DLE; DLE; DLE; DLE;
//----------------------------------------------------------


volatile unsigned char enc_count = 0;
volatile unsigned char enc_flag = 0;
@interrupt void encoder_io(void) {
	PD_ODR = PD_ODR ^ 0x08;
	
	if(PD_IDR & 0x80)
		enc_count++;
	else
		enc_count--;
		
	enc_flag++;
		
	return;
}
//----------------------------------------------------------------------------

volatile unsigned char send_flag = 0;

@interrupt void timer_int(void)
{	 
	TIM2_SR1 &= ~0x01;
		
	//PD_ODR = (PD_ODR & ~0x08) | (PD_ODR | 0x08);

	send_flag = 1;

	return;
}
//----------------------------------------------------------------------------

@interrupt void spi_int(void)
{
	unsigned char val;
	
	val = SPI_DR;

	return;
}
//----------------------------------------------------------

#define RX_QUEUE_LEN			16
volatile unsigned char rx_buf[RX_QUEUE_LEN];
volatile unsigned char rx_pptr = 0;
volatile unsigned char rx_gptr = 0; 

@interrupt void uart_rx_int()
{
	unsigned char ch;
	ch = UART1_DR;
	rx_buf[rx_pptr] = ch;
	rx_pptr++;
	rx_pptr %= RX_QUEUE_LEN; 
	
	return;
}
//----------------------------------------------------------

void WaitStatus(void);
void WriteCmd(unsigned char aCmd);

void WriteData(unsigned  char aData) {
	WaitStatus();

	PB_DDR |= 0xFF; PB_CR1 |= 0xFF; PB_CR2 &= 0x00; 

	DISP_DATA;
	DELAY;
	
	DISP_CS_n(0);
	DISP_RD_n(1);
	DISP_WR_n(1);
	DELAY;
	
	DISP_OUT = aData;
	DELAY;
	
	DISP_WR_n(0);
	DELAY;
	DISP_WR_n(1);
	DELAY;
	DISP_CS_n(1);	
	DELAY;
}

void WriteAddr(unsigned int aAddr) {	
	WriteData(aAddr & 0xFF);
	WriteData((aAddr >> 8) & 0xFF);
	WriteCmd(0x24);
}

void SetTextHomeAddr(unsigned int aAddr) {	
	WriteData(aAddr & 0xFF);
	WriteData((aAddr >> 8) & 0xFF);
	WriteCmd(0x40);
}

void SetTextArea(unsigned char aColumn) {	
	WriteData(aColumn);
	WriteData(0x00);
	WriteCmd(0x41);
}

void WriteCmd(unsigned char aCmd) {
	WaitStatus();
	
 	PB_DDR |= 0xFF; PB_CR1 |= 0xFF; PB_CR2 &= 0x00; 

	DISP_CMD;
	DELAY;
	
	DISP_CS_n(0);
	DISP_RD_n(1);
	DISP_WR_n(1);
	DELAY;
	
	DISP_OUT = aCmd;
	DELAY;

	DISP_WR_n(0);
	DELAY;
	DISP_WR_n(1);
	DELAY;
	DISP_CS_n(1);	
	DELAY;
}

unsigned char GetStatus(void) {
	unsigned char res = 0x00;
	
	PB_DDR &= 0x00; PB_CR1 &= 0x00; PB_CR2 &= 0x00;
	DELAY;
	
	DISP_CMD;
	DELAY;
	
	DISP_CS_n(0);
	DISP_WR_n(1);
	DELAY;
	
	DISP_RD_n(0);
	DELAY;
	res = DISP_IN;
	DELAY;
	DISP_RD_n(1);		
	DELAY;
	DISP_CS_n(1);
	DELAY;

	return res;
}

void WaitStatus(void) {
	while((GetStatus() & 0x03) != 0x03)
		;
}

char *str_t = "WWW.TVEMA.RU\0";
char *str = "DScope SPRUT-2\0";
char *str_m = "Version 1.0a\0";
char *str1 = "Waiting for main unit...\0";

//----------------------------------------------------------

unsigned char ScanKeys(void) {
	unsigned char ch = 0;
	unsigned char v;

	int i;
	for(i = 0; i < 100; i++) { DLE; }
	
	v = PC_IDR;
	ch |= ((v & 0x10) == 0x10) ? 0x01 : 0x00;
	ch |= ((v & 0x08) == 0x08) ? 0x02 : 0x00;
	ch |= ((v & 0x04) == 0x04) ? 0x04 : 0x00;
	ch |= ((v & 0x02) == 0x02) ? 0x08 : 0x00;
	ch |= ((PE_IDR & 0x20) == 0x20) ? 0x10 : 0x00;
	
	return ~ch;
}

//----------------------------------------------------------

main()
{
	long i = 0;
	unsigned char n = 0;
	unsigned char *pbuf;
	unsigned char ch;
	
	unsigned long keys;
	
	unsigned char state = 0;
	unsigned char h_cnt = 0;
	unsigned char t_len = 0;

	unsigned char kbuf[8];
	unsigned char *kptr;
	unsigned char s_count = 0xFF;

	_asm("SIM");
	
	CLK_SWR = 0xE1;
	while(CLK_CMSR != 0xE1) _asm("NOP");
	CLK_CKDIVR = 0x00; //0x02; //0x00;		// turn to Power MODE)))
	//CLK_PCKENR1 =
	
	CLK_HSITRIMR = 3;
	
	for(i = 0; i < 10000; i++) _asm("NOP");
	
	// ENCODER A=PD[4], B=PD[7]
	PD_DDR &= ~0x90;
	PD_CR1 &= ~0x90;
	PD_CR2 |= 0x90;
	
	// ENCODER Enable IRQ
	EXTI_CR1 |= 0x80;
		
	// LED PORT_D[3] config as output
	PD_DDR |= 0x08;
	PD_CR1 |= 0x08;
	PD_CR2 &= ~0x08;
	
	// Display DATA PB[7:0] config as output
	PB_DDR |= 0xFF;
	PB_CR1 |= 0xFF;
	PB_CR2 &= 0x00;
	
	// Display WR_n PA[1], RD_n PA[2], CS_n PA[3], Cmd/Data_n PA[4] as Output
	PA_DDR |= 0x1E;
	PA_CR1 |= 0x1E;
	PA_CR2 &= ~0x1E;
	PA_ODR |= 0x02; // clear WR_n
	PA_ODR |= 0x04; // clear RD_n
	PA_ODR |= 0x08; // clear CS_n
	
	// Display CMD/DATA PF[4]
	PF_DDR |= 0x10;
	PF_CR1 |= 0x10;
	PF_CR2 &= ~0x10;
	
	// Display rst_n PD[2]
	PD_DDR |= 0x04;		// config
	PD_CR1 |= 0x04;
	PD_CR2 &= ~0x04;
	PD_ODR &= ~0x04;	// send reset_n
	DELAY;
	PD_ODR |= 0x04;
	
	//--------------------------------------------------------------------------
	
	// Buttons MAP
	PD_DDR |= 0x01;		// COL 0	Output Open Drain
	PD_CR1 &= ~0x01;
	PD_CR2 &= ~0x01;

	PC_DDR |= 0x80;		// COL 1
	PC_CR1 &= ~0x80;
	PC_CR2 &= ~0x80;
	
	PC_DDR |= 0x40;		// COL 2
	PC_CR1 &= ~0x40;
	PC_CR2 &= ~0x40;
	
	PC_DDR |= 0x20;		// COL 1
	PC_CR1 &= ~0x20;
	PC_CR2 &= ~0x20;
	
	PC_DDR &= ~0x10;		// ROW 0 Input pull up
	PC_CR1 |= 0x10;
	PC_CR2 &= ~0x10;

	PC_DDR &= ~0x08;		// ROW 1
	PC_CR1 |= 0x08;
	PC_CR2 &= ~0x08;

	PC_DDR &= ~0x04;		// ROW 2
	PC_CR1 |= 0x04;
	PC_CR2 &= ~0x04;

	PC_DDR &= ~0x02;		// ROW 3
	PC_CR1 |= 0x02;
	PC_CR2 &= ~0x02;

	PE_DDR &= ~0x20;		// ROW 4
	PE_CR1 |= 0x20;
	PE_CR2 &= ~0x20;
	
	//--------------------------------------------------------------------------

	WaitStatus();
	
	// write data to Display
	
	WriteCmd(0xB2);

	WriteCmd(0x94);
	
	SetTextHomeAddr(0x0200);
	SetTextArea(30); // 30 columns 8 * 30 == 240 dot
	
	WriteAddr(0x0200);
	WriteCmd(0xB0);

	while((GetStatus() & 0x08) != 0x08)
		;

	for(i = 0; i < 320; i++)
		WriteData(0);
		
	WriteCmd(0x80);
	
	_asm("RIM");


	//while(1) {
		
		WriteCmd(0x94);
	
		SetTextHomeAddr(0x0200);
		SetTextArea(30); // 30 columns 8 * 30 == 240 dot
		
		WriteAddr(0x0200 + 9);
		WriteCmd(0xB0);
		while((GetStatus() & 0x08) != 0x08) _asm("NOP");
		pbuf = str_t;
		while(*pbuf) {
			WriteData(*pbuf - 32);
			pbuf++;
		}
		WriteCmd(0xB2);
		
		WriteAddr(0x0200 + 60 + 8);
		WriteCmd(0xB0);
		while((GetStatus() & 0x08) != 0x08) _asm("NOP");
		pbuf = str;
		while(*pbuf) {
			WriteData(*pbuf - 32);
			pbuf++;
		}
		WriteCmd(0xB2);
		
		WriteAddr(0x0200 + 90 + 9);
		WriteCmd(0xB0);
		while((GetStatus() & 0x08) != 0x08) _asm("NOP");
		pbuf = str_m;
		while(*pbuf) {
			WriteData(*pbuf - 32);
			pbuf++;
		}
		WriteCmd(0xB2);

		WriteAddr(0x0200 + 210);
		WriteCmd(0xB0);
		while((GetStatus() & 0x08) != 0x08) _asm("NOP");
		pbuf = str1;
		while(*pbuf) {
			WriteData(*pbuf - 32);
			pbuf++;
		}
	
		WriteCmd(0xB2);
	
		WriteCmd(0x94);
		
		SetTextHomeAddr(0x0200 + n % 30);
		n++;
		SetTextArea(30); // 30 columns 8 * 30 == 240 dot
	
		//WriteData(0x01);
		//WriteCmd(0xD0);
		for(i = 0; i < 10000; i++) {				
			_asm("NOP");
		}
		
		//WriteData(0x00);
		//WriteCmd(0xD0);
		for(i = 0; i < 10000; i++) {				
			_asm("NOP");
		}
	//}
	
	// COM Port PIN Configure
	PD_DDR |= 0x20;		// TX as output
	PD_CR1 |= 0x20;
	PD_CR2 &= ~0x20;

	PD_DDR &= ~0x40;	// RX as input
	PD_CR1 |= 0x40;
	PD_CR2 &= ~0x40;
		
	// COM Port
	UART1_BRR1 = 0x65; //0x08;
	UART1_BRR2 = 0x00; //0x0B;
	UART1_CR1 = 0x00;
	UART1_CR2 = 0x0C;
	UART1_CR3 = 0x00;
	
	// COM Interrupt RIEN enable
	UART1_CR2 |= 0x20;
	
	// TIMER 2 Interrupt Enable
	TIM2_CR1 = 0x81;
	TIM2_IER = 0x41;	// interrupt enable & update
	
	TIM2_EGR |= 0x01;	// generate event
	
	TIM2_PSCR = 4; // tim2 prescaler = F_mster / 16 (2^5)
	
	TIM2_ARRH = (50000 >> 8) & 0xFF;	// Timer Counter
	TIM2_ARRL = 50000 & 0xFF;

	/* while(1){
		if((UART1_SR & 0x80) == 0x80) {
			UART1_DR = 0xAA;
			
			while((UART1_SR & 0x40) == 0x00) ;				
		}		
	} */

	while(1)
	{
		while(rx_pptr != rx_gptr) {//(UART1_SR & 0x20) != 0x00) {
			ch = rx_buf[rx_gptr]; //UART1_DR;
			rx_gptr++;
			rx_gptr %= RX_QUEUE_LEN;
			switch(state) {
				case 0:	// wait header
					if(ch == 0x55) {
						h_cnt++;
						if(h_cnt == 4) {
							state = 1;
							h_cnt = 0;
						}
					} else
						h_cnt = 0;
					break;
				
				case 1: // recv Text Length
					h_cnt = 0;
					t_len = ch;
					state = 2;
					break;
					
				case 2:	// recv Text Target Addr
					WriteAddr(0x0200 + ch);
					WriteCmd(0xB0);
					while((GetStatus() & 0x08) != 0x08) _asm("NOP");
					state = 3;
					break;
					
				case 3:
					if(ch == 0x5D)
						state = 4;
					else {
						WriteCmd(0xB2);
						state = 0;
						h_cnt = 0;
					}
					break;
					
				case 4:
					WriteData(ch);
					h_cnt++;
					if(h_cnt >= t_len) {
						WriteCmd(0xB2);
						state = 0;
						h_cnt = 0;
					}
					break;
			}
		}
				
		if(s_count == 0xFF) {
			if(send_flag == 1) {
				keys = 0;
			
				PD_ODR &= ~0x01;
				keys |= (unsigned long)(0x1F & ScanKeys());
				keys <<= 5;
				PD_ODR |= 0x01;
				
				for(i = 0; i < 100; i++) { DLE; }
				
				PC_ODR &= ~0x80;
				keys |= (unsigned long)(0x1F & ScanKeys());
				keys <<= 5;
				PC_ODR |= 0x80;
				
				for(i = 0; i < 100; i++) { DLE; }
				
				PC_ODR &= ~0x40;
				keys |= (unsigned long)(0x1F & ScanKeys());
				keys <<= 5;
				PC_ODR |= 0x40;
				
				for(i = 0; i < 100; i++) { DLE; }
				
				PC_ODR &= ~0x20;
				keys |= (unsigned long)(0x1F & ScanKeys());
				keys <<= 8;
				PC_ODR |= 0x20;
				
				keys |= (unsigned long)enc_count;
				
				send_flag = 0;

				kbuf[0] = 0x55; 
				kbuf[1] = 0x55; 
				kbuf[2] = 0x55; 
				kbuf[3] = 0x55;
				
				kbuf[4] = (keys >> 24) & 0xFF;				
				kbuf[5] = (keys >> 16) & 0xFF; 
				kbuf[6] = (keys >> 8) & 0xFF; 
				kbuf[7] = keys & 0xFF;
				s_count = 0;
				send_flag = 0;
			}
		}

		if(s_count < 8) {
			if((UART1_SR & 0x80) == 0x80) {
				UART1_DR = kbuf[s_count];
				
				while((UART1_SR & 0x40) == 0x00) ;
				
				s_count++;
				if(s_count >= 8) s_count = 0xFF;
			}
		}
	}
}