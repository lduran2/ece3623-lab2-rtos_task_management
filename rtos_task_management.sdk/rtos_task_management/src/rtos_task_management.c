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
 *    Version: 	1.5
 */

/********************************************************************************************
* VERSION HISTORY
********************************************************************************************
* 	v1.5 - 16 September 2020
* 		Added TaskBTN feature that controls TaskSW.
*
* 	v1.4 - 16 September 2020
* 		Added TaskSW  feature that controls TaskLED and TaskBTN.
*
* 	v1.3 - 16 September 2020
* 		Stubbed TaskSW.  Optimized TaskBTN.
*
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
*
* TaskSW  := reads the switches to control the other tasks
*
*******************************************************************************************/

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
/* Xilinx includes. */
#include "xil_printf.h"
#define	printf	xil_printf
#include "xparameters.h"
#include "xgpio.h"
#include "xstatus.h"

/* task definitions */
#define	DO_TASK_LED	1								/* whether to do TaskLED */
#define	DO_TASK_BTN	1								/* whether to do TaskBTN */
#define	DO_TASK_SW 	1								/* whether to do TaskSW */

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
XGpio  InInst;									/* GPIO Device driver instance for input */
XGpio OutInst;									/* GPIO Device driver instance for output */

/* bit masks for on */
#define	      ON4 	0b1111						/* 4 bit on  */
#define	BTN10_ON  	0b1100
#define	 BTN2_ON  	0b1011
#define	 BTN3_ON  	0b0111
#define	 SW10_ON  	0b1100
#define	  SW2_ON  	0b1011
#define	  SW3_ON  	0b0111

/* bit masks for off */
#define       OFF4	0b0000						/* 4 bit off */
#define	BTN10_OFF 	0b0011
#define	 BTN2_OFF 	0b0100
#define	 BTN3_OFF 	0b1000
#define	 SW10_OFF 	0b0011
#define	  SW2_OFF 	0b0100
#define	  SW3_OFF 	0b1000
/*-----------------------------------------------------------*/

/* The tasks as described at the top of this file. */
static void prvTaskLED( void *pvParameters );
static void prvTaskBTN( void *pvParameters );
static void prvTaskSW ( void *pvParameters );
/*-----------------------------------------------------------*/

/* The task handles to control other tasks. */
static TaskHandle_t xTaskLED;
static TaskHandle_t xTaskBTN;
static TaskHandle_t xTaskSW;
/* The LED Counter. */
int ledCntr = LED;
/* The last value of button. */
int btn;

int main( void )
{
	int Status;

	if (DO_TASK_LED) {
		printf( "Starting TaskLED. . .\r\n" );
		/* Create TaskLED with priority 1. */
		xTaskCreate( 	prvTaskLED, 				/* The function that implements the task. */
				( const char * ) "TaskLED", 		/* Text name for the task, provided to assist debugging only. */
						configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
						NULL, 						/* The task parameter is not used, so set to NULL. */
						( UBaseType_t ) 1,			/* The next to lowest priority. */
						&xTaskLED );
		printf( "\tSuccessful\r\n" );
	}

	if (DO_TASK_BTN) {
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
	}

	if (DO_TASK_SW) {
		printf( "Starting TaskSW . . .\r\n" );
		/* Create TaskSW with priority 1. */
		xTaskCreate(
					prvTaskSW,						/* The function implementing the task. */
				( const char * ) "TaskSW",			/* Text name provided for debugging. */
					configMINIMAL_STACK_SIZE,		/* Not much need for a stack. */
					NULL,							/* The task parameter, not in use. */
					( UBaseType_t ) 1,				/* The next to lowest priority. */
					&xTaskSW );
		printf( "\tSuccessful\r\n" );
	}

	/* initialize the GPIO driver for the LEDs */
	Status = XGpio_Initialize(&OutInst, OUT_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		printf("GPIO output to the LEDs failed!\r\n");
		return 0;
	}
	/* set LEDs to output */
	XGpio_SetDataDirection(&OutInst, LED_CHANNEL, 0x00);

	/* initialize the GPIO driver for the buttons */
	Status = XGpio_Initialize( &InInst,  IN_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		printf("GPIO input from the buttons and switches failed!\r\n");
		return 0;
	}
	/* set buttons to input */
	XGpio_SetDataDirection( &InInst, BTN_CHANNEL, 0xFF);
	/* set switches to input */
	XGpio_SetDataDirection( &InInst,  SW_CHANNEL, 0xFF);

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
		XGpio_DiscreteWrite(&OutInst, LED_CHANNEL, ledCntr);
		printf("TaskLED: count := %d\r\n", ledCntr);

		/* Delay for visualization. */
		vTaskDelay( LEDseconds );

		/* update the counter */
		++ledCntr;
	}
}


