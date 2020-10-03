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
 *     Version:		1.50
 */

/********************************************************************************************
* VERSION HISTORY
********************************************************************************************
* 	v1.50 - 10/02/2015
* 		Objective 5, flashing on queue empty implemented (slow with
* 		vTaskDelay).
*
* 	v1.45 - 10/02/2015
* 		TaskSW, Overrode previous buttons with switches.
*
* 	v1.40 - 10/02/2015
* 		Implemented task TaskSW.
*
* 	v1.35 - 10/02/2015
* 		Allowed button data to be zero.
*
* 	v1.30 - 10/02/2015
* 		Added task TaskBTN, priority 2 task which transmits button
* 		presses to xQueueBtnSw, whenever a new non-zero button
* 		combination is depressed.  Debounce stops when the buttons are
* 		released.
*
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
 *   	TaskLED := priority 2, receives and encodes the LEDs to
 *   	activate from xQueueBtnSw every 10 seconds with block time of
 *   	30 seconds.
 *
 *   	TaskBTN := priority 2, send button values to xQueueBtnSw
 *   	when the button changes with debounce to low.
 *
 *   	TaskSW  := priority 2, send switch values to xQueueBtnSw
 *   	when the switch changes.
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

typedef char* String;

/* GPIO instances */
XGpio  InInst;									/* GPIO Device driver instance for input */
XGpio OutInst;									/* GPIO Device driver instance for output */

/* time and queue constants */
#define TIMER_ID	1
#define DELAY_10_SECONDS	10000UL
#define DELAY_1_SECOND		1000UL
#define	TASKLED_DELAY		10000UL	/* wait between LED updates, 10 s */
#define	ERROR_DUTY_ON		1000UL	/* error duty cycle  on time */
#define	ERROR_DUTY_OFF		1000UL	/* error duty cycle off time */
#define	DEBOUNCE_DELAY		250UL	/* wait to debounce, 0.250 s */
#define	QUEUE_RX_BLOCK		30000UL	/* queue receiving block time, 30 s */
#define	QUEUE_TX_BLOCK		30000UL	/* queue transmitting block time, 30 s */
#define TIMER_CHECK_THRESHOLD	9
#define	QUEUE_BTN_SW_LEN	10u

/* input data constants */
#define LED_INIT	0b0000
#define BTN_INIT	0b0000
#define	BTN_SHIFT	0
#define BTN_MASK	0b1111
#define  SW_INIT	0b0000
#define	 SW_SHIFT	4
#define  SW_MASK	0b1111
/*-----------------------------------------------------------*/

/* The TaskLED, TaskBTN, TaskSW tasks as described at the top
 * of this file. */
static void prvTaskLED( void *pvParameters );
static void prvTaskBTN( void *pvParameters );
static void prvTaskSW ( void *pvParameters );
static void vTimerCallback( TimerHandle_t pxTimer );
/*-----------------------------------------------------------*/

/* The TaskLED, TaskBTN, TaskSW tasks as described at the top
 * of this file. */
