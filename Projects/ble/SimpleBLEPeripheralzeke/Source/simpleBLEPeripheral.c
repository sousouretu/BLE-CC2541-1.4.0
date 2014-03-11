/**************************************************************************************************
 Filename:       simpleBLEPeripheral.c
 Revised:        $Date: 2010-08-06 08:56:11 -0700 (Fri, 06 Aug 2010) $
 Revision:       $Revision: 23333 $

 Description:    This file contains the Simple BLE Peripheral sample application
 for use with the CC2540 Bluetooth Low Energy Protocol Stack.

 Copyright 2010 - 2012 Texas Instruments Incorporated. All rights reserved.

 IMPORTANT: Your use of this Software is limited to those specific rights
 granted under the terms of a software license agreement between the user
 who downloaded the software, his/her employer (which must be your employer)
 and Texas Instruments Incorporated (the "License").  You may not use this
 Software unless you agree to abide by the terms of the License. The License
 limits your use, and you acknowledge, that the Software may not be modified,
 copied or distributed unless embedded on a Texas Instruments microcontroller
 or used solely and exclusively in conjunction with a Texas Instruments radio
 frequency transceiver, which is integrated into your product.  Other than for
 the foregoing purpose, you may not use, reproduce, copy, prepare derivative
 works of, modify, distribute, perform, display or sell this Software and/or
 its documentation for any purpose.

 YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
 PROVIDED �AS IS?WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
 NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
 TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
 NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
 LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
 INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
 OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
 OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
 (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

 Should you have any questions regarding your right to use this Software,
 contact Texas Instruments Incorporated at www.TI.com.
 **************************************************************************************************/

/*********************************************************************
 * INCLUDES
 */

#include "bcomdef.h"
#include "OSAL.h"
#include "OSAL_PwrMgr.h"
#include "osal_snv.h"

#include "OnBoard.h"
#include "hal_adc.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_lcd.h"
#include "SimpleBLESerialUart.h"

#include "gatt.h"

#include "hci.h"

#include "gapgattserver.h"
#include "gattservapp.h"
#include "devinfoservice.h"
#include "simpleGATTprofile.h"

#if defined ( PLUS_BROADCASTER )
#include "peripheralBroadcaster.h"
#else
#include "peripheral.h"
#endif

#include "gapbondmgr.h"

#include "simpleBLEPeripheral.h"

#if defined FEATURE_OAD
#include "oad.h"
#include "oad_target.h"
#endif

#include "hal_i2c.h"
#include "hal_eeprom.h"
#include "hal_at24c32.h"
#include "hal_ds1307.h"
#include "SimpleEepromUtils.h"
#include "SimpleRTCInterrupt.h"
#include "SimpleBLESPIFlash.h"
#include "battservice.h"
#include "SimpleTemperature.h"
#include "SimpleDS18B20.h"
#include "Simplepasscode.h"
/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

// How often to perform periodic event
//#define SBP_PERIODIC_EVT_PERIOD                   5000

#define TEMP_CHECK_PERIOD                         5000

// How often to check battery voltage (in ms)
#define BATTERY_CHECK_PERIOD                     20000////////////////////////////////////batt

// What is the advertising interval when device is discoverable (units of 625us, 160=100ms)
#define DEFAULT_ADVERTISING_INTERVAL          160
/*//范围20MS-10.24S之间 除去广播延时时间0-10ms，为其可设置的范围。
//若广播事件是可扫描无向事件或非连接无向事件 其值不得小于100MS，若广播事件是可连接无向事件，其值可以为20MS或者更大   
//与手机通信过程中用的是,默认设置  GAP_ADTYPE_ADV_IND即可连接无向事件  测功耗时可以适当调整其值 */
// Whether to enable automatic parameter update request when a connection is formed
#define DEFAULT_ENABLE_UPDATE_REQUEST         FALSE//TRUE

// Limited discoverable mode advertises for 30.72s, and then stops
// General discoverable mode advertises indefinitely

#if defined ( CC2540_MINIDK )
#define DEFAULT_DISCOVERABLE_MODE             GAP_ADTYPE_FLAGS_LIMITED
#else
#define DEFAULT_DISCOVERABLE_MODE             GAP_ADTYPE_FLAGS_GENERAL
#endif  // defined ( CC2540_MINIDK )
// Minimum connection interval (units of 1.25ms, 80=100ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL     80

// Maximum connection interval (units of 1.25ms, 800=1000ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL     800

