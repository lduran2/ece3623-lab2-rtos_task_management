/*
    Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
    Copyright (C) 2012 - 2018 Xilinx, Inc. All Rights Reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy of
    this software and associated documentation files (the "Software"), to deal in
    the Software without restriction, including without limitation the rights to
    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
    the Software, and to permit persons to whom the Software is furnished to do so,
    subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software. If you wish to use our Amazon
    FreeRTOS name, please do so in a fair use way that does not cause confusion.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
    FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
    COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
    IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    http://www.FreeRTOS.org
    http://aws.amazon.com/freertos


    1 tab == 4 spaces!
*/
/*
 * rtos_task_management.c
 *
 * Created on: 	15 September 2020 (based on FreeRTOS_Hello_World.c)
 *     Author: 	Leomar Duran
 *    Version: 	1.2
 */

/********************************************************************************************
* VERSION HISTORY
********************************************************************************************
* 	v1.2 - 16 September 2020
* 		Added TaskBTN feature that controls TaskLED.
*
* 	v1.1 - 15 September 2020
* 		Set up LED counter.
*
* 	v1.0 - 2017
* 		Started with FreeRTOS_Hello_World.c
*
*******************************************************************************************/

/********************************************************************************************
* TASK DESCRIPTION
********************************************************************************************
* TaskLED := a counter from 0 to 15, then looping back to 0 that is displayed in the LEDs.
*
* TaskBTN := reads the buttons to control the other tasks
*******************************************************************************************/

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
/* Xilinx includes. */
#include "xil_printf.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xstatus.h"
//#define	printf	xil_printf

/* GPIO definitions */
#define  IN_DEVICE_ID	XPAR_AXI_GPIO_0_DEVICE_ID	/* GPIO device for input */
#define OUT_DEVICE_ID	XPAR_AXI_GPIO_1_DEVICE_ID	/* GPIO device that LEDs are connected to */
#define LED 0b0000									/* Initial LED value - 0000 */
#define BTN_DELAY	250UL							/* button delay length for debounce */
#define LED_DELAY	500UL							/* LED delay length for visualization */
#define BTN_CHANNEL	1								/* GPIO port for buttons */
#define  SW_CHANNEL	2								/* GPIO port for switches */
#define LED_CHANNEL	1								/* GPIO port for LEDs */

/* GPIO instances */
XGpio BTNInst;									/* GPIO Device driver instance for buttons */
XGpio  SWInst;									/* GPIO Device driver instance for switches */
XGpio LEDInst;									/* GPIO Device driver instance for LEDs */

/* bit masks */
#define	BTN01	0b0001
#define	BTN2	0b0100
#define	BTN3	0b1000
#define	 SW0	0b0001
#define	 SW1	0b0010
#define	 SW2	0b0100
#define	 SW3	0b1000
/*-----------------------------------------------------------*/

/* The tasks as described at the top of this file. */
static void prvTaskLED( void *pvParameters );
static void prvTaskBTN( void *pvParameters );
/*-----------------------------------------------------------*/

/* The task handles to control other tasks. */
static TaskHandle_t xTaskLED;
static TaskHandle_t xTaskBTN;
/* The LED Counter. */
long LEDCntr = LED;

int main( void )
{
	int Status;

	printf( "Starting TaskLED. . .\r\n" );
	/* Create TaskLED with priority 1. */
	xTaskCreate( 	prvTaskLED, 				/* The function that implements the task. */
			( const char * ) "TaskLED", 		/* Text name for the task, provided to assist debugging only. */
					configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
					NULL, 						/* The task parameter is not used, so set to NULL. */
					( UBaseType_t ) 1,			/* The next to lowest priority. */
					&xTaskLED );
	printf( "\tSuccessful\r\n" );

	printf( "Starting TaskBTN. . .\r\n" );
	/* Create TaskBTN with priority 1. */
	xTaskCreate(
				prvTaskBTN,						/* The function implementing the task. */
			( const char * ) "TaskBTN",			/* Text name provided for debugging. */
				configMINIMAL_STACK_SIZE,		/* Not much need for a stack. */
				NULL,							/* The task parameter, not in use. */
				( UBaseType_t ) 1,				/* The next to lowest priority. */
				&xTaskBTN );
	printf( "\tSuccessful\r\n" );

	/* initialize the GPIO driver for the LEDs */
	Status = XGpio_Initialize(&LEDInst, OUT_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		printf("GPIO output to the LEDs failed!\r\n");
		return 0;
	}
	/* set LEDs to output */
	XGpio_SetDataDirection(&LEDInst, LED_CHANNEL, 0x00);

	/* initialize the GPIO driver for the buttons */
	Status = XGpio_Initialize(&BTNInst,  IN_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		printf("GPIO input to the buttons failed!\r\n");
		return 0;
	}
	/* set buttons to input */
	XGpio_SetDataDirection(&BTNInst, BTN_CHANNEL, 0xFF);

	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	/* If all is well, the scheduler will now be running, and the following line
	will never be reached.  If the following line does execute, then there was
	insufficient FreeRTOS heap memory available for the idle and/or timer tasks
	to be created.  See the memory management section on the FreeRTOS web site
	for more details. */
	for( ;; );
}


/*-----------------------------------------------------------*/
static void prvTaskLED( void *pvParameters )
{
const TickType_t LEDseconds = pdMS_TO_TICKS( LED_DELAY );

	for( ;; )
	{
		/* display the counter */
		XGpio_DiscreteWrite(&LEDInst, LED_CHANNEL, LEDCntr);

		/* Delay for visualization. */
		vTaskDelay( LEDseconds );

		/* update the counter */
		++LEDCntr;
	}
}


/*-----------------------------------------------------------*/
static void prvTaskBTN( void *pvParameters )
{
const TickType_t BTNseconds = pdMS_TO_TICKS( BTN_DELAY );
	int btn;	/* Hold the current button value. */
	for( ;; )
	{
		/* Read input from the buttons. */
		btn = XGpio_DiscreteRead(&BTNInst, BTN_CHANNEL);

		/* If BTN2 is depressed, regardless of the status of
		 * BTN0 and BTN1, then TaskLED is resumed.  So BTN2
		 * gets priority. */
		if ( ( btn & BTN2 ) == BTN2 ) {
			/* delay until the end of the bounce */
			vTaskDelay(BTNseconds);
			/* check again before acting */
			if ( ( btn & BTN2 ) == BTN2 ) {
				vTaskResume(xTaskLED);
			}
		}
		/* Otherwise if BTN0 and BTN1 are depressed at some
		 * point together then TaskLED is suspended */
		else if ( (btn & BTN01 ) == BTN01 ){
			/* delay until the end of the bounce */
			vTaskDelay(BTNseconds);
			/* check again before acting */
			if ( ( btn & BTN01 ) == BTN01 ) {
				vTaskSuspend(xTaskLED);
			}
		}
		//XGpio_DiscreteWrite(&LEDInst, LED_CHANNEL, btn);
	}
}