static TaskHandle_t xTaskLED;
static TaskHandle_t xTaskBTN;
static TaskHandle_t xTaskSW ;
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

	xTaskCreate( 	prvTaskBTN,
					( const char * ) "TaskBTN",
					configMINIMAL_STACK_SIZE,
					NULL,
					2, /* priority */
					&xTaskBTN );

	xTaskCreate( 	prvTaskSW ,
					( const char * ) "TaskSW ",
					configMINIMAL_STACK_SIZE,
					NULL,
					2, /* priority */
					&xTaskSW  );

	/* Create the queue used by the tasks.  It has room for 10 items
	 * to encode each new button depressed and new total switch
	 * settings. */
	xQueueBtnSw = xQueueCreate( QUEUE_BTN_SW_LEN,
								sizeof( HWdata ) );	/* Each space holds a char. */

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

	/* initialize the GPIO driver for output */
	Status = XGpio_Initialize(&OutInst, OUT_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("GPIO output failed!\r\n");
		return 0;
	}
	/* set LEDs to output */
	XGpio_SetDataDirection(&OutInst, LED_CHANNEL, 0x00);

	/* initialize the GPIO driver for input */
	Status = XGpio_Initialize(& InInst,  IN_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("GPIO input failed!\r\n");
		return 0;
	}
	/* set buttons to input */
	XGpio_SetDataDirection(& InInst, BTN_CHANNEL, 0xFF);
	/* set switches to input */
	XGpio_SetDataDirection(& InInst,  SW_CHANNEL, 0xFF);

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
	/* all variables local to TaskLED */
	String task = "TaskLED";
	int unsigned ledData = LED_INIT;

	int unsigned iBtn; /* switch counter, unsigned ensures logical RS */
	int unsigned btnData;
	int unsigned swcData;
	int unsigned nextBtnData;
	int unsigned nextSwcData;

	char unsigned rxData;
	BaseType_t reception;

	int ledError = 0b1111;

	const TickType_t rxBlockTicks = pdMS_TO_TICKS( QUEUE_RX_BLOCK );
	const TickType_t delayTicks = pdMS_TO_TICKS( TASKLED_DELAY );
	const TickType_t errorDutyOnTicks = pdMS_TO_TICKS( ERROR_DUTY_ON );
	const TickType_t errorDutyOffTicks = pdMS_TO_TICKS( ERROR_DUTY_OFF );

	xil_printf("[%s] started\n", task);

	for( ;; )
	{
		/* Block to wait for data arriving on the queue. */
		reception =
				xQueueReceive( 	xQueueBtnSw,				/* The queue being read. */
								&rxData,	/* Data is read into this address. */
								rxBlockTicks );	/* Block by the receiving block time. */

		/* if the queue is empty, display an error and stop */
		if (reception == errQUEUE_EMPTY) {
			xil_printf("[%s] ERROR: QUEUE IS EMPTY\n", task);
			XGpio_DiscreteWrite(&OutInst, LED_CHANNEL, ledError);
			ledError = ~ledError;
			vTaskDelay( (ledError != 0b0000) ? errorDutyOnTicks : errorDutyOffTicks );
			continue;
		}

		/* log the data received */
		xil_printf("[%s] data received:\t0x%x\t-> ", task, rxData);

		/* get the button and switch data */
		nextBtnData = ((rxData >> BTN_SHIFT) & BTN_MASK);
		nextSwcData = ((rxData >>  SW_SHIFT) &  SW_MASK);

		/* update the data that are nonzero */
		/* however, if buttons are 0, clear switches*/
		if (nextBtnData != 0b0000) {
			btnData = nextBtnData;
		}
		if ((nextSwcData != 0b0000) || (nextBtnData == 0b0000)) {
			swcData = nextSwcData;
		}

		/* ignore button presses that occurred while the switch
		 * was off */
		btnData &= swcData;

		xil_printf("switch: 0x%x\tbutton: 0x%x\n", swcData, btnData);

		/* loop through the switches */
		for (iBtn = 0b1000; iBtn; iBtn >>= 1) {
			/* if the switch matches both a switch in swcData
			 * and also a button in btnData,
			 */
			if ((iBtn & btnData) == iBtn) {
				/* activate the corresponding LED */
				ledData |= iBtn;
				xil_printf("[%s] LED 0x%x\ton \n", task, iBtn);
			}
			else {
				/* otherwise, deactivate the corresponding LED */
				ledData &= ~iBtn;
				xil_printf("[%s] LED 0x%x\toff\n", task, iBtn);
			}
		}

		/* update LED display */
		XGpio_DiscreteWrite(&OutInst, LED_CHANNEL, ledData);

		/* wait before reading again */
		vTaskDelay( delayTicks );
	}
} /* end static void prvTaskLED( void *pvParameters ) */

/*-----------------------------------------------------------*/
static void prvTaskBTN( void *pvParameters )
{
	/* all variables local to TaskBTN */
	String task = "TaskBTN";
	int unsigned data = BTN_INIT;
	int unsigned nextData;
	int unsigned encodedData;

	const TickType_t debounceTicks = pdMS_TO_TICKS( DEBOUNCE_DELAY );
	const TickType_t txBlockTicks = pdMS_TO_TICKS( QUEUE_TX_BLOCK );

	xil_printf("[%s] started\n", task);

	for( ;; )
	{
		/* read in the button data */
		nextData = XGpio_DiscreteRead(&InInst, BTN_CHANNEL);
		/* if no nonzero change, skip this loop run */
		if ((nextData == 0b0000) || (nextData == data)) continue;

		/* debounce by looping until no button pressed */
		while (XGpio_DiscreteRead(&InInst, BTN_CHANNEL) == 0b0000) {
			/* wait in loop */
			vTaskDelay( debounceTicks );
		}

		/* log the buttons pressed */
		xil_printf("[%s] buttons pressed:\t0x%x -> 0x%x\n",
				task, data, nextData);

		/* since change, update the data */
		data = nextData;

		/* encode the data */
		encodedData = ((data & BTN_MASK) << BTN_SHIFT);

		/* Send the next value on the queue.  The queue should always be
		empty at this point so a block time of 0 is used. */
		xQueueSend( xQueueBtnSw,			/* The queue being written to. */
					&encodedData, /* The address of the data being sent. */
					txBlockTicks );	/* The block time. */
	}
}

/*-----------------------------------------------------------*/
static void prvTaskSW ( void *pvParameters )
{
	/* all variables local to TaskSW  */
	String task = "TaskSW ";
	int unsigned data =  SW_INIT;
	int unsigned nextData;
	int unsigned encodedData;

	const TickType_t txBlockTicks = pdMS_TO_TICKS( QUEUE_TX_BLOCK );

	for( ;; )
	{
		/* read in the button data */
		nextData = XGpio_DiscreteRead(&InInst,  SW_CHANNEL);
		/* if no change, skip this loop run */
		if (nextData == data) continue;

		/* no need to debounce */

		/* since change, update the data */
		data = nextData;

		/* log the switches thrown */
		xil_printf("[%s] switches thrown:\t0x%x -> 0x%x\n",
				task, data, nextData);

		/* encode the data */
		encodedData = ((data &  SW_MASK) <<  SW_SHIFT);

		/* Send the next value on the queue.  The queue should always be
		empty at this point so a block time of 0 is used. */
		xQueueSend( xQueueBtnSw,			/* The queue being written to. */
					&encodedData, /* The address of the data being sent. */
					txBlockTicks );	/* The block time. */
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

//	vTaskDelete( xTaskLED );
//	vTaskDelete( xTaskBTN );
}