// Slave latency to use if automatic parameter update request is enabled
#define DEFAULT_DESIRED_SLAVE_LATENCY         0//10  参数更新打开 与手机通信时 要小于4

// Supervision timeout value (units of 10ms, 1000=10s) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_CONN_TIMEOUT          1000//与手机通信时 要大于等于6S

// Company Identifier: Texas Instruments Inc. (13)
#define TI_COMPANY_ID                         0x000D

#define INVALID_CONNHANDLE                    0xFFFF

// Length of bd addr as a string
#define B_ADDR_STR_LEN                        15

#if defined ( PLUS_BROADCASTER )
#define ADV_IN_CONN_WAIT                    500 // delay 500 ms
#endif

// GAP connection handle
static uint16 gapConnHandle;

extern uint8 SBP_UART_STUDY_CMD;
extern uint8 SBP_UART_STUDY_CMD_LEN;

/*************************************************************
 *  recv data define
 */
//XDATA uint8 a[3000]={0};
extern UartState u_state;
#define TRANSFER_DATA_SIGN 0xFE
#define TRANSFER_DATA_SIGN_RE 0xFF
#define UART_DATA_START_INDEX 2
#define one_time_data_len 125
// uint8 code recv_value1[2540] = { 0 };
static uint8  recv_value[254] = { 0 };
static uint8 TRANSFER_DATA_STATE_IN = FALSE;
static char newValueBuf[20] = { 0 };
static uint8 data_len = 0, cur_data_len = 0, data_len_index = 0, send_times = 0;

/*********************************************************************
 * SPI FLASH
 */
uint8 buf[20];
uint8 bufrx[20];

/*********************************************************************
 * temperature
 */
uint8 temperature[20];
uint8 temp_18b20[10];
uint8 temp_flag=0;
uint8 temp_18b20_flag=0;
/*************************************************************
 *  LIST
 */

 typedef struct
{           
  
  uint8 listlen;                   
  uint8 value1[100]; 
  uint8 value2[100];                   
  uint8 value3[100]; 
   //uint8 value4[100]; 
    //uint8 value5[100]; 
    // uint8 value6[100]; 
  uint8 value1_len; 
  uint8 value2_len;                   
  uint8 value3_len;
}list;
//list current_list={1,{0},{0},0,0}; 
 //list current_list={1,{0},{0},{0},{0},{0},0,0,0};
list current_list={1,{0},{0},{0},0,0,0};



/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */
static uint8 simpleBLEPeripheral_TaskID; // Task ID for internal task/event processing

static gaprole_States_t gapProfileState = GAPROLE_INIT;

// GAP - SCAN RSP data (max size = 31 bytes)
static uint8 scanRspData[] = {
// complete name
		0x14,// length of this data
		GAP_ADTYPE_LOCAL_NAME_COMPLETE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

		// connection interval range
		0x05,// length of this data
		GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE, LO_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL), // 100ms
		HI_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL), LO_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL), // 1s
		HI_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL),

		// Tx power level
		0x02,// length of this data
		GAP_ADTYPE_POWER_LEVEL, 0 // 0dBm
		};

// GAP - Advertisement data (max size = 31 bytes, though this is
// best kept short to conserve power while advertisting)
static uint8 advertData[] = {
// Flags; this sets the device to use limited discoverable
// mode (advertises for 30 seconds at a time) instead of general
// discoverable mode (advertises indefinitely)
		0x02,// length of this data
		GAP_ADTYPE_FLAGS, DEFAULT_DISCOVERABLE_MODE | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

		// service UUID, to notify central devices what services are included
		// in this peripheral
		0x03,// length of this data
		GAP_ADTYPE_16BIT_MORE, // some of the UUID's, but not all
		LO_UINT16(SIMPLEPROFILE_SERV_UUID), HI_UINT16(SIMPLEPROFILE_SERV_UUID),

};

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void simpleBLEPeripheral_ProcessOSALMsg(osal_event_hdr_t *pMsg);
static void peripheralStateNotificationCB(gaprole_States_t newState);
static void performPeriodicTask(void);
static void simpleProfileChangeCB(uint8 paramID);
static void simpleBLEPeripheral_HandleKeys(uint8 shift, uint8 keys);
//static void simpleBLEPeripheralPasscodeCB(uint8 *deviceAddr, uint16 connectionHandle, uint8 uiInputs, uint8 uiOutputs);
static void simpleBLEPeripheralPairStateCB(uint16 connHandle, uint8 state, uint8 status);
static char *bdAddr2Str(uint8 *pAddr);
static void gettemp(void);
//static void updateDeviceName(char *name, uint8 len);
//static uint32 atoi(uint8 s[]);
static void Uartsend_irdata(void);
static void Receive_Save_Uartsend_irdata(void);

