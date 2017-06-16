/*	BASIC INTERRUPT VECTORS TABLE FOR ST7 devices
 * 		(for Metrowerks compiler only)
 *	Copyright (c) 2002-2005 STMicroelectronics
 */


extern void _Startup();		/* startup routine */  



interrupt void NonHandledInterrupt (void)
{
	/* in order to detect unexpected events during development, 
	   it is recommended to set a breakpoint on the following instruction
	*/
	return;
}


#pragma CONST_SEG .myVectorTable
/* Interrupt vector table, to be linked at the address
   0xFFE0 (in ROM) */ 
volatile void (* const _vectab[])() = {
	NonHandledInterrupt,  			/* 0xFFE0 */
	NonHandledInterrupt,			/* 0xFFE2 */
	NonHandledInterrupt,			/* 0xFFE4 */
	NonHandledInterrupt,			/* 0xFFE6 */
	NonHandledInterrupt,			/* 0xFFE8 */
	NonHandledInterrupt,			/* 0xFFEA */
	NonHandledInterrupt,			/* 0xFFEC */
	NonHandledInterrupt,			/* 0xFFEE */
	NonHandledInterrupt,			/* 0xFFF0 */
	NonHandledInterrupt,			/* 0xFFF2 */
	NonHandledInterrupt,			/* 0xFFF4 */
	NonHandledInterrupt,			/* 0xFFF6 */
	NonHandledInterrupt,			/* 0xFFF8 */
	NonHandledInterrupt,			/* 0xFFFA */
	NonHandledInterrupt,			/* 0xFFFC */
	_Startup,		/* Reset Vector */
};   
#pragma DATA_SEG DEFAULT             

