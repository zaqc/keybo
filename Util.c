/*
 * Util.c
 *
 *  Created on: 21 θώνÿ 2017 γ.
 *      Author: s.zaicev
 */
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"

#include <assert.h>

void setupSWOForPrint(void) {
	/* Enable GPIO clock. */
	CMU->HFPERCLKEN0 |= CMU_HFPERCLKEN0_GPIO;

	/* Enable Serial wire output pin */
	GPIO->ROUTE |= GPIO_ROUTE_SWOPEN;

#if defined(_EFM32_GIANT_FAMILY) || defined(_EFM32_LEOPARD_FAMILY) || defined(_EFM32_WONDER_FAMILY) || defined(_EFM32_GECKO_FAMILY)
	/* Set location 0 */GPIO ->ROUTE = (GPIO ->ROUTE
			& ~(_GPIO_ROUTE_SWLOCATION_MASK)) | GPIO_ROUTE_SWLOCATION_LOC0;

	/* Enable output on pin - GPIO Port F, Pin 2 */GPIO ->P[5].MODEL &=
	~(_GPIO_P_MODEL_MODE2_MASK);
	GPIO ->P[5].MODEL |= GPIO_P_MODEL_MODE2_PUSHPULL;
#else
	/* Set location 1 */
	GPIO->ROUTE = (GPIO->ROUTE & ~(_GPIO_ROUTE_SWLOCATION_MASK))
			| GPIO_ROUTE_SWLOCATION_LOC1;
	/* Enable output on pin */
	GPIO->P[2].MODEH &= ~(_GPIO_P_MODEH_MODE15_MASK);
	GPIO->P[2].MODEH |= GPIO_P_MODEH_MODE15_PUSHPULL;
#endif

	/* Enable debug clock AUXHFRCO */
	CMU->OSCENCMD = CMU_OSCENCMD_AUXHFRCOEN;

	/* Wait until clock is ready */
	while (!(CMU->STATUS & CMU_STATUS_AUXHFRCORDY))
		;

	/* Enable trace in core debug */
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	ITM->LAR = 0xC5ACCE55;
	ITM->TER = 0x0;
	ITM->TCR = 0x0;
	TPI->SPPR = 2;
	TPI->ACPR = 0xf;
	ITM->TPR = 0x0;
	DWT->CTRL = 0x400003FE;
	ITM->TCR = 0x0001000D;
	TPI->FFCR = 0x00000100;
	ITM->TER = 0x1;
}
//----------------------------------------------------------------------------

// line receiver description:
// 1. waiting for first rise
// 2. after we are waiting fall
// 3. compare time from rise to fall with reference (~12.5 mSec for 40 Hz)
// 4. clamp on reference frequency (tune to front)
// additional:
// 1. slider windowed filter
// 2. ? (Schmit's trigger) fall on < low_ref && rise on > high_ref
int transmit_state = 0;

volatile unsigned char send_data = 0x00; // data for send
volatile int data_ready_for_send = 0; // data ready for transmit
volatile int sender_busy = 0;	// flag sender is busy

#define LINE_LOW_LEVEL	0x00
#define	LINE_HIGH_LEVEL	0x01

unsigned int test_buf[32];
int test_buf_ptr = 0;

void set_line_state(unsigned int val) {
	if (test_buf_ptr < 32)
		test_buf[test_buf_ptr++] = val;
}
//----------------------------------------------------------------------------

void reset_test_buf(void) {
	test_buf_ptr = 0;
	for (int i = 0; i < 32; i++)
		test_buf[i] = 0;
}
//----------------------------------------------------------------------------

#define	SS_NONE			0	// sender states
#define SS_DEFAULT		1
#define SS_START_BIT	3
#define SS_DATA			5

#define SEND_DEFAULT_STATE_INTERVAL		4	// interval when wire is not transmit any data (pause)
#define SEND_START_BIT_TRANSFORM_COUNT	4	// count of line level transforms for sending start bits (4 for two bits)
#define SEND_DATA_BIT_COUNT				8	// send data size in bit (8 bit => 16 line level transforms)

int send_state = SS_NONE;	// initial state of state machine
int send_bit_count = 0;		// bit counter
int send_half_bit = 0;

int send_start_bits[4] = {	//
		LINE_HIGH_LEVEL, LINE_LOW_LEVEL, /* "0" */
		LINE_LOW_LEVEL, LINE_HIGH_LEVEL }; /* "1" */

