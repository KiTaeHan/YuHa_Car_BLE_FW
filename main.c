/* Board headers */
#include "init_mcu.h"
#include "init_board.h"
#include "init_app.h"
#include "ble-configuration.h"
#include "board_features.h"

/* Bluetooth stack headers */
#include "bg_types.h"
#include "native_gecko.h"
#include "gatt_db.h"

/* Libraries containing default Gecko configuration values */
#include "em_emu.h"
#include "em_cmu.h"
#include "em_timer.h"

/* Device initialization header */
#include "hal-config.h"

#if defined(HAL_CONFIG)
#include "bsphalconfig.h"
#else
#include "bspconfig.h"
#endif

/* Application header */
#include "app.h"
#include "utils/util.h"
#include "main.h"

/***********************************************************************************************//**
 * @addtogroup Application
 * @{
 **************************************************************************************************/
// Global variables used to set top value and duty cycle of the timer
#define PWM_FREQ          1000
#define DUTY_CYCLE_STEPS  0.3

static uint32_t topValue = 0;
//static volatile float FdutyCycle = 0;
//static volatile float RdutyCycle = 0;
/***********************************************************************************************//**
 * @addtogroup app
 * @{
 **************************************************************************************************/

#ifndef MAX_ADVERTISERS
#define MAX_ADVERTISERS 1
#endif

#ifndef MAX_CONNECTIONS
#define MAX_CONNECTIONS 4
#endif

uint8_t bluetooth_stack_heap[DEFAULT_BLUETOOTH_HEAP(MAX_CONNECTIONS)];

/* Bluetooth stack configuration parameters (see "UG136: Silicon Labs Bluetooth C Application Developer's Guide" for details on each parameter) */
static gecko_configuration_t config = {
  .config_flags = 0,                                   /* Check flag options from UG136 */
#if defined(FEATURE_LFXO) || defined(PLFRCO_PRESENT)
//  .sleep.flags = SLEEP_FLAGS_DEEP_SLEEP_ENABLE,        /* Sleep is enabled */
  .sleep.flags = 0,
#else
  .sleep.flags = 0,
#endif
  .bluetooth.max_connections = MAX_CONNECTIONS,        /* Maximum number of simultaneous connections */
  .bluetooth.max_advertisers = MAX_ADVERTISERS,        /* Maximum number of advertisement sets */
  .bluetooth.heap = bluetooth_stack_heap,              /* Bluetooth stack memory for connection management */
  .bluetooth.heap_size = sizeof(bluetooth_stack_heap), /* Bluetooth stack memory for connection management */
#if defined(FEATURE_LFXO)
  .bluetooth.sleep_clock_accuracy = 100,               /* Accuracy of the Low Frequency Crystal Oscillator in ppm. *
                                                       * Do not modify if you are using a module                  */
#elif defined(PLFRCO_PRESENT)
  .bluetooth.sleep_clock_accuracy = 500,               /* In case of internal RCO the sleep clock accuracy is 500 ppm */
#endif
  .gattdb = &bg_gattdb_data,                           /* Pointer to GATT database */
  .ota.flags = 0,                                      /* Check flag options from UG136 */
  .ota.device_name_len = 3,                            /* Length of the device name in OTA DFU mode */
  .ota.device_name_ptr = "OTA",                        /* Device name in OTA DFU mode */
  .pa.config_enable = 1,                               /* Set this to be a valid PA config */
#if defined(FEATURE_PA_INPUT_FROM_VBAT)
  .pa.input = GECKO_RADIO_PA_INPUT_VBAT,               /* Configure PA input to VBAT */
#else
  .pa.input = GECKO_RADIO_PA_INPUT_DCDC,               /* Configure PA input to DCDC */
#endif // defined(FEATURE_PA_INPUT_FROM_VBAT)
  .rf.flags = GECKO_RF_CONFIG_ANTENNA,                 /* Enable antenna configuration. */
  .rf.antenna = GECKO_RF_ANTENNA,                      /* Select antenna path! */
};

