/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "mt3620-baremetal.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "printf.h"

#include "nnom.h"
#include "image.h"
#include "weights.h"

#define APP_STACK_SIZE_BYTES		(2048 / 4)

/// <summary>Base address of IO CM4 MCU Core clock.</summary>
static const uintptr_t IO_CM4_RGU = 0x2101000C;
static const uintptr_t IO_CM4_DEBUGUART = 0x21040000;
static const uintptr_t IO_CM4_ISU0 = 0x38070500;
static const uintptr_t IO_CM4_GPT_BASE = 0x21030000;
static QueueHandle_t UARTDataQueue;
static TaskHandle_t NNTaskHandle;


static void ISU0_ISR(void);
static _Noreturn void DefaultExceptionHandler(void);
static _Noreturn void RTCoreMain(void);

extern uint32_t StackTop; // &StackTop == end of TCM0
extern void SVC_Handler(void);
extern void PendSV_Handler(void);
extern void SysTick_Handler(void);

// ARM DDI0403E.d SB1.5.2-3
// From SB1.5.3, "The Vector table must be naturally aligned to a power of two whose alignment
// value is greater than or equal to (Number of Exceptions supported x 4), with a minimum alignment
// of 128 bytes.". The array is aligned in linker.ld, using the dedicated section ".vector_table".

// The exception vector table contains a stack pointer, 15 exception handlers, and an entry for
// each interrupt.
#define INTERRUPT_COUNT 100 // from datasheet
#define EXCEPTION_COUNT (16 + INTERRUPT_COUNT)
#define INT_TO_EXC(i_) (16 + (i_))
const uintptr_t ExceptionVectorTable[EXCEPTION_COUNT] __attribute__((section(".vector_table")))
__attribute__((used)) = {
    [0] = (uintptr_t)&StackTop,					// Main Stack Pointer (MSP)
    [1] = (uintptr_t)RTCoreMain,				// Reset
    [2] = (uintptr_t)DefaultExceptionHandler,	// NMI
    [3] = (uintptr_t)DefaultExceptionHandler,	// HardFault
    [4] = (uintptr_t)DefaultExceptionHandler,	// MPU Fault
    [5] = (uintptr_t)DefaultExceptionHandler,	// Bus Fault
    [6] = (uintptr_t)DefaultExceptionHandler,	// Usage Fault
    [11] = (uintptr_t)SVC_Handler,				// SVCall
    [12] = (uintptr_t)DefaultExceptionHandler,	// Debug monitor
    [14] = (uintptr_t)PendSV_Handler,			// PendSV
    [15] = (uintptr_t)SysTick_Handler,			// SysTick
    [INT_TO_EXC(0)... INT_TO_EXC(46)] = (uintptr_t)DefaultExceptionHandler,
	[INT_TO_EXC(47)] = (uintptr_t)ISU0_ISR,
	[INT_TO_EXC(48)... INT_TO_EXC(INTERRUPT_COUNT - 1)] = (uintptr_t)DefaultExceptionHandler
};

static _Noreturn void DefaultExceptionHandler(void)
{
	_putchar('H'); _putchar('a'); _putchar('r'); _putchar('d');
	_putchar('F'); _putchar('a'); _putchar('u'); _putchar('l'); _putchar('t');
	_putchar('\n');

    for (;;) {
        // empty.
    }
}

void GPT3UsFreeRunTimerInit()
{
	// GPT3_INIT = initial counter value
	WriteReg32(IO_CM4_GPT_BASE, 0x54, 0x0);

	// GPT3_CTRL
	uint32_t ctrlOn = 0x0;
	ctrlOn |= (0x19) << 16; // OSC_CNT_1US (default value)
	ctrlOn |= 0x1;          // GPT3_EN = 1 -> GPT3 enabled
	WriteReg32(IO_CM4_GPT_BASE, 0x50, ctrlOn);
}

uint32_t GetCurrentUs()
{
	return ReadReg32(IO_CM4_GPT_BASE, 0x58);
}