// Handler for TIMER0 interrupt (tick interval ~40Hz)
void stepped_send(void) {
	switch (send_state) {
	case SS_NONE:
		send_bit_count = 0;
		send_state = SS_DEFAULT;
		set_line_state(0xFF); // line z-state
		break;

	case SS_DEFAULT:
		set_line_state(LINE_LOW_LEVEL);
		if (send_bit_count + 1 < SEND_DEFAULT_STATE_INTERVAL)
			send_bit_count++;
		else {
			if (data_ready_for_send) {
				send_bit_count = 0;
				send_state = SS_START_BIT;
				data_ready_for_send = 0;
				sender_busy = 1;
			}
		}

		break;

	case SS_START_BIT:
		set_line_state(send_start_bits[send_bit_count]);
		send_bit_count++;
		if (send_bit_count >= SEND_START_BIT_TRANSFORM_COUNT) {
			send_state = SS_DATA;
			send_bit_count = 0;
		}
		break;

	case SS_DATA:
		// reverse bits send for receive in normal bit order
		set_line_state(
				(send_half_bit & 0x01) == 0x00 ?
						((send_data & 0x80) == 0x80 ?
						LINE_HIGH_LEVEL :
														LINE_LOW_LEVEL) :
						((send_data & 0x80) == 0x80 ?
						LINE_LOW_LEVEL :
														LINE_HIGH_LEVEL));

		send_data <<= send_half_bit & 0x01;
		send_bit_count += (send_half_bit & 0x01);
		send_half_bit = ~send_half_bit & 0x01;

		if (send_bit_count >= SEND_DATA_BIT_COUNT) {
			send_state = SS_DEFAULT;
			send_bit_count = 0;
			sender_busy = 0;
		}
		break;
	}
}
//----------------------------------------------------------------------------

void send_tst(unsigned char val) {
	// output buffer size is contains
	// 2 bits low line state (for test only)
	// 2 * 2 two start bits 2'b01 (10 01)
	// 8 * 2 eight bits of data
	// total: 2 + 2 * 2 + 8 * 2 = 22 bits
	unsigned char out[22];
	//unsigned int val = 0xA5;
	unsigned char half_bit = 0;
	int n_out = 0;

	out[n_out++] = 0;	// default line state
	out[n_out++] = 0;
	out[n_out++] = 1;	// first start bit "0"
	out[n_out++] = 0;
	out[n_out++] = 0;	// second start bit "1"
	out[n_out++] = 1;

	for (int i = 0; i < 16; i++) {
		out[n_out++] =
				(val & 0x01) == 0x01 ? (half_bit & 0x01) : (~half_bit) & 0x01;
		val >>= half_bit & 0x01;
		half_bit = (~half_bit) & 0x01;
	}

	for (int i = 0; i < 16; i++)
		ITM_SendChar(out[16]);
}
//----------------------------------------------------------------------------

unsigned char recv_tst(unsigned int *val) {
	// default line state is "0"
	// pause must be present between data sequence
	// minimum of pause time is time needed for sending 4 bits
	// decode steps:
	// 1. waiting two start bits (00...00 10 01)
	// 2. then receive 8 bits of data
	// 3. happy to do anything else :)
	unsigned short res = 0;
	unsigned char prev_line_state = 0;
	unsigned char half_bit = 0;
	int bit_count = 0;
	int n = 0;
	while (bit_count < 11 && n < 22) {
		unsigned char line_state = val[n++] & 0x01;
		if (n > 22)
			transmit_state = -1;	// receive error

		if (0x00 == half_bit) {
			prev_line_state = line_state;
			half_bit = ~half_bit & 0x01;
		} else {
			if ((line_state & 0x01) != (prev_line_state & 0x01)) { // add new bit
				res <<= 1;
				res |= (line_state & 0x01);
				half_bit = ~half_bit & 0x01;
				bit_count++;
			} else { // receive error
				if (bit_count == 0 && prev_line_state == 0x00
						&& line_state == 0x00) {
					// line is empty, nothing to do, wait for any 'high' line state
				} else {
					res = ~res & 0xFF;
					res <<= 1;
					res |= (line_state & 0x01);
					bit_count++;
				}
			}
		}
	}

	if ((res & 0x300) != 0x100)
		transmit_state = -7; // error receive start bits

	return (unsigned char) (res & 0xFF);
}
//----------------------------------------------------------------------------

void test(void) {
	// default line state is logical "0" (physics level is high voltage)
	// needed two start bits 2'b01 => 10 01 (for bit stream synchronization)
	// 8'b01011010 => 10 01 10 01 01 10 01 10
	send_tst(0x5A);
	// 8'b10001111
	unsigned int ref_data[] = { //
			0, 0, /* line default state */
			1, 0, 0, 1, /* start bits "0" "1" */
			0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1 /* data 8'b10001111 */
			};
	unsigned char res = recv_tst(ref_data); // pass as argument 0x8F
	assert(0x8F == res); // wait for return 0x8F

	ITM_SendChar(res);

	send_data = 0x8F;
	data_ready_for_send = 1;
	reset_test_buf();

	while (sender_busy || data_ready_for_send)
		stepped_send();

	for (int i = 0; i < 24; i++)
		ITM_SendChar(test_buf[i]);
}
//----------------------------------------------------------------------------
