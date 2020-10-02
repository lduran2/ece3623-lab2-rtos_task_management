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
 * initials_FreeRTOStm.c
 *
 *  Created on:		10/01/2020
 *      Author:		Leomar Duran
 *     Version:		1.25
 */

/********************************************************************************************
* VERSION HISTORY
********************************************************************************************
* 	v1.25 - 10/01/2015
* 		Fixed TaskLED encoding to ready for TaskBTN.
*
* 	v1.2 - 10/01/2015
* 		Added task TaskLED, priority 2 task which reads
* 		xQueueBtnSw every 10 seconds with block of 30 seconds
* 		for switch and button combinations to control the LEDs.
*
* 	v1.1 - 10/01/2015
* 		Added queue xQueueBtnSw, 10 item queue for button and switch
* 		data.
*
*	v1.0 - Unknown
*		FreeRTOS Hello World template on lab 2 hardware.
*******************************************************************************************/
/*
 * Tasks:
 *   	TaskLED := priority 2, received a button value and total
 *   	current switch settings from xQueueBtnSw every 10 seconds.
 */

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

/* GPIO definitions */
#define  IN_DEVICE_ID	XPAR_AXI_GPIO_0_DEVICE_ID	/* GPIO device for input */
#define OUT_DEVICE_ID	XPAR_AXI_GPIO_1_DEVICE_ID	/* GPIO device that LEDs are connected to */
#define BTN_CHANNEL	1								/* GPIO port for buttons */
#define  SW_CHANNEL	2								/* GPIO port for switches */
#define LED_CHANNEL	1								/* GPIO port for LEDs */

/* GPIO instances */
XGpio  InInst;									/* GPIO Device driver instance for input */
XGpio OutInst;									/* GPIO Device driver instance for output */

/* time and queue constants */
#define TIMER_ID	1
#define DELAY_10_SECONDS	10000UL
#define DELAY_1_SECOND		1000UL
#define	TASKLED_DELAY		10000UL
#define	QUEUE_RX_BLOCK		30000UL	/* queue receiving block time, 30 s */
#define TIMER_CHECK_THRESHOLD	9
#define	QUEUE_BTN_SW_LEN	10u

/* input data constants */
#define LED_INIT	0b0000
#define	BTN_SHIFT	0
#define BTN_MASK	0b1111
/*-----------------------------------------------------------*/

/* The Tx and Rx tasks as described at the top of this file. */
static void prvTxTask( void *pvParameters );
static void prvTaskLED( void *pvParameters );
static void vTimerCallback( TimerHandle_t pxTimer );
/*-----------------------------------------------------------*/

/* The queue used by the Tx and Rx tasks, as described at the top of this
file. */
static TaskHandle_t xTxTask;
static TaskHandle_t xTaskLED;
static QueueHandle_t xQueueBtnSw = NULL;
static TimerHandle_t xTimer = NULL;
char HWdata = 0;	/* the data is encoded in a byte */
long RxtaskCntr = 0;

