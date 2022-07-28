#include <stdio.h>
#include <stdlib.h>
#include "diag/trace.h"

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"
#define CCM_RAM __attribute__((section(".ccmram")))

 TaskHandle_t  mytasksender = NULL;  // Defining task handles
 TaskHandle_t  mytaskreceiver = NULL;


SemaphoreHandle_t Sem1;    // Defining semaphores handles
SemaphoreHandle_t Sem2;

QueueHandle_t Taskqueue;	// Defining queue handle


static TimerHandle_t xTimer1 = NULL; // Defining Timers handles
static TimerHandle_t xTimer2 = NULL;

BaseType_t xTimer1Started, xTimer2Started;    //variables for checking timers start



static void prvTsenderCallback( TimerHandle_t xTimer ); // Declaring call back functions of timers
static void prvTreceiverCallback( TimerHandle_t xTimer );

int tot_sent_mess,tot_blocked_mess,tot_recieved_mess=0;
int Current_Period[]={100,140,180,220,260,300};

void Init();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

void mytasksend(void *p){

	char send_mess [50]; // An array of characters to read message in it
	Taskqueue= xQueueCreate (2,sizeof(send_mess)); //creating queue

	while(1){

		if(xSemaphoreTake(Sem1,portMAX_DELAY))  //Checking if semaphore is avaliable to take
		{
			sprintf(send_mess,"Time is %ld\n",xTaskGetTickCount());
			if(xQueueSend(Taskqueue,(void*)send_mess,(TickType_t) 0))  //check if sending is successful
			{
				//trace_printf("sent %s\n",mess);
				tot_sent_mess++;  //Increment total number of sent messages
			}
			else{
				//trace_puts("fail send");
				tot_blocked_mess++;  //Increment total number of sent messages
				}
			}
		}
}

void mytaskreceive(void *q){

	char mess_recv[50];
	while (1){
		if(tot_recieved_mess!=500){ 						//checks if total number of receieved messages equals 500
			 if(xSemaphoreTake(Sem2,portMAX_DELAY)){ 		//checks for semaphore availability
					if(xQueueReceive(Taskqueue,(void*)mess_recv,(TickType_t) 0)) //check if receiving is successful
					{
						//trace_printf("data received:%s\n",messi);
						//trace_printf("ticks %d\n",xTaskGetTickCount());
							tot_recieved_mess++;					 //if yes -->Increment total number of received messages
					}
		     }
		}
		else
			Init();

	}
}

int main(int argc, char* argv[])

{
	//creating timers
	xTimer1 = xTimerCreate( "Timer1", ( pdMS_TO_TICKS(100) ), pdTRUE, ( void * ) 0, prvTsenderCallback);
	xTimer2 = xTimerCreate( "Timer2", ( pdMS_TO_TICKS(200) ), pdTRUE, ( void * ) 0, prvTreceiverCallback);
	//creating semaphores
	Sem1=xSemaphoreCreateBinary();
	Sem2=xSemaphoreCreateBinary();
	//checks if timers are created then it starts them
	if( ( xTimer1 != NULL ) && ( xTimer2 != NULL ) )
	{  //if yes then start timers
		xTimer1Started = xTimerStart( xTimer1, 0);
		xTimer2Started = xTimerStart( xTimer2, 0);
	}
	if( xTimer1Started == pdPASS && xTimer2Started == pdPASS)
	{	//If timers started then create tasks
		xTaskCreate(mytaskreceive,"receiver Timer", 2048 , (void*)0 ,1, &mytaskreceiver);
		xTaskCreate(mytasksend,"sender Timer", 2048 , (void*)0 ,0, &mytasksender);
		Init();
		vTaskStartScheduler();
	}

	return 0;
}


static void prvTsenderCallback( TimerHandle_t xTimer )
{
		xSemaphoreGiveFromISR(Sem1,NULL);
}

static void prvTreceiverCallback ( TimerHandle_t xTimer )
{
     	xSemaphoreGiveFromISR(Sem2,NULL);
}
void Init(){
	static int index=0; // determine the index of current period
	trace_printf("\nSender Timer period : %d\n",Current_Period[index]); //
	trace_printf("Total message sent %d\nTotal message Blocked: %d\n",tot_sent_mess,tot_blocked_mess);
	if((Current_Period[index]<300) && (index<5)){//check whether we finished all elements of array
		//This is specifcally used for the first print of init function from main as this condition prevent the first run t0 enter this section
		//by using the number of sent messages as variable as the first run has tot_sent_mess=0 , but the rest of runs it will have value
		if((tot_sent_mess)){
			xQueueReset(Taskqueue);
			index++;
			xTimerChangePeriod( xTimer1, pdMS_TO_TICKS(Current_Period[index]), 0 );//change timer period with respect to new element
			//reset the two timers
			xTimerReset(xTimer1,0);
			xTimerReset(xTimer2,0);
		}

	}
	else{
	xTimerDelete(xTimer1,0);
	xTimerDelete(xTimer2,0);
	trace_puts("\nGame over");
	vTaskDelete(mytasksend);
	vTaskDelete(mytaskreceiver);
	}
	tot_sent_mess=tot_blocked_mess=tot_recieved_mess=0;
}


void vApplicationMallocFailedHook( void )
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	//for( ;; );
}

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	for( ;; );
}


void vApplicationIdleHook( void )
{
volatile size_t xFreeStackSpace;

	/* This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amout of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if( xFreeStackSpace > 100 )
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}

void vApplicationTickHook(void) {
}

StaticTask_t xIdleTaskTCB CCM_RAM;
StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE] CCM_RAM;

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {
  /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
  state will be stored. */
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

  /* Pass out the array that will be used as the Idle task's stack. */
  *ppxIdleTaskStackBuffer = uxIdleTaskStack;

  /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
  Note that, as the array is necessarily of type StackType_t,
  configMINIMAL_STACK_SIZE is specified in words, not bytes. */
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB CCM_RAM;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH] CCM_RAM;

/*configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize) {
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
  *ppxTimerTaskStackBuffer = uxTimerTaskStack;
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