/*********************************************************************
 * PROFILE CALLBACKS
 */

// GAP Role Callbacks
static gapRolesCBs_t simpleBLEPeripheral_PeripheralCBs = { peripheralStateNotificationCB, // Profile State Change Callbacks
		NULL // When a valid RSSI is read from controller (not used by application)
		};

// GAP Bond Manager Callbacks
static gapBondCBs_t simpleBLEPeripheral_BondMgrCBs = { NULL, // Passcode callback (not used by application)
		simpleBLEPeripheralPairStateCB // Pairing / Bonding state Callback (not used by application)
		};

// Simple GATT Profile Callbacks
static simpleProfileCBs_t simpleBLEPeripheral_SimpleProfileCBs = { simpleProfileChangeCB // Charactersitic value change callback
		};

/*********************************************************************
 * @fn      pairStateCB
 * @brief   Pairing state callback.
 * @return  none
 */
static void simpleBLEPeripheralPairStateCB(uint16 connHandle, uint8 state, uint8 status) {
	if (state == GAPBOND_PAIRING_STATE_STARTED) {
		HalLcdWriteString("Pairing started", HAL_LCD_LINE_7);
	} else if (state == GAPBOND_PAIRING_STATE_COMPLETE) {
		if (status == SUCCESS) {
			HalLcdWriteString("Pairing success", HAL_LCD_LINE_7);
		} else {
			HalLcdWriteStringValue("Pairing fail", status, 10, HAL_LCD_LINE_7);
			uint8 a = GAPRole_TerminateConnection();
			HalLcdWriteStringValue("Pairing fail--a", a, 10, HAL_LCD_LINE_7);
		}
	} else if (state == GAPBOND_PAIRING_STATE_BONDED) {
		if (status == SUCCESS) {
			HalLcdWriteString("Bonding success", HAL_LCD_LINE_1);
		}
	}

	//osal_start_timerEx(simpleBLEPeripheral_TaskID, SBP_ZEKEZANG_EVT, 5000);
}

/*********************************************************************
 * @fn      simpleBLECentralPasscodeCB
 * @brief   Passcode callback.
 * @return  none

 static void simpleBLEPeripheralPasscodeCB(uint8 *deviceAddr, uint16 connectionHandle, uint8 uiInputs, uint8 uiOutputs) {
 HalLcdWriteStringValue("uiInputs:", uiInputs, 10, HAL_LCD_LINE_5);
 HalLcdWriteStringValue("uiOutputs", uiOutputs, 10, HAL_LCD_LINE_6);
 }
 */

/*********************************************************************
 * @fn      readWriteFlash
 * @brief   readWriteFlash
 * @return  none

 static uint32 passs = 0;
 static void readWriteFlash() {
 uint8 * aa;
 aa = osal_msg_allocate(15);
 osal_memset(aa, 0, 15);
 osal_memcpy(aa, "as", 2);
 uint16 p = 1234;
 if (osal_snv_write(0xE0, sizeof(uint16), &p) == SUCCESS) {
 HalLcdWriteString("write ok", HAL_LCD_LINE_2);
 }
 osal_msg_deallocate(aa);
 uint8 bb[15] = { 0x0 };
 uint16 bb = 0;
 if (osal_snv_read(0xE0, 15, &bb) == SUCCESS) {
 HalLcdWriteString("read ok", HAL_LCD_LINE_2);
 passs = bb;
 }
 }
 */

void gettemp(void)
{
     // osal_snv_read(0xE0,6,&temperature);
       // uint8 TempValue[6];  
        uint8 AvgTemp=0,i=0,j;
        uint8 sensor_data_value;  //传感器数据

        initTempSensor();
        AvgTemp = getTemperature();  
        
        temperature[temp_flag]=AvgTemp;
        temp_flag++;
       
        DS18B20_SendConvert();
        //延时1S
        for(j=20; j>0; j--)
          delay_nus(50000);
        sensor_data_value=DS18B20_GetTem();
        temp_18b20[temp_18b20_flag]=sensor_data_value;
        temp_18b20_flag++;

        if(temp_flag==10)
        {

          for(i=0;i<temp_flag;i++)
             HalLcdWriteStringValueValue("Temp,18b20:", temperature[i], 10, temp_18b20[i],10,i+1);
        }
        
        
}
/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      SimpleBLEPeripheral_Init
 *
 * @brief   Initialization function for the Simple BLE Peripheral App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notificaiton ... ).
 *
 * @param   task_id - the ID assigned by OSAL.  This ID should be
 *                    used to send messages and set timers.
 *
 * @return  none
 */
