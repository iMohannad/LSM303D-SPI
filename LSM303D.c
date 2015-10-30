/*
===============================================================================
 Name        : LSM303D.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/

#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include <cr_section_macros.h>
#include <stdio.h>


#define SSP_BUFSIZE	16
#define FIFOSIZE	8

void SSP_init();
//void SSPSend(uint8_t address, uint8_t *buf, uint32_t length);
//void SSPReceive(uint8_t address, uint8_t *buf, uint32_t length);
void SSPSend(uint8_t address, uint8_t buf);
uint8_t SSPReceive(uint8_t address);
void SSP_SSEL(int port, int toggle);
int is_Tx_not_full();
int is_Tx_empty();
int is_Rx_full();
int is_Rx_not_empty();
int is_busy();


int main(void) {

	int j;

	SSP_init();
	uint8_t sendBuf[FIFOSIZE], recBuf[FIFOSIZE];

	//Write on CTRL1 Register (address = 0x20)
	//Enable X, Y, Z Accelerometer
	SSPSend(0x20, 0x87);

	printf("CTRL1 > %x\n", SSPReceive(0x20));
	printf("CTRL3 > %x\n", SSPReceive(0x23));

	//Write on CTRL3 Register (address = 0x23)
	SSPSend(0x23, 0x40);


	uint8_t ACC_Data[6];

	// Force the counter to be placed into memory
	volatile static int i = 0 ;
	// Enter an infinite loop, just incrementing a counter
	while(1) {
		ACC_Data[0] = SSPReceive(0x28);
		printf("ACC_Data[0] > %x\n", ACC_Data[0]);

		ACC_Data[1] = SSPReceive(0x29);
		printf("ACC_Data[1] > %x\n", ACC_Data[1]);

		ACC_Data[2] = SSPReceive(0x2A);
		printf("ACC_Data[2] > %x\n", ACC_Data[2]);

		ACC_Data[3] = SSPReceive(0x2B);
		printf("ACC_Data[3] > %x\n", ACC_Data[3]);


		ACC_Data[4] = SSPReceive(0x2C);
		printf("ACC_Data[4] > %x\n", ACC_Data[4]);

		ACC_Data[5] = SSPReceive(0x2D);
		printf("ACC_Data[5] > %x\n", ACC_Data[5]);


    }
    return 0;
}


void SSP_init(){

	uint8_t dummy;

	printf("SSP Init\n");
	//Power the SPP0 Peripheral
	LPC_SC -> PCONP |= 1 << 21;

	//Divide the SSP0 clock by 4
	LPC_SC -> PCLKSEL1 &= ~(0x3 << 10);

	//Configure P0.15 to SPP0 CLK pin
	LPC_PINCON -> PINSEL0 |= 1 << 31;

	/* Configure P0.16 to SSEL   */
	//LPC_PINCON -> PINSEL1 |= 1 << 1;
	LPC_GPIO0 -> FIODIR |= 1 << 16;
	LPC_GPIO0 -> FIOSET |= 1 << 16;

	//Configure P0.17 to MISO0
	LPC_PINCON -> PINSEL1 |= 1 << 3;

	//Configure P0.18 to MOSI0
	LPC_PINCON -> PINSEL1 |= 1 << 5;


	//PULL DOWN
	LPC_PINCON -> PINMODE0 |= 0x3 << 30;
	LPC_PINCON -> PINMODE1 |= (0x3) | (0x3 << 2) | (0x3<<4);

	//SPP0 work on Master Mode and SSP enable
	LPC_SSP0 -> CR1 |= (1 << 1);

	/*
	 * SSP0 Control Register 0
	 * Set DSS data to 8-bit	(7 at 3:0)
	 * frame format SPI			(00 at 5:4)
	 * CPOL =0, CPHA=0, and SCR is 15
	 */
	LPC_SSP0 -> CR0 = 0x0707;

	/* SSPCPSR clock prescale register, master mode, minimum divisor is 0x02*/
	LPC_SSP0 -> CPSR = 0x4;

	int i=0;
	/* Clear RxFIFO */
	for(i=0; i<FIFOSIZE; i++){
		dummy = LPC_SSP0 -> DR;
	}


}

void SSPSend(uint8_t address, uint8_t buf){

	SSP_SSEL(0,0);
	//Send Address of the register to write on
	//Move only if is not busy and TX FIFO is NOT FULL
	while(is_busy() & !is_Tx_not_full());
	LPC_SSP0 -> DR = (address);

	while(is_busy());
	uint16_t dummy= LPC_SSP0 -> DR;
	printf("dummy = %x\n", dummy);
	SSP_SSEL(0,1);
	SSP_SSEL(0,0);

	//Send Data
	while(is_busy() & !is_Tx_not_full());
	LPC_SSP0 -> DR = buf;

	while(is_busy());
	dummy = LPC_SSP0 -> DR;
	printf("dummy2 = %x\n", dummy);

	SSP_SSEL(0,1);
}


uint8_t SSPReceive(uint8_t address){
	address |= 1 << 7; //READ Bit
	int i=0;
	SSP_SSEL(0,0);

	LPC_SSP0 -> DR = address;
	//Read only if it's not busy and the receiver FIFO is not empty
	//while(!is_Rx_not_empty())	printf("Empty.. LPC_SR > %x\n", LPC_SSP0->SR);
	while(is_busy() & !is_Rx_not_empty());
	uint8_t data = LPC_SSP0 -> DR;

	SSP_SSEL(0,1);

	return data;

}

/* Description : Manual set for SSP Chip Select (CS) */
void SSP_SSEL(int port, int toggle){
	if(port == 0){
		if(!toggle)
			LPC_GPIO0 -> FIOCLR |= 1<<16;
		else
			LPC_GPIO0 -> FIOSET |= 1<<16;
	}
	else if(port == 1){
		if(!toggle)
			LPC_GPIO1 -> FIOCLR |= 1<<6;
		else
			LPC_GPIO1 -> FIOSET |= 1<<6;
	}
}

/* Status Register
 * Bit	Symbol		Description
 * 0	TFE			Transmit FIFO empty
 * 1	TNF			Transmit FIFO Not Full
 * 2	RNE			Receiver FIFO Not Empty
 * 3	RFF			Receiver FIFO Full
 * 4	BSY			Busy sending/receiving
 */


/* returns 1 if Transmitter FIFO Not FULL
 * returns 0 if Transmitter FIFO is FULL
 */
int is_Tx_not_full(){
	uint32_t reg = LPC_SSP0 -> SR;
	return ((reg & 0x2) >> 1);
}

/* returns 1 if Transmitter FIFO empty
 * returns 0 if Transmitter FIFO is not empty
 */
int is_Tx_empty(){
	uint32_t reg = LPC_SSP0 -> SR;
	return (reg & 0x1);
}

/* returns 1 if Receiver FIFO FULL
 * returns 0 if Receiver FIFO is NOT FULL
 */
int is_Rx_full(){
	uint32_t reg = LPC_SSP0 -> SR;
	return ((reg & 0x4) >> 2);
}

/* returns 1 if Receiver FIFO Not empty
 * returns 0 if Receiver FIFO is empty
 */
int is_Rx_not_empty(){
	uint32_t reg = LPC_SSP0 -> SR;
	return (reg & (1 << 2)) >> 2;
}

int is_busy(){
	uint32_t reg = LPC_SSP0 -> SR;
	return ((reg & 0x10) >> 4);
}