void TIMER0_IRQHandler(void)
{
  // Acknowledge the interrupt
  uint32_t flags = TIMER_IntGet(TIMER0);
  TIMER_IntClear(TIMER0, flags);

  // Update CCVB to alter duty cycle starting next period
//  TIMER_CompareBufSet(TIMER0, 0, (uint32_t)(topValue * FdutyCycle));
}

void Init_GPIO(void)
{
	// Enable GPIO clock
	CMU_ClockEnable(cmuClock_GPIO, true);

	GPIO_PinModeSet(DCMOTER_BB_PORT, DCMOTER_BB_PIN, gpioModePushPull, 0);
	GPIO_PinModeSet(DCMOTER_BA_PORT, DCMOTER_BA_PIN, gpioModePushPull, 0);
	GPIO_PinModeSet(DCMOTER_AA_PORT, DCMOTER_AA_PIN, gpioModePushPull, 0);
	GPIO_PinModeSet(DCMOTER_AB_PORT, DCMOTER_AB_PIN, gpioModePushPull, 0);
}

void Init_Timer(void)
{
  uint32_t timerFreq = 0;
  // Initialize the timer
  TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;
  // Configure TIMER0 Compare/Capture for output compare
  TIMER_InitCC_TypeDef timerCCInit = TIMER_INITCC_DEFAULT;

  // Use PWM mode, which sets output on overflow and clears on compare events
  timerInit.prescale = timerPrescale64;
  timerInit.enable = false;
  timerCCInit.mode = timerCCModePWM;

  // Enable clock to TIMER1
  CMU_ClockEnable(cmuClock_TIMER0, true);

  // Configure but do not start the timer
  TIMER_Init(TIMER0, &timerInit);

  // Route Timer0 CC0 output to PC6
  GPIO->TIMERROUTE[0].ROUTEEN  = GPIO_TIMER_ROUTEEN_CC0PEN;
  GPIO->TIMERROUTE[0].CC0ROUTE = (DCMOTER_AB_PORT << _GPIO_TIMER_CC0ROUTE_PORT_SHIFT)
  								  | (DCMOTER_AB_PIN << _GPIO_TIMER_CC0ROUTE_PIN_SHIFT);
  // Route Timer0 CC1 output to PB0
  GPIO->TIMERROUTE[0].ROUTEEN  |= GPIO_TIMER_ROUTEEN_CC1PEN;
  GPIO->TIMERROUTE[0].CC1ROUTE = (DCMOTER_AA_PORT << _GPIO_TIMER_CC1ROUTE_PORT_SHIFT)
  								  | (DCMOTER_AA_PIN << _GPIO_TIMER_CC1ROUTE_PIN_SHIFT);

  // Configure CC Channel 0
  TIMER_InitCC(TIMER0, 0, &timerCCInit);
  TIMER_InitCC(TIMER0, 1, &timerCCInit);


  // set PWM period
  timerFreq = CMU_ClockFreqGet(cmuClock_TIMER1) / (timerInit.prescale + 1);
  topValue = (timerFreq / PWM_FREQ);
  // Set top value to overflow at the desired PWM_FREQ frequency
  TIMER_TopSet(TIMER0, topValue);

  // Enable clock to TIMER1
  CMU_ClockEnable(cmuClock_TIMER1, true);

  // Configure but do not start the timer
  TIMER_Init(TIMER1, &timerInit);

  // Route Timer0 CC0 output to PC6
  GPIO->TIMERROUTE[1].ROUTEEN  = GPIO_TIMER_ROUTEEN_CC0PEN;
  GPIO->TIMERROUTE[1].CC0ROUTE = (DCMOTER_BB_PORT << _GPIO_TIMER_CC0ROUTE_PORT_SHIFT)
  								  | (DCMOTER_BB_PIN << _GPIO_TIMER_CC0ROUTE_PIN_SHIFT);
  // Route Timer0 CC1 output to PB0
  GPIO->TIMERROUTE[1].ROUTEEN  |= GPIO_TIMER_ROUTEEN_CC1PEN;
  GPIO->TIMERROUTE[1].CC1ROUTE = (DCMOTER_BA_PORT << _GPIO_TIMER_CC1ROUTE_PORT_SHIFT)
  								  | (DCMOTER_BA_PIN << _GPIO_TIMER_CC1ROUTE_PIN_SHIFT);

  // Configure CC Channel 0
  TIMER_InitCC(TIMER1, 0, &timerCCInit);
  TIMER_InitCC(TIMER1, 1, &timerCCInit);

  // set PWM period
  timerFreq = CMU_ClockFreqGet(cmuClock_TIMER1) / (timerInit.prescale + 1);
  topValue = (timerFreq / PWM_FREQ);
  // Set top value to overflow at the desired PWM_FREQ frequency
  TIMER_TopSet(TIMER1, topValue);

#if 0
  // Enable TIMER0 compare event interrupts to update the duty cycle
  TIMER_IntEnable(TIMER0, TIMER_IEN_CC0);
  NVIC_EnableIRQ(TIMER0_IRQn);
#endif
}