void SimpleBLEPeripheral_Init(uint8 task_id) {
	simpleBLEPeripheral_TaskID = task_id;
	SbpHalUART_Init(task_id);
        
	// Setup the GAP Peripheral Role Profile
	{

#if defined( CC2540_MINIDK )
		// For the CC2540DK-MINI keyfob, device doesn't start advertising until button is pressed
		uint8 initial_advertising_enable = FALSE;
#else
		// For other hardware platforms, device starts advertising upon initialization
		uint8 initial_advertising_enable = TRUE;
#endif

		// By setting this to zero, the device will go into the waiting state after
		// being discoverable for 30.72 second, and will not being advertising again
		// until the enabler is set back to TRUE
		uint16 gapRole_AdvertOffTime = 0;

		uint8 enable_update_request = DEFAULT_ENABLE_UPDATE_REQUEST;
		uint16 desired_min_interval = DEFAULT_DESIRED_MIN_CONN_INTERVAL;
		uint16 desired_max_interval = DEFAULT_DESIRED_MAX_CONN_INTERVAL;
		uint16 desired_slave_latency = DEFAULT_DESIRED_SLAVE_LATENCY;
		uint16 desired_conn_timeout = DEFAULT_DESIRED_CONN_TIMEOUT;

		// Set the GAP Role Parameters
		GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8), &initial_advertising_enable);
		GAPRole_SetParameter(GAPROLE_ADVERT_OFF_TIME, sizeof(uint16), &gapRole_AdvertOffTime);

		GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scanRspData), scanRspData);
		GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);

		GAPRole_SetParameter(GAPROLE_PARAM_UPDATE_ENABLE, sizeof(uint8), &enable_update_request);
		GAPRole_SetParameter(GAPROLE_MIN_CONN_INTERVAL, sizeof(uint16), &desired_min_interval);
		GAPRole_SetParameter(GAPROLE_MAX_CONN_INTERVAL, sizeof(uint16), &desired_max_interval);
		GAPRole_SetParameter(GAPROLE_SLAVE_LATENCY, sizeof(uint16), &desired_slave_latency);
		GAPRole_SetParameter(GAPROLE_TIMEOUT_MULTIPLIER, sizeof(uint16), &desired_conn_timeout);
	}

	//readWriteFlash();


	// Set advertising interval
	{
		uint16 advInt = DEFAULT_ADVERTISING_INTERVAL;

		GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MIN, advInt);
		GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MAX, advInt);
		GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MIN, advInt);
		GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MAX, advInt);
	}
        set_passkey();


	// Initialize GATT attributes
	GGS_AddService(GATT_ALL_SERVICES); // GAP
	GATTServApp_AddService(GATT_ALL_SERVICES); // GATT attributes
	DevInfo_AddService(); // Device Information Service
	SimpleProfile_AddService(GATT_ALL_SERVICES); // Simple GATT Profile
        Batt_AddService();     // Battery Service
        
        
        
        
#if defined FEATURE_OAD
			VOID OADTarget_AddService(); // OAD Profile
#endif

	// Setup the SimpleProfile Characteristic Values
	{
		uint8 charValue1 = 1;
		uint8 charValue2 = 2;
		uint8 charValue3 = 3;
		uint8 charValue4 = 4;
		uint8 charValue5[SIMPLEPROFILE_CHAR5_LEN] = { 1, 2, 3, 4, 5 };
		SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR1, sizeof(uint8), &charValue1);
		SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR2, sizeof(uint8), &charValue2);
		SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR3, sizeof(uint8), &charValue3);
		SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4, sizeof(uint8), &charValue4);
		SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR5, SIMPLEPROFILE_CHAR5_LEN, charValue5);
	}

	HalLcdWriteString("BLE slave aico", HAL_LCD_LINE_1);

	// Register callback with SimpleGATTprofile
	VOID SimpleProfile_RegisterAppCBs(&simpleBLEPeripheral_SimpleProfileCBs);

	//who open who byebye
	//HCI_EXT_ClkDivOnHaltCmd(HCI_EXT_ENABLE_CLK_DIVIDE_ON_HALT); no no no...

	// Register for all key events - This app will handle all key events
	RegisterForKeys(simpleBLEPeripheral_TaskID);