static void ISU0_ISR(void)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	uint32_t iirId = ReadReg32(IO_CM4_ISU0, 0x08) & 0x1F;

	if (iirId == 0x04) {
		
		char data = ReadReg32(IO_CM4_ISU0, 0x00);
		xQueueSendFromISR(UARTDataQueue, &data, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

static void DebugUARTInit(void)
{
	// Configure UART to use 115200-8-N-1.
	WriteReg32(IO_CM4_DEBUGUART, 0x0C, 0x80); // LCR (enable DLL, DLM)
	WriteReg32(IO_CM4_DEBUGUART, 0x24, 0x3);  // HIGHSPEED
	WriteReg32(IO_CM4_DEBUGUART, 0x04, 0);    // Divisor Latch (MS)
	WriteReg32(IO_CM4_DEBUGUART, 0x00, 1);    // Divisor Latch (LS)
	WriteReg32(IO_CM4_DEBUGUART, 0x28, 224);  // SAMPLE_COUNT
	WriteReg32(IO_CM4_DEBUGUART, 0x2C, 110);  // SAMPLE_POINT
	WriteReg32(IO_CM4_DEBUGUART, 0x58, 0);    // FRACDIV_M
	WriteReg32(IO_CM4_DEBUGUART, 0x54, 223);  // FRACDIV_L
	WriteReg32(IO_CM4_DEBUGUART, 0x0C, 0x03); // LCR (8-bit word length)
}

static void ISU0Init(void)
{
	// Configure UART to use 115200-8-N-1.
	WriteReg32(IO_CM4_ISU0, 0x0C, 0x80); // LCR (enable DLL, DLM)
	WriteReg32(IO_CM4_ISU0, 0x24, 0x3);  // HIGHSPEED
	WriteReg32(IO_CM4_ISU0, 0x04, 0);    // Divisor Latch (MS)
	WriteReg32(IO_CM4_ISU0, 0x00, 1);    // Divisor Latch (LS)
	WriteReg32(IO_CM4_ISU0, 0x28, 224);  // SAMPLE_COUNT
	WriteReg32(IO_CM4_ISU0, 0x2C, 110);  // SAMPLE_POINT
	WriteReg32(IO_CM4_ISU0, 0x58, 0);    // FRACDIV_M
	WriteReg32(IO_CM4_ISU0, 0x54, 223);  // FRACDIV_L
	WriteReg32(IO_CM4_ISU0, 0x0C, 0x03); // LCR (8-bit word length)

	SetReg32(IO_CM4_ISU0, 0x04, 0x01);

	SetNvicPriority(47, 3);
	EnableNvicInterrupt(47);
}


static void UARTTask(void* pParameters)
{
	char c;

	while (1) {
		xQueueReceive(UARTDataQueue, &c, portMAX_DELAY);
		if ((c >= '0') && (c <= '9')) {
			xTaskNotify(NNTaskHandle, (c - '0'), eSetValueWithOverwrite);
		}
	}
}

// ASCII lib from (https://www.jianshu.com/p/1f58a0ebf5d9)
const char codeLib[] = "@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'.   ";
void print_img(int8_t* buf)
{
	for (int y = 0; y < 28; y++)
	{
		for (int x = 0; x < 28; x++)
		{
			int index = 69 / 127.0 * (127 - buf[y * 28 + x]);
			if (index > 69) index = 69;
			if (index < 0) index = 0;
			NNOM_LOG("%c", codeLib[index]);
			NNOM_LOG("%c", codeLib[index]);
		}
		NNOM_LOG("\n");
	}
}

static void NNTask(void* pParameters)
{
	nnom_model_t *model;
	uint32_t time;
	uint32_t predic_label;
	float prob;
	uint32_t index;

	model = nnom_model_create();

	while (1) {
		
		xTaskNotifyWait(0, UINT32_MAX, &index, portMAX_DELAY);

		NNOM_LOG("\nprediction start on img[%d]...\n", index);
		time = nnom_ms_get();

		// copy data and do prediction
		memcpy(nnom_input_data, (int8_t*)&img[index][0], 784);
		nnom_predict(model, &predic_label, &prob);
		time = nnom_ms_get() - time;

		//print original image to console
		print_img((int8_t*)& img[index][0]);

		NNOM_LOG("Time: %d tick\n", time);
		NNOM_LOG("Truth label: %d\n", label[index]);
		NNOM_LOG("Predicted label: %d\n", predic_label);
		NNOM_LOG("Probability: %d%%\n", (int)(prob * 100));

		model_stat(model);
	}
}


static void TaskInit(void* pParameters) 
{
	xTaskCreate(UARTTask, "UART Task", APP_STACK_SIZE_BYTES, NULL, 3, NULL);
	xTaskCreate(NNTask, "NN Task", APP_STACK_SIZE_BYTES, NULL, 2, &NNTaskHandle);
	vTaskSuspend(NULL);
}

static _Noreturn void RTCoreMain(void)
{
    // SCB->VTOR = ExceptionVectorTable
    WriteReg32(SCB_BASE, 0x08, (uint32_t)ExceptionVectorTable);

	DebugUARTInit();
	printf("\NNOM MINST-simple demo on Azure Sphere RTcore\n");
	printf("Input number 0 - 9 to feed img[0] - img[9] from test dataset to pre-generated model\n");

	GPT3UsFreeRunTimerInit();
	ISU0Init();
	UARTDataQueue = xQueueCreate(10, sizeof(uint8_t));

	// Boost M4 core to 197.6MHz (@26MHz), refer to chapter 3.3 in MT3620 Datasheet
	uint32_t val = ReadReg32(IO_CM4_RGU, 0);
	val &= 0xFFFF00FF;
	val |= 0x00000200;
	WriteReg32(IO_CM4_RGU, 0, val);

	xTaskCreate(TaskInit, "Init Task", APP_STACK_SIZE_BYTES, NULL, 7, NULL);
	vTaskStartScheduler();

	while (1);
}

// applicaiton hooks

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
	while (1);
}

void vApplicationMallocFailedHook(void)
{
	while (1);
}

void _putchar(char character)
{
	while (!(ReadReg32(IO_CM4_DEBUGUART, 0x14) & (UINT32_C(1) << 5)));
	WriteReg32(IO_CM4_DEBUGUART, 0x0, character);
}