static float get_Duty(int x)
{
	return 0.1*x + 0.5;
}

/*
 * @note AA is port B1 & CC 1 channel
 * 		 AB is port B0 & CC 0 channel
 */
void Set_Left_Speed(int speed)
{
	float duty;

	if(0 == speed)		// Stop
	{
		duty = 0.0;
		TIMER_Enable(TIMER0, false);
		TIMER_CompareSet(TIMER0, 1, 0);
		TIMER_CompareSet(TIMER0, 0, 0);
	}
	else if(5>=speed)	// Reverse(1~5)
	{
		duty = get_Duty(6 - speed);
		TIMER_CompareSet(TIMER0, 1, 0);
		TIMER_CompareSet(TIMER0, 0, (uint32_t)(topValue * duty));
		TIMER_Enable(TIMER0, true);
	}
	else				// Forward (6~10)
	{
		duty = get_Duty(speed - 5);
		TIMER_CompareSet(TIMER0, 0, 0);
		TIMER_CompareSet(TIMER0, 1, (uint32_t)(topValue * duty));
		TIMER_Enable(TIMER0, true);
	}

	UTIL_delay(30);
}

/*
 * @note BA is port C7 & CC 1 channel
 * 		 BB is port C6 & CC 0 channel
 */
void Set_Right_Speed(int speed)
{
	float duty;

	if(0 == speed)		// Stop
	{
		duty = 0.0;
		TIMER_Enable(TIMER1, false);
		TIMER_CompareSet(TIMER1, 1, 0);
		TIMER_CompareSet(TIMER1, 0, 0);
	}
	else if(5>=speed)	// Reverse(1~5)
	{
		duty = get_Duty(6 - speed);
		TIMER_CompareSet(TIMER1, 1, 0);
		TIMER_CompareSet(TIMER1, 0, (uint32_t)(topValue * duty));
		TIMER_Enable(TIMER1, true);
	}
	else				// Forward (6~10)
	{
		duty = get_Duty(speed - 5);
		TIMER_CompareSet(TIMER1, 0, 0);
		TIMER_CompareSet(TIMER1, 1, (uint32_t)(topValue * duty));
		TIMER_Enable(TIMER1, true);
	}

	UTIL_delay(30);
}

void Test_Moter(void)
{
	Set_Left_Speed(10);
	Set_Right_Speed(10);
	UTIL_delay(500);
	Set_Left_Speed(0);
	Set_Right_Speed(0);
	UTIL_delay(500);
	Set_Left_Speed(1);
	Set_Right_Speed(1);
	UTIL_delay(500);
	Set_Left_Speed(0);
	Set_Right_Speed(0);
}

/**
 * @brief  Main function
 */
int main(void)
{
  /* Initialize device */
  initMcu();
  /* Initialize board */
  initBoard();
  /* Initialize application */
  initApp();
  initVcomEnable();

  UTIL_init();

  Init_GPIO();
  Init_Timer();

  Test_Moter();

  /* Start application */
  appMain(&config);
}
