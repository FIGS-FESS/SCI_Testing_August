//###########################################################################
//
// FILE:   Example_2837xDSci_FFDLB_int.c
//
// TITLE:  SCI Digital Loop Back with Interrupts.
//
//! \addtogroup cpu01_example_list
//! <h1>SCI Digital Loop Back with Interrupts (sci_loopback_interrupts)</h1>
//!
//!  This program uses the internal loop back test mode of the peripheral.
//!  Other then boot mode pin configuration, no other hardware configuration
//!  is required. Both interrupts and the SCI FIFOs are used.
//!
//!  A stream of data is sent and then compared to the received stream.
//!  The SCI-A sent data looks like this: \n
//!  00 01 \n
//!  01 02 \n
//!  02 03 \n
//!  .... \n
//!  FE FF \n
//!  FF 00 \n
//!  etc.. \n
//!  The pattern is repeated forever.
//!
//!  \b Watch \b Variables \n
//!  - \b sdataA - Data being sent
//!  - \b rdataA - Data received
//!  - \b rdata_pointA - Keep track of where we are in the data stream.
//!    This is used to check the incoming data
//!
//
//###########################################################################
// $TI Release: F2837xD Support Library v210 $
// $Release Date: Tue Nov  1 14:46:15 CDT 2016 $
// $Copyright: Copyright (C) 2013-2016 Texas Instruments Incorporated -
//             http://www.ti.com/ ALL RIGHTS RESERVED $
//###########################################################################

//
// Included Files
//
#include "F28x_Project.h"

//
// Defines
//
#define CPU_FREQ        200E6
#define LSPCLK_FREQ     CPU_FREQ/4
#define SCI_FREQ        2400
#define SCI_PRD         (LSPCLK_FREQ/(SCI_FREQ*8))-1

//
// Globals
//
Uint16 ADCdata[2];
Uint16 sdataA[4];    // Send data for SCI-A
Uint16 rdataA[2];	 // Received data for SCI-A
Uint16 rdataA_check[2] = {'r','q'};    // Check data for SCI-A
Uint16 rdata_pointA; // Used for checking the received data

//
// Function Prototypes
//
interrupt void sciaTxFifoIsr(void);
interrupt void sciaRxFifoIsr(void);
void scia_fifo_init(void);
void error(void);
int CRC_Gen(uint32_t);
int Check_CRC(uint32_t, int);

