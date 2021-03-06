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
#define SCI_FREQ        1E6   //Baud Rate
#define SCI_PRD         (LSPCLK_FREQ/(SCI_FREQ*8))-1


//
// Globals
//
char sdataA[6] = "TEST!!";    // Send data for SCI-A
char rdataA[6];    // Received data for SCI-A
Uint16 rdata_pointA; // Used for checking the received data

//
// Function Prototypes
//
interrupt void sciaTxFifoIsr(void);
interrupt void sciaRxFifoIsr(void);
void scia_fifo_init(void);
void error(void);

//
// Main
//
void main(void)
{
   Uint16 i;

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

   rdata_pointA = sdataA[0];

//
// Enable interrupts required for this example
//
   PieCtrlRegs.PIECTRL.bit.ENPIE = 1;   // Enable the PIE block
   PieCtrlRegs.PIEIER9.bit.INTx1 = 1;   // PIE Group 9, INT1
   PieCtrlRegs.PIEIER9.bit.INTx2 = 1;   // PIE Group 9, INT2
   IER = 0x100;                         // Enable CPU INT
   EINT;

//
// Step 6. IDLE loop. Just sit and loop forever (optional):
//
    for(;;);
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
    Uint16 i;

    for(i=0; i< 6; i++)
    {
       SciaRegs.SCITXBUF.all=sdataA[i];  // Send data
    }

//    for(i=0; i< 8; i++)                  // Increment send data for next cycle
//    {
//       sdataA[i] = (sdataA[i]+1) & 0x00FF;
//    }

    SciaRegs.SCIFFTX.bit.TXFFINTCLR=1;   // Clear SCI Interrupt flag
    PieCtrlRegs.PIEACK.all|=0x100;       // Issue PIE ACK
}

//
// sciaRxFifoIsr - SCIA Receive FIFO ISR
//
interrupt void sciaRxFifoIsr(void)
{
    Uint16 i;

    for(i=0;i<6;i++)
    {
       rdataA[i]=(char)SciaRegs.SCIRXBUF.all;  // Read data
    }

//    for(i=0;i<2;i++)                     // Check received data
//    {
//       if(rdataA[i] != ( (rdata_pointA+i) & 0x00FF) )
//       {
//           error();
//       }
//    }

    rdata_pointA = (rdata_pointA+1) & 0x00FF;

    SciaRegs.SCIFFRX.bit.RXFFOVRCLR=1;   // Clear Overflow flag
    SciaRegs.SCIFFRX.bit.RXFFINTCLR=1;   // Clear Interrupt flag

    PieCtrlRegs.PIEACK.all|=0x100;       // Issue PIE ack
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
   SciaRegs.SCIHBAUD.all = 0x0000;  //MSB of baud period
   SciaRegs.SCILBAUD.all = SCI_PRD;  //LSB of baud period
   SciaRegs.SCICCR.bit.LOOPBKENA = 0; // Enable loop back
   SciaRegs.SCIFFTX.all = 0xC028;
   SciaRegs.SCIFFRX.all = 0x0028;
   SciaRegs.SCIFFCT.all = 0x00;

   SciaRegs.SCICTL1.all = 0x0023;     // Relinquish SCI from Reset
   SciaRegs.SCIFFTX.bit.TXFIFORESET = 1;
   SciaRegs.SCIFFRX.bit.RXFIFORESET = 1;
}

//
// End of file
//