/*-----------------------------------------------------------*/
static void prvTaskBTN( void *pvParameters )
{
const TickType_t BTNseconds = pdMS_TO_TICKS( BTN_DELAY );
	int nextBtn;	/* Hold the new button value. */
	for( ;; )
	{
		/* Read input from the buttons. */
		nextBtn = XGpio_DiscreteRead(&InInst, BTN_CHANNEL);

		/* Debounce: */
		/* If the button has changed, */
		if ( btn != nextBtn ) {
			btn = nextBtn;	/* store the old value */
			vTaskDelay( BTNseconds );	/* delay until the end of the bounce */
			nextBtn = XGpio_DiscreteRead( &InInst, BTN_CHANNEL );	/* read again */
			/* if the button value is still the same, continue */
			if ( btn == nextBtn ) {
				printf("TaskBTN: Button changed to 0x%x.\r\n", btn);

				btn = nextBtn;	/* update btn */
				/* If BTN2 is depressed, regardless of the
				 * status of BTN0 and BTN1, then TaskLED is
				 * resumed.  So BTN2 gets priority. */
				if ( ( btn | BTN2_ON ) == ON4 ) {
					vTaskResume(xTaskLED);
					printf("TaskBTN: TaskLED is resumed.\r\n");
				}
				/* Otherwise if BTN0 and BTN1 are depressed at
				 * some point together then TaskLED is
				 * suspended */
				else if ( ( btn | BTN10_ON ) == ON4 ) {
					vTaskSuspend(xTaskLED);
					printf("TaskBTN: TaskLED is suspended.\r\n");
				}

				/* This logic below is independent from those above. */
				/* If BTN3 is depressed then TaskSW is suspended */
				if ( ( btn | BTN3_ON ) == ON4 ) {
					vTaskSuspend(xTaskSW);
					printf("TaskBTN: TaskSW  is suspended.\r\n");
				}
				/* Either BTN3 is depressed or it is released. */
				/* If BTN3 is released then TaskSW is resumed */
				else {
					vTaskResume(xTaskSW);
					printf("TaskBTN: TaskSW  is resumed.\r\n");
				}

			} /* end if ( btn == nextBtn ) check if button is consistent */
		} /* end if ( btn != nextBtn ) check if button has changed since last call */
	} /* end for( ;; ) */
}


/*-----------------------------------------------------------*/
static void prvTaskSW( void *pvParameters )
{
	int sw;	/* Hold the current switch value. */
	for( ;; )
	{
		/* Read input from the switches. */
		sw = XGpio_DiscreteRead(&InInst,  SW_CHANNEL);

		/* If SW0 and SW1 are ON together at some point then
		 * TaskBTN is suspended */
		if ( ( sw | SW10_ON ) == ON4 ) {
			vTaskSuspend(xTaskBTN);
		}
		/* Switches 0 and 1 cannot both be off if they're both on. */
		/* If SW0 and SW1 are OFF then TaskBTN is resumed. */
		else if ( (sw & SW10_OFF ) == OFF4 ) {
			vTaskResume(xTaskBTN);
		}

		/* This logic below is independent from those above. */
		/* If SW3 is ON then TaskLED is suspended. */
		if ( ( sw | SW3_ON ) == ON4 ) {
			vTaskSuspend(xTaskLED);
			/* If SW3 is then turned OFF, then resume TaskLED. */
			sw = XGpio_DiscreteRead(&InInst,  SW_CHANNEL);
			if ( ( sw & SW3_OFF ) == OFF4 ) {
				vTaskResume(xTaskLED);
				printf("TaskSW : TaskLED is resumed.\r\n");
			}
		}
	}
}