volatile int RXRCV_flag = 0;
volatile int TXRDY_flag = 1;
volatile char frame[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

//
// Main
//
void main(void)
{
   Uint16 CRC;
   Uint32 data;
   int err = 0;

//
// Step 1. Initialize System Control:
// PLL, WatchDog, enable Peripheral Clocks
// This example function is found in the F2837xD_SysCtrl.c file.
//
   InitSysCtrl();

//
// Step 2. Initialize GPIO:
// This example function is found in the F2837xD_Gpio.c file and
// illustrates how to set the GPIO to it's default state.
//
   InitGpio();

//
// For this example, only init the pins for the SCI-A port.
//  GPIO_SetupPinMux() - Sets the GPxMUX1/2 and GPyMUX1/2 register bits
//  GPIO_SetupPinOptions() - Sets the direction and configuration of the GPIOS
// These functions are found in the F2837xD_Gpio.c file.
//
   GPIO_SetupPinMux(28, GPIO_MUX_CPU1, 1);
   GPIO_SetupPinOptions(28, GPIO_INPUT, GPIO_PUSHPULL);
   GPIO_SetupPinMux(29, GPIO_MUX_CPU1, 1);
   GPIO_SetupPinOptions(29, GPIO_OUTPUT, GPIO_ASYNC);

//
// Step 3. Clear all interrupts and initialize PIE vector table:
// Disable CPU interrupts
//
   DINT;

//
// Initialize PIE control registers to their default state.
// The default state is all PIE interrupts disabled and flags
// are cleared.
// This function is found in the F2837xD_PieCtrl.c file.
//
   InitPieCtrl();

//
// Disable CPU interrupts and clear all CPU interrupt flags:
//
   IER = 0x0000;
   IFR = 0x0000;

//
// Initialize the PIE vector table with pointers to the shell Interrupt
// Service Routines (ISR).
// This will populate the entire table, even if the interrupt
// is not used in this example.  This is useful for debug purposes.
// The shell ISR routines are found in F2837xD_DefaultIsr.c.
// This function is found in F2837xD_PieVect.c.
//
   InitPieVectTable();

//
// Interrupts that are used in this example are re-mapped to
// ISR functions found within this file.
//
   EALLOW;  // This is needed to write to EALLOW protected registers
   PieVectTable.SCIA_RX_INT = &sciaRxFifoIsr;
   PieVectTable.SCIA_TX_INT = &sciaTxFifoIsr;
   EDIS;    // This is needed to disable write to EALLOW protected registers

//
// Step 4. Initialize the Device Peripherals:
//
   scia_fifo_init();  // Init SCI-A

//
// Step 5. User specific code, enable interrupts:
//
// Init send data.  After each transmission this data
// will be updated for the next transmission
//
   ADCdata[0] = 0x0978;
   ADCdata[1] = 0x0536;

   rdata_pointA = rdataA_check[0];

//
// Enable interrupts required for this example
//
   PieCtrlRegs.PIECTRL.bit.ENPIE = 1;   // Enable the PIE block
   PieCtrlRegs.PIEIER9.bit.INTx1 = 1;   // PIE Group 9, INT1 - RX INT
   PieCtrlRegs.PIEIER9.bit.INTx2 = 1;   // PIE Group 9, INT2 - TX INT
   IER = 0x100;                         // Enable CPU INT
   EINT;

//
// Step 6. IDLE loop. Just sit and loop forever (optional):
//
    for(;;){

    	data = ( ( (0x0000FFF & (Uint32)ADCdata[0]) << 12 ) | (0x0FFF & ADCdata[1]) );

    	CRC = CRC_Gen(data);

    	sdataA[0] = (0x00FF0000 & data) >> 16; //0x007F;//
    	sdataA[1] = (0x0000FF00 & data) >> 8;  //0x007F;//
    	sdataA[2] = (0x000000FF & data);       //0x007F;//
    	sdataA[3] = (0x00FF & CRC);            //0x007F;//

//    	test = Check_CRC(data, CRC);

//    	if( test == 1 )
//    	{
//    		err++;
//    	}

    	if( TXRDY_flag == 1 )
    	{
    		SciaRegs.SCICTL2.bit.TXINTENA = 1;	 // Enable TX Interrupt
    	}


        if( RXRCV_flag == 1 )
        {

           RXRCV_flag = 0;

		   if( ( rdataA[0] != (rdata_pointA & 0x00FF) ) &&
			   ( rdataA[1] != ( (rdata_pointA+1) & 0x00FF ) ) )
		   {
			   err++; //error();
		   }else
		   {
			   TXRDY_flag = 1;
			   SciaRegs.SCICTL2.bit.RXBKINTENA = 0;   // Disable RX Interrupt
		   }
        }


//        if( (TXRDY_flag != 1) && (RXRCV_flag != 1) )
//        {
//        	PieCtrlRegs.PIEIER9.bit.INTx2 = 1;   // PIE Group 9, INT2 - TX INT
//        }


    }
}

/*
 * CRC calculation function
 */

int CRC_Gen(uint32_t data){
    int buffer = 8;
    uint32_t crc_poly = 0x00000083;
    int i = 24 - 1;
    data = data << buffer;
    for(; i > 0 ; i=i-1){
        if(data & ((uint32_t)1<<(i+buffer))){
            data ^= crc_poly << i;
        }
    }
    return (int)(0xff & (~(data)));
}

/*
 * CRC calculation function
 */

int Check_CRC(uint32_t data, int crc){
    int buffer = 8;
    uint32_t crc_poly = 0x00000083;
    int i = 24 - 1;
    data = data << buffer;
    for(; i > 0 ; i=i-1){
        if(data & ((uint32_t)1<<(i+buffer))){
            data ^= crc_poly << i;
        }
    }
    if((int)(0xff & (~(data))) && crc){
        return 0;
    }
    else{
        return 1;
    }
}

//
// error - Function to halt debugger on error
//
void error(void)
{
    asm("     ESTOP0"); // Test failed!! Stop!
	for (;;);
}

//
// sciaTxFifoIsr - SCIA Transmit FIFO ISR
//
interrupt void sciaTxFifoIsr(void)
{
	unsigned int i = 0;

	for(; i<4; i++)
	{
		SciaRegs.SCITXBUF.all=sdataA[i];  // Send data
	}

	TXRDY_flag = 0;

	SciaRegs.SCICTL2.bit.RXBKINTENA = 1;   // Enable RX Interrupt

    SciaRegs.SCIFFTX.bit.TXFFINTCLR=1;   // Clear SCI Interrupt flag
    SciaRegs.SCICTL2.bit.TXINTENA = 0;	 // Disable TX Interrupt
 //   PieCtrlRegs.PIEIER9.bit.INTx2 = 0;   // PIE Group 9, INT2
    PieCtrlRegs.PIEACK.all|=0x100;       // Issue PIE ACK
}

//
// sciaRxFifoIsr - SCIA Receive FIFO ISR
//
interrupt void sciaRxFifoIsr(void)
{
	unsigned int i = 0;
	static unsigned int cnt = 0;

    for(i=0;i<2;i++)
    {
       rdataA[i]=SciaRegs.SCIRXBUF.all;  // Read data
    }

    //for(i=0;i<2;i++)                     // Check received data
    //{
    //   if(rdataA[i] != ( (rdata_pointA+i) & 0x00FF) ) error();
    //}
    //rdata_pointA = (rdata_pointA+1) & 0x00FF;

	RXRCV_flag = 1; //set flag to indicate frame ready
	cnt++;

    SciaRegs.SCIFFRX.bit.RXFFOVRCLR=1;   // Clear Overflow flag
    SciaRegs.SCIFFRX.bit.RXFFINTCLR=1;   // Clear Interrupt flag

    PieCtrlRegs.PIEACK.all|=0x100;       // Issue PIE ack

	//if(SciaRegs.SCIRXBUF.bit.SCIFFFE || SciaRegs.SCIRXBUF.bit.SCIFFPE)
	//{
	//	//error handling here
	//	i = 6; //to bypass frame read, put a breakpoint here as well for debugging
	//}

	//for(; i<2;i++)
	//{
	//	rdataA[i] = SciaRegs.SCIRXBUF.bit.SAR; //read entire frame - TRY CHANGING THIS TO THE EXAMPLE METHOD
	//}


    //SciaRegs.SCIFFRX.bit.RXFFOVRCLR=1;   // Clear Overflow flag
    //SciaRegs.SCIFFRX.bit.RXFFINTCLR=1;   // Clear Interrupt flag

    //PieCtrlRegs.PIEACK.all|=0x100;       // Issue PIE ack
}

//
// scia_fifo_init - Configure SCIA FIFO
//
void scia_fifo_init()
{
   SciaRegs.SCICCR.all = 0x0007;      // 1 stop bit,  No loopback
                                      // No parity,8 char bits,
                                      // async mode, idle-line protocol
   SciaRegs.SCICTL1.all = 0x0003;     // enable TX, RX, internal SCICLK,
                                      // Disable RX ERR, SLEEP, TXWAKE
   SciaRegs.SCICTL2.bit.TXINTENA = 1;
   SciaRegs.SCICTL2.bit.RXBKINTENA = 1;
   SciaRegs.SCIHBAUD.all = 0x000A;	//Hard coded for 2400 Baud
   SciaRegs.SCILBAUD.all = 0x00C2;//SCI_PRD;  //only accepts the lower 8 bits
   SciaRegs.SCIFFTX.all = 0xC022;
   SciaRegs.SCIFFRX.all = 0x0022;
   SciaRegs.SCIFFCT.all = 0x00;

   SciaRegs.SCICTL1.all = 0x0023;     // Relinquish SCI from Reset
   SciaRegs.SCIFFTX.bit.TXFIFORESET = 1;
   SciaRegs.SCIFFRX.bit.RXFIFORESET = 1;
}

//
// End of file
//