#if defined ( DC_DC_P0_7 )
	// Enable stack to toggle bypass control on TPS62730 (DC/DC converter)
	HCI_EXT_MapPmIoPortCmd( HCI_EXT_PM_IO_PORT_P0, HCI_EXT_PM_IO_PORT_PIN7 );
#endif // defined ( DC_DC_P0_7 )
	// Setup a delayed profile startup
	osal_set_event(simpleBLEPeripheral_TaskID, SBP_START_DEVICE_EVT);
        
        
         // initialize the ADC for battery reads
        HalAdcInit();

	/***********************************test something zekezang**********************************/
	//HalLcdWriteString(" start", HAL_LCD_LINE_1);
	
            XNV_SPI_INIT();
      


	/***********************************test something zekezang**********************************/
}

/*********************************************************************
 * @fn      SimpleBLEPeripheral_ProcessEvent
 *
 * @brief   Simple BLE Peripheral Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  events not processed
 */
uint16 SimpleBLEPeripheral_ProcessEvent(uint8 task_id, uint16 events) {

	VOID task_id; // OSAL required parameter that isn't used in this function
        uint8 i;
	if (events & SYS_EVENT_MSG) {
		uint8 *pMsg;

		if ((pMsg = osal_msg_receive(simpleBLEPeripheral_TaskID)) != NULL) {
			simpleBLEPeripheral_ProcessOSALMsg((osal_event_hdr_t *) pMsg);

			// Release the OSAL message
			VOID osal_msg_deallocate(pMsg);
		}

		// return unprocessed events
		return (events ^ SYS_EVENT_MSG);
	}

	if (events & SBP_START_DEVICE_EVT) {
		// Start the Device
		GAPRole_StartDevice(&simpleBLEPeripheral_PeripheralCBs);

		// Start Bond Manager
		GAPBondMgr_Register(&simpleBLEPeripheral_BondMgrCBs);

		// Set timer for first periodic event
		//osal_start_timerEx(simpleBLEPeripheral_TaskID, SBP_PERIODIC_EVT, SBP_PERIODIC_EVT_PERIOD);
                
                // Set timer for first battery read event
                osal_start_timerEx(simpleBLEPeripheral_TaskID, SBP_PERIODIC_EVT, BATTERY_CHECK_PERIOD );
                

                osal_start_timerEx(simpleBLEPeripheral_TaskID, SBP_TEMP_EVT, TEMP_CHECK_PERIOD );
          
		return (events ^ SBP_START_DEVICE_EVT);
	}

	if (events & SBP_PERIODIC_EVT) {
		
                //Restart timer
                if ( BATTERY_CHECK_PERIOD )
               {
                 osal_start_timerEx(simpleBLEPeripheral_TaskID, SBP_PERIODIC_EVT, BATTERY_CHECK_PERIOD );
                }
                 
               HalLedSet(HAL_LED_1, HAL_LED_MODE_ON ); 
                 //延时1S
               for(i=20; i>0; i--)
                  delay_nus(50000);
               HalLedSet(HAL_LED_1, HAL_LED_MODE_OFF );
               // perform battery level check
               Batt_MeasLevel();
               
		return (events ^ SBP_PERIODIC_EVT);
	}

//	if (events & SBP_ZEKEZANG_EVT) {
//		uint8 initial_advertising_enable = FALSE;
//		GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8), &initial_advertising_enable);
//		return (events ^ SBP_ZEKEZANG_EVT);
//	}

//	if (events & SBP_SEND_IRDATA_EVT) {
//		HalLcdWriteString("send plan compl", HAL_LCD_LINE_4);
//		return (events ^ SBP_SEND_IRDATA_EVT);
//	}
//
//	if (events & SBP_ADV_IN_CONNECTION_EVT) {
//		uint8 turnOnAdv = TRUE;
//		// Turn on advertising while in a connection
//		GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8), &turnOnAdv);
//
//		return (events ^ SBP_ADV_IN_CONNECTION_EVT);
//	}
        
        if (events & SBP_TEMP_EVT) {
              // HalLcdWriteString("start get temp", HAL_LCD_LINE_5);
              
              if ( SBP_TEMP_EVT)
               {
                  osal_start_timerEx(simpleBLEPeripheral_TaskID, SBP_TEMP_EVT, TEMP_CHECK_PERIOD ); 
               }
              HalLedSet(HAL_LED_2, HAL_LED_MODE_ON ); 
                 //延时1S
               for(i=20; i>0; i--)
                  delay_nus(50000);
               HalLedSet(HAL_LED_2, HAL_LED_MODE_OFF );
	       gettemp();
              
                
	       return (events ^ SBP_TEMP_EVT);
	}
        

	return 0;
}

