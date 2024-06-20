#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdio.h>
#include "SMM_MPS2.h"
#include "CMSDK_CM3.h"  

#define mainQUEUE_RECEIVE_TASK_PRIORITY    (tskIDLE_PRIORITY + 2)
#define mainQUEUE_SEND_TASK_PRIORITY       (tskIDLE_PRIORITY + 1)
#define mainQUEUE_LENGTH                   (1)
#define mainQUEUE_SEND_FREQUENCY_MS        (200 / portTICK_PERIOD_MS)

static void prvQueueReceiveTask(void *pvParameters);
static void prvQueueSendTask(void *pvParameters);

static QueueHandle_t xQueue = NULL;

// Define CMSDK_UART0_BASE and CMSDK_UART_TypeDef 
#ifndef CMSDK_UART0_BASE
#define CMSDK_UART0_BASE        (CMSDK_APB_BASE + 0x4000UL)
#endif

#define UART_CLOCK_FREQ 50000000UL  // Assuming UART clock is 50 MHz

#define DESIRED_BAUD_RATE 115200

void prvUART_Init(void)
{
    // Initialize UART
    CMSDK_UART0->BAUDDIV = (UART_CLOCK_FREQ / (DESIRED_BAUD_RATE * 16)) - 1;
    CMSDK_UART0->CTRL = (1 << CMSDK_UART_CTRL_TXEN_Pos) | (1 << CMSDK_UART_CTRL_RXEN_Pos);
}

void prvUART_Send(const char *pcMessage)
{
    // Send message via UART
    while (*pcMessage)
    {
        // Wait for TX ready
        while (!(CMSDK_UART0->STATE & CMSDK_UART_STATE_TXBF_Msk));
        CMSDK_UART0->DATA = *pcMessage++;
    }
}

void main_blinky(void)
{
    xQueue = xQueueCreate(mainQUEUE_LENGTH, sizeof(uint32_t));

    if (xQueue != NULL)
    {
        prvUART_Init();

        xTaskCreate(prvQueueReceiveTask, "Rx", configMINIMAL_STACK_SIZE, NULL,
                    mainQUEUE_RECEIVE_TASK_PRIORITY, NULL);

        xTaskCreate(prvQueueSendTask, "TX", configMINIMAL_STACK_SIZE, NULL,
                    mainQUEUE_SEND_TASK_PRIORITY, NULL);

        vTaskStartScheduler();
    }

    for (;;)
    {
    }
}

static void prvQueueSendTask(void *pvParameters)
{
    TickType_t xNextWakeTime;
    const uint32_t ulValueToSend = 100UL;

    (void)pvParameters;

    xNextWakeTime = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&xNextWakeTime, mainQUEUE_SEND_FREQUENCY_MS);

        if (xQueueSend(xQueue, &ulValueToSend, 0U) == pdPASS)
        {
            printf("Sent value %lu to the queue\n", ulValueToSend);
            prvUART_Send("Sent value 100 to the queue\n");
        }
        else
        {
            printf("Failed to send value %lu to the queue\n", ulValueToSend);
            prvUART_Send("Failed to send value 100 to the queue\n");
        }
    }
}

volatile uint32_t ulRxEvents = 0;
static void prvQueueReceiveTask(void *pvParameters)
{
    uint32_t ulReceivedValue;
    const uint32_t ulExpectedValue = 100UL;

    (void)pvParameters;

    // If CMSDK_FPGAIO is not defined correctly, comment out or replace this line
    // CMSDK_FPGAIO->LED = 0x3;

    for (;;)
    {
        if (xQueueReceive(xQueue, &ulReceivedValue, portMAX_DELAY) == pdPASS)
        {
            if (ulReceivedValue == ulExpectedValue)
            {
                vTaskDelay(1000);

                ulReceivedValue = 0U;
                ulRxEvents++;

                printf("LED toggled successfully\n");
                prvUART_Send("LED toggled successfully\n");
            }
            else
            {
                printf("Received unexpected value: %lu\n", ulReceivedValue);
                prvUART_Send("Received unexpected value\n");
            }
        }
        else
        {
            printf("Failed to receive from the queue\n");
            prvUART_Send("Failed to receive from the queue\n");
        }
    }
}