int main( void )
{
	int Status;

	const TickType_t x10seconds = pdMS_TO_TICKS( DELAY_10_SECONDS );

	xil_printf( "Starting all tasks . . .\n" );

	/* Create the two tasks.  The Tx task is given a lower priority than the
	Rx task, so the Rx task will leave the Blocked state and pre-empt the Tx
	task as soon as the Tx task places an item in the queue. */
	xTaskCreate( 	prvTaskLED,					/* The function that implements the task. */
					( const char * ) "TaskLED",	/* Text name for the task, provided to assist debugging only. */
					configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
					NULL, 						/* The task parameter is not used, so set to NULL. */
					2, /* priority */			/* The task runs at the idle priority. */
					&xTaskLED );

	xTaskCreate( 	prvTxTask,
					( const char * ) "Tx",
					configMINIMAL_STACK_SIZE,
					NULL,
					tskIDLE_PRIORITY,
					&xTxTask );

	/* Create the queue used by the tasks.  It has room for 10 items
	 * to encode each new button depressed and new total switch
	 * settings.
	 * The Rx task has a higher priority
	than the Tx task, so will preempt the Tx task and remove values from the
	queue as soon as the Tx task writes to the queue - therefore the queue can
	never have more than one item in it. */
	xQueueBtnSw = xQueueCreate( QUEUE_BTN_SW_LEN,	/* There is only one space in the queue. */
								sizeof( HWdata ) );	/* Each space in the queue is large enough to hold a uint32_t. */

	/* Check the queue was created. */
	configASSERT( xQueueBtnSw );

	/* Create a timer with a timer expiry of 10 seconds. The timer would expire
	 after 10 seconds and the timer call back would get called. In the timer call back
	 checks are done to ensure that the tasks have been running properly till then.
	 The tasks are deleted in the timer call back and a message is printed to convey that
	 the example has run successfully.
	 The timer expiry is set to 10 seconds and the timer set to not auto reload. */
	xTimer = xTimerCreate( (const char *) "Timer",
							x10seconds,
							pdFALSE,
							(void *) TIMER_ID,
							vTimerCallback);
	/* Check the timer was created. */
	configASSERT( xTimer );

	/* start the timer with a block time of 0 ticks. This means as soon
	   as the schedule starts the timer will start running and will expire after
	   10 seconds */
	xTimerStart( xTimer, 0 );

	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	/* initialize the GPIO driver for the LEDs */
	Status = XGpio_Initialize(&OutInst, OUT_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("GPIO output to the LEDs failed!\r\n");
		return 0;
	}
	/* set LEDs to output */
	XGpio_SetDataDirection(&OutInst, LED_CHANNEL, 0x00);

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
	/* all variables local to TaskLED */
	char rxData;
	int unsigned iSwc; /* switch counter, unsigned ensures logical RS */
	int ledData = LED_INIT;
	int btnData;
	int swcData = 0b1111; /* for now all switches as if on */

	const TickType_t rxBlockSeconds = pdMS_TO_TICKS( QUEUE_RX_BLOCK );
	const TickType_t delaySeconds = pdMS_TO_TICKS( TASKLED_DELAY );

	for( ;; )
	{
		/* Block to wait for data arriving on the queue. */
		xQueueReceive( 	xQueueBtnSw,				/* The queue being read. */
						&rxData,	/* Data is read into this address. */
						rxBlockSeconds );	/* Block by the receiving block time. */

		/* get the button and switch data */
		btnData = (btnData >> BTN_SHIFT) & BTN_MASK;

		/* loop through the switches */
		for (iSwc = 0b1000; iSwc; iSwc >>= 1) {
			/* if the switch matches both a switch in rxData.swcData
			 * and also a button in rxData.btnData
			 */
			if ((iSwc & swcData & btnData) == iSwc) {
				/* activate the corresponding LED */
				ledData |= iSwc;
				xil_printf("[TaskLED] LED 0x%x on \n", iSwc);
			}
			else {
				/* otherwise deactivate the corresponding LED */
				ledData &= ~iSwc;
				xil_printf("[TaskLED] LED 0x%x off\n", iSwc);
			}
		}

		/* update LED display */
		XGpio_DiscreteWrite(&OutInst, LED_CHANNEL, ledData);

		/* wait before reading again */
		vTaskDelay( delaySeconds );
	}
} /* end static void prvTaskLED( void *pvParameters ) */

/*-----------------------------------------------------------*/
static void prvTxTask( void *pvParameters )
{
const TickType_t x1second = pdMS_TO_TICKS( DELAY_1_SECOND );

	for( ;; )
	{
		/* Delay for 1 second. */
		vTaskDelay( x1second );

		/* Send the next value on the queue.  The queue should always be
		empty at this point so a block time of 0 is used. */
		xQueueSend( xQueueBtnSw,			/* The queue being written to. */
					&HWdata, /* The address of the data being sent. */
					0UL );			/* The block time. */
	}
}

/*-----------------------------------------------------------*/
static void vTimerCallback( TimerHandle_t pxTimer )
{
	long lTimerId;
	configASSERT( pxTimer );

	lTimerId = ( long ) pvTimerGetTimerID( pxTimer );

	if (lTimerId != TIMER_ID) {
		xil_printf("FreeRTOS Hello World Example FAILED");
	}

	/* If the RxtaskCntr is updated every time the Rx task is called. The
	 Rx task is called every time the Tx task sends a message. The Tx task
	 sends a message every 1 second.
	 The timer expires after 10 seconds. We expect the RxtaskCntr to at least
	 have a value of 9 (TIMER_CHECK_THRESHOLD) when the timer expires. */
	if (RxtaskCntr >= TIMER_CHECK_THRESHOLD) {
		xil_printf("FreeRTOS Hello World Example PASSED");
	} else {
		xil_printf("FreeRTOS Hello World Example FAILED");
	}

	//vTaskDelete( xTaskLED );
	vTaskDelete( xTxTask );
}