/*********************************************************************
 * @fn      simpleBLEPeripheral_ProcessOSALMsg
 *
 * @brief   Process an incoming task message.
 *
 * @param   pMsg - message to process
 *
 * @return  none
 */
static void simpleBLEPeripheral_ProcessOSALMsg(osal_event_hdr_t *pMsg) {
	switch (pMsg->event) {
	case KEY_CHANGE:
		simpleBLEPeripheral_HandleKeys(((keyChange_t *) pMsg)->state, ((keyChange_t *) pMsg)->keys);
		break;
	default:
		// do nothing
		break;
	}
}

static void simpleBLEPeripheral_HandleKeys(uint8 shift, uint8 keys) {
	if (keys & HAL_KEY_UP) {
		//u_state = IR_DATA_STUDY_CMD_START_BEGIN_STATE;
		//SbpHalUARTWrite(&SBP_UART_STUDY_CMD, SBP_UART_STUDY_CMD_LEN);
	}

	if (keys & HAL_KEY_LEFT) {
		HalLcdWriteString("change to de_paddkey_name", HAL_LCD_LINE_3);
                set_de_passkey();
	}

	if (keys & HAL_KEY_DOWN) {
		HalLcdWriteString("send after 3s...", HAL_LCD_LINE_4);
		//osal_start_timerEx(simpleBLEPeripheral_TaskID, SBP_SEND_IRDATA_EVT, 3000);
	}

	if (keys & HAL_KEY_RIGHT) {
		//HalLcdWriteStringValue("data_len:", data_len, 10, HAL_LCD_LINE_2);
	}

}

/*********************************************************************
 * @fn      peripheralStateNotificationCB
 *
 * @brief   Notification from the profile of a state change.
 *
 * @param   newState - new state
 *
 * @return  none
 */
static void peripheralStateNotificationCB(gaprole_States_t newState) {

	//HalLcdWriteStringValue("newState", newState, 10, HAL_LCD_LINE_4);

	switch (newState) {
	case GAPROLE_STARTED: {
		uint8 ownAddress[B_ADDR_LEN];
		uint8 systemId[DEVINFO_SYSTEM_ID_LEN];

		GAPRole_GetParameter(GAPROLE_BD_ADDR, ownAddress);

		// use 6 bytes of device address for 8 bytes of system ID value
		systemId[0] = ownAddress[0];
		systemId[1] = ownAddress[1];
		systemId[2] = ownAddress[2];

		// set middle bytes to zero
		systemId[4] = 0x00;
		systemId[3] = 0x00;

		// shift three bytes up
		systemId[7] = ownAddress[5];
		systemId[6] = ownAddress[4];
		systemId[5] = ownAddress[3];

		DevInfo_SetParameter(DEVINFO_SYSTEM_ID, DEVINFO_SYSTEM_ID_LEN, systemId);

		// Display device address
		bdAddr2Str(ownAddress);
		HalLcdWriteString(bdAddr2Str(ownAddress), HAL_LCD_LINE_3);
		HalLcdWriteString("Initialized", HAL_LCD_LINE_3);
	}
		break;

	case GAPROLE_ADVERTISING: {
		HalLcdWriteString("Advertising", HAL_LCD_LINE_3);
	}
		break;

	case GAPROLE_CONNECTED: {
		HalLcdWriteString("Connected", HAL_LCD_LINE_3);
		//simpleProfile_StateNotify( uint16 connHandle, attHandleValueNoti_t *pNoti )
		GAPRole_GetParameter(GAPROLE_CONNHANDLE, &gapConnHandle);
	}
		break;

	case GAPROLE_WAITING: {
		HalLcdWriteString("Disconnected", HAL_LCD_LINE_3);
	}
		break;

	case GAPROLE_WAITING_AFTER_TIMEOUT: {
		HalLcdWriteString("Timed Out", HAL_LCD_LINE_3);
	}
		break;

	case GAPROLE_ERROR: {
		HalLcdWriteString("Error", HAL_LCD_LINE_3);
	}
		break;

	default: {
		HalLcdWriteString("", HAL_LCD_LINE_3);
	}
		break;

	}

	gapProfileState = newState;

#if !defined( CC2540_MINIDK )
	VOID gapProfileState; // added to prevent compiler warning with
						  // "CC2540 Slave" configurations
#endif

}

/*********************************************************************
 * @fn      performPeriodicTask
 *
 * @brief   Perform a periodic application task. This function gets
 *          called every five seconds as a result of the SBP_PERIODIC_EVT
 *          OSAL event. In this example, the value of the third
 *          characteristic in the SimpleGATTProfile service is retrieved
 *          from the profile, and then copied into the value of the
 *          the fourth characteristic.
 *
 * @param   none
 *
 * @return  none
 */
static void performPeriodicTask(void) {
	uint8 valueToCopy;
	uint8 stat;

	// Call to retrieve the value of the third characteristic in the profile
	stat = SimpleProfile_GetParameter(SIMPLEPROFILE_CHAR3, &valueToCopy);

	if (stat == SUCCESS) {
		/*
		 * Call to set that value of the fourth characteristic in the profile. Note
		 * that if notifications of the fourth characteristic have been enabled by
		 * a GATT client device, then a notification will be sent every time this
		 * function is called.
		 */
		SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4, sizeof(uint8), &valueToCopy);
	}
}

/*********************************************************************
 * @fn      simpleProfileChangeCB
 * @brief   Callback from SimpleBLEProfile indicating a value change
 * @param   paramID - parameter ID of the value that was changed.
 *
 * @return  none
 */
static void simpleProfileChangeCB(uint8 paramID) {
	osal_memset(buf, 0, 20);
         uint8 valuechar3[20]={0};
	switch (paramID) {
	case SIMPLEPROFILE_CHAR1:
		/* SimpleProfile_GetParameter(SIMPLEPROFILE_CHAR1, newValueBuf);
            //uint8 *new1=&newValueBuf[0];
            
            if ((newValueBuf[0] == TRANSFER_DATA_SIGN) && (newValueBuf[1] == TRANSFER_DATA_SIGN_RE)) 
	     {
			TRANSFER_DATA_STATE_IN = FALSE;
                       
		}
            
            
	   if ((newValueBuf[4] != 0) && (!TRANSFER_DATA_STATE_IN)) 
	    {
			timer_flag=newValueBuf[2];
                        if(timer_flag==2)
                         { 
                           time=newValueBuf[3]*1000;
                         }
                        else time=0;
                        data_len = newValueBuf[UART_DATA_START_INDEX];
			TRANSFER_DATA_STATE_IN = TRUE;
			data_len_index = 0;
			osal_memset(recv_value, 0, data_len);
                     //   HalLcdWriteString("ok", HAL_LCD_LINE_5);
		}
	
	  cur_data_len = osal_strlen(newValueBuf);//有问题 是0的话 长度不对
         //  cur_data_len = sizeof(newValueBuf);
           HalLcdWriteStringValue("cur_data_len:",cur_data_len, 10, HAL_LCD_LINE_5); 
           
	   if (TRANSFER_DATA_STATE_IN) 
	    {
		osal_memcpy((recv_value + data_len_index), newValueBuf, cur_data_len);
                
		data_len_index += cur_data_len;
                
	     }
           //HalLcdWriteStringValue("data_len:", data_len, 10, HAL_LCD_LINE_6);
           //HalLcdWriteStringValue("recv_value_len:", osal_strlen((char *)recv_value), 10, HAL_LCD_LINE_7); 
           //HalLcdWriteStringValue("data_len_index:", data_len_index, 10, HAL_LCD_LINE_8);
           
           
           
           if (data_len_index == data_len) 
	   {
            
                 if(timer_flag==1)
		    Uartsend_irdata();
		
		 else if(timer_flag==2) 
		
		    Receive_Save_Uartsend_irdata();
			
			
		
		TRANSFER_DATA_STATE_IN = FALSE;
		HalLcdWriteStringValue("data_len:", osal_strlen((char *)recv_value), 10, HAL_LCD_LINE_6); 
                HalLcdWriteStringValue("listlen:", current_list.listlen, 10, HAL_LCD_LINE_8);
		data_len = 0;
                send_times = 0;
		cur_data_len = 0;
		data_len_index = 0;
		osal_memset(recv_value, 0, data_len);
           }*/
          
          /*SimpleProfile_GetParameter(SIMPLEPROFILE_CHAR1,buf);
                 HalSPIWrite(0x0,buf,20);
                 HalLcdWriteString((uint8*)buf, HAL_LCD_LINE_6);
		  //HalLcd_HW_WriteLine(HAL_LCD_LINE_4, "Write:"); 
		 // HalLcd_HW_WriteLine(HAL_LCD_LINE_5, buf); 
		  //注意，连续读写之间至少要延时800us
		  //HalHW_WaitUs(800);
	          HalSPIRead(0x0,bufrx,20);
                   HalLcdWriteString((uint8*)bufrx, HAL_LCD_LINE_7);
		  //HalLcd_HW_WriteLine(HAL_LCD_LINE_6, "Read:"); 
		 // HalLcd_HW_WriteLine(HAL_LCD_LINE_7, bufrx); 
*/
		

		break;
	case SIMPLEPROFILE_CHAR3:
		SimpleProfile_GetParameter(SIMPLEPROFILE_CHAR3, valuechar3);
                set_code_name(valuechar3);
 
		break;
	default:
		// should not reach here!
		break;
	}
}
/*********************************************************************
 * @fn      Receive_Save_Uartsend_irdata(void)
 * @brief  if
 * @param  
 * @return  none
 */
  static void Receive_Save_Uartsend_irdata(void)
 {
               
		/*if(current_list.listlen==1)
		{	osal_memset(current_list.value1, 0, data_len);
			osal_memcpy(current_list.value1, recv_value, data_len);
			current_list.value1_len=data_len;
			current_list.listlen++;
			osal_start_timerEx(simpleBLEPeripheral_TaskID, SBP_SEND_IRDATA_EVT1, time);
			
			 
			
		}
		else if(current_list.listlen==2)
		{	osal_memset(current_list.value2, 0, data_len);
			osal_memcpy(current_list.value2, recv_value, data_len);
			current_list.value2_len=data_len;
			current_list.listlen++;
			osal_start_timerEx(simpleBLEPeripheral_TaskID, SBP_SEND_IRDATA_EVT2, time);
			
		}
		else if(current_list.listlen==3)
		{
			osal_memset(current_list.value3, 0, data_len);
			osal_memcpy(current_list.value3, recv_value, data_len);
			current_list.value3_len=data_len;
			osal_start_timerEx(simpleBLEPeripheral_TaskID, SBP_SEND_IRDATA_EVT3, time);
			current_list.listlen++;
		} 
		SimpleProfile_SetParameter( SIMPLEPROFILE_CHAR2, sizeof(uint8), &current_list.listlen);
	*/

} 
/*********************************************************************
 * @fn      Uartsend_irdata
 * @brief  
 * @param  
 * @return  none
 */
 static void Uartsend_irdata()
 {
	        //HalLcdWriteString("ok", HAL_LCD_LINE_5); 
		recv_value[UART_DATA_START_INDEX] = 0xE3;
                SbpHalUARTWrite(recv_value + UART_DATA_START_INDEX, data_len-4);
 }
 

/*********************************************************************
 * @fn      bdAddr2Str
 *
 * @brief   Convert Bluetooth address to string. Only needed when
 *          LCD display is used.
 *
 * @return  none
 */
char *bdAddr2Str(uint8 *pAddr) {
	uint8 i;
	char hex[] = "0123456789ABCDEF";
	static char str[B_ADDR_STR_LEN];
	char *pStr = str;

	*pStr++ = '0';
	*pStr++ = 'x';

	// Start from end of addr
	pAddr += B_ADDR_LEN;

	for (i = B_ADDR_LEN; i > 0; i--) {
		*pStr++ = hex[*--pAddr >> 4];
		*pStr++ = hex[*pAddr & 0x0F];
	}

	*pStr = 0;

	return str;
}
/*********************************************************************
 *********************************************************************/
//static int ascii2hex(char c) {
//	int ret = -1;
//	if ((c >= '0') && (c <= '9')) {
//		ret = c - '0';
//	} else if ((c >= 'A') && (c <= 'Z')) {
//		ret = c - 'A' + 65;
//	} else if ((c >= 'a') && (c <= 'z')) {
//		ret = c - 'a' + 97;
//	}
//	return ret;
//}
//static void updateDeviceName(char *name, uint8 len) {
//	uint8 k = 0;
//	for (k = 0; k < len; k++) {
//		scanRspData[k + 2] = ascii2hex(*(name + k));
//	}
//}


