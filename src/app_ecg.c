/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app_ecg.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It
    implements the logic of the application's state machine and it may call
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************
#include <string.h>
#include "main.h"
#include "app_ecg.h"
#include "app_oled.h"
#include "GraphicLib.h"
#include "firmware/application/sml_recognition_run.h"

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_ECG_Initialize function.

    Application strings and buffers are be defined outside this structure.
*/
#define DV_ENABLE    1
#define DEBUG_ENABLE 0

APP_ECG_DATA app_ecgData;
uint16_t UartReadSize;
uint8_t  UartReadBuffer[1024];
uint8_t  UartRingBuffer[2048];
uint16_t UartRingLast = 0;
uint16_t UartRingTailSize;

#define BMD101_SYNC_BYTE           0xAA // Sync bytes 0xAA 0xAA
#define BMD101_EXTENDED_CODE_LEVEL 0x55 // Extended Code Level
#define BMD101_CODE_SIGNAL_QUALITY 0x02 // Signal Quality (0—sensor off, 200—sensor on), 1 byte w/o LENGTH byte
#define BMD101_CODE_HEART_RATE     0x03 // Real-time Heart Rate (Beats Per Minute), 1 byte w/o LENGTH byte
#define BMD101_CODE_DONT_CARE1     0x08 // Don’t Care, 1 byte w/o LENGTH byte
#define BMD101_CODE_ECG_RAW        0x80 // 16-bit Raw Data (2’s Complement), 2 bytes w/ LENGTH byte
#define BMD101_CODE_DONT_CARE2     0x84 // Don’t Care, 5 bytes w/ LENGTH byte
#define BMD101_CODE_DONT_CARE3     0x85 // Don’t Care, 3 bytes w/ LENGTH byte
typedef enum
{
    /* Application's state machine's initial state. */
    SENSOR_OFF=0,
    SENSOR_ON=200
} BMD101_SIGNAL_QUALITY;

typedef enum
{
    /* Application's state machine's initial state. */
    BMD101_SYNC1=0,
    BMD101_SYNC2,
    BMD101_PLEN,
    BMD101_DATA,
    BMD101_CHKSUM
} BMD101_PARSER_STATES;

BMD101_PARSER_STATES ParserState = BMD101_SYNC1;
uint8_t BMD101_pLength;
uint8_t BMD101_payload_idx = 0;
uint8_t BMD101_payload[256];
uint8_t BMD101_chksum;
uint8_t BMD101_SignalQaulity = SENSOR_OFF;  // Signal Quality

#define ECG_TAKE_SAMPLES           2000     // Take 2000 samples to calculate
#define ECG_WAVE_UPDATE_RATE       20      // Heart Rate wave update after N new samples coming
int16_t ECG_SampleBufferRingIdx = 0;        // ECG Raw sample buffer ring index (latest)
int16_t ECG_SampleBuffer[ECG_TAKE_SAMPLES]; // ECG Raw sample buffer
uint8_t ECG_HeartRate = 0;

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************

#define ECG_MOVING_AVG_WINDOW_SIZE 16                    // Moving Average Window Size
int16_t ECG_MovingAvgWindowRingIdx = 0;                  // ECG Moving Average Window ring index (latest)
int16_t ECG_MovingAvgWindow[ECG_MOVING_AVG_WINDOW_SIZE]; // ECG Moving Average Window
int16_t APP_ECG_MovingAverage( int16_t ECG_Raw )
{
    int32_t ECG_MovingAvgResult;

    ECG_MovingAvgWindow[ECG_MovingAvgWindowRingIdx] = ECG_Raw;
    ECG_MovingAvgWindowRingIdx = (ECG_MovingAvgWindowRingIdx+1)%ECG_MOVING_AVG_WINDOW_SIZE;
    ECG_MovingAvgResult = 0;
    for( int i=0 ; i<ECG_MOVING_AVG_WINDOW_SIZE ; i++ )
    {
        ECG_MovingAvgResult+=ECG_MovingAvgWindow[i];
    }

    return (int16_t)(ECG_MovingAvgResult/ECG_MOVING_AVG_WINDOW_SIZE);
}

#define ECG_IIR_ALPHA  0.1f        // IIR Filter smoothing factor
int16_t ECG_IIF_Filted = 0;        // IIR Filtered sample
int16_t APP_ECG_IIR( int16_t ECG_Raw )
{
    ECG_IIF_Filted = (int16_t)((ECG_IIR_ALPHA*(float)ECG_Raw) + ((1.0f-ECG_IIR_ALPHA)*(float)ECG_IIF_Filted));

    return ECG_IIF_Filted;
}

#define PULSE_WINDOW         20
#define PULSE_THRESHOLD      1500
void APP_ECG_Output( int16_t *SampleBuf, int16_t RingIdx )
{
    int16_t ECG_RawMin = 0x7FFF; // ECG Raw Minimum
    int16_t ECG_RawMax = 0x8000; // ECG Raw Maximum
    int16_t ECG_WinMin = 0x7FFF; // ECG Window Minimum
    int16_t ECG_WinMax = 0x8000; // ECG Window Maximum
    int16_t ECG_WinMinIdx = 0; // ECG Window Minimum Index
    int16_t ECG_WinMaxIdx = 0; // ECG Window Maximum Index
    int16_t ECG_WinIdx = 0;
    static uint8_t ECG_WinSize = 0;

    ECG_RawMin = 0x7FFF; // ECG Raw Minimum
    ECG_RawMax = 0x8000; // ECG Raw Maximum
    ECG_WinMin = 0x7FFF; // ECG Window Minimum
    ECG_WinMax = 0x8000; // ECG Window Maximum
    for( int j=0 ; j<ECG_TAKE_SAMPLES ; j++)
    {
        // Find Min and Max data in collected ECG Raw buffers for UI upper/lower boundary
        if(ECG_RawMin>SampleBuf[j]) ECG_RawMin=SampleBuf[j];
        if(ECG_RawMax<SampleBuf[j]) ECG_RawMax=SampleBuf[j];

        // Find Min and Max data in Window of ECG Raw buffers for Heart Beat sound
        if( j<PULSE_WINDOW )
        {
            ECG_WinIdx = (j+RingIdx+ECG_TAKE_SAMPLES-PULSE_WINDOW)%ECG_TAKE_SAMPLES;
            if(ECG_WinMin>SampleBuf[ECG_WinIdx])
            {
                ECG_WinMin=SampleBuf[ECG_WinIdx];
                ECG_WinMinIdx=ECG_WinIdx;
            }
            if(ECG_WinMax<SampleBuf[ECG_WinIdx])
            {
                ECG_WinMax=SampleBuf[ECG_WinIdx];
                ECG_WinMaxIdx=ECG_WinIdx;
            }
        }
    }

    // Check Heart Beat again after a full Window Size
    if( ++ECG_WinSize>PULSE_WINDOW )
    {
        ECG_WinSize = PULSE_WINDOW;
        // Heart Beat Detect
        // Window index of Sample Max is less than Sample Min and
        // Sample Max - Min is large than PULSE_THRESHOLD
        if( ((ECG_WinMaxIdx+ECG_TAKE_SAMPLES-RingIdx)%ECG_TAKE_SAMPLES <
             (ECG_WinMinIdx+ECG_TAKE_SAMPLES-RingIdx)%ECG_TAKE_SAMPLES ) &&
            (SampleBuf[ECG_WinMaxIdx]-SampleBuf[ECG_WinMinIdx] > PULSE_THRESHOLD) )
        {
            LED1_Set();
            ECG_WinSize = 0;
        }
        else
        {
            LED1_Clear();
        }
    }

    APP_OLED_ECG_Wave(ECG_RawMin, ECG_RawMax, SampleBuf[RingIdx]);
}

bool SensorInference = false;
uint8_t UART_ReadByte[1];
bool buffer_init = false;
uint16_t count = 0;
void BMD101_CODE_Parser( uint8_t *pPayload, uint8_t Length )
{
    int i;
    uint8_t ECG_Raw[2] = {0,0};
    int16_t ECG_Signal = 0;
    int16_t ECG_RawFiltered = 0; // Filtered Raw

    for( i=0 ; i<Length ; i++ )
    {
        if( pPayload[i]==BMD101_EXTENDED_CODE_LEVEL )
            continue;

        switch( pPayload[i] )
        {
        case BMD101_CODE_SIGNAL_QUALITY: // Signal Quality (0—sensor off, 200—sensor on), 1 byte w/o LENGTH byte
            if( i<Length-1 )
            {
                i++;
                BMD101_SignalQaulity = pPayload[i];
#if DEBUG_ENABLE
                myprintf("\033[3;1HSignal Quality = %03d(0—sensor off, 200—sensor on)", BMD101_SignalQaulity);
#endif
                // Output Sensor Detect
                APP_OLED_ECG_Detect( BMD101_SignalQaulity );
            }
            break;

        case BMD101_CODE_HEART_RATE: // Real-time Heart Rate (Beats Per Minute), 1 byte w/o LENGTH byte
            if( i<Length-1 )
            {
                i++;
                ECG_HeartRate = pPayload[i];
#if DEBUG_ENABLE
                myprintf("\033[4;1HHeart Rate = %03d(bpm)", ECG_HeartRate);
#endif
                if( BMD101_SignalQaulity == SENSOR_ON )
                {
                    // Output Result UI in interval of RESULT_UPDATE_RATE
                    APP_OLED_ECG_HeartRate( ECG_HeartRate );
                    // Output Filter Type
                    APP_OLED_ECG_FilterType( VR1_Pos );
                }
            }
            break;

        case BMD101_CODE_ECG_RAW: // 16-bit Raw Data (2’s Complement), 2 bytes w/ LENGTH byte
            if( i<Length-3 )
            {
                i++;
                if( pPayload[i]==0x2 ) // Check LENGTH byte
                {
                    i++;
                    ECG_Raw[0] = pPayload[i];
                    i++;
                    ECG_Raw[1] = pPayload[i];
                    ECG_Signal = ECG_Raw[0]<<8|ECG_Raw[1];
#if DV_ENABLE
//                    uint8_t DVbuf[5] = {0x03, ECG_Raw[1], ECG_Raw[0], ECG_HeartRate, 0xFC };
//                    SERCOM5_USART_Write( DVbuf, sizeof(DVbuf) );
//                    while( SERCOM5_USART_WriteIsBusy() ) {}
#endif
                       
                    if( BMD101_SignalQaulity == SENSOR_ON )
                    {
                        //start the inference once it get the triggered signal('k')
                        if(SERCOM5_USART_Read(UART_ReadByte, 1))
                        {
                            // Echo UART input
                            SERCOM5_USART_Write(UART_ReadByte, 1);
                            while( SERCOM5_USART_WriteIsBusy() ) {}
                            // Check key-in byte
                            switch( UART_ReadByte[0] )
                            {
                            case 'k': case 'K':
                                SensorInference = true;
                                buffer_init = true;
                                break;
                            }
                        }
                        // inference control
                        if(SensorInference==true)
                        {
                            if(buffer_init==true)
                            {
                                // initialize the buffer for model input
                                sml_recognition_run(&ECG_Signal, 1, buffer_init);
                                buffer_init = false;
                                
                                // Start the Inference Interval Timer.
                                TC4_DelayMS( INFERENCE_INTERVAL, DELAY_TIMER_INFERENCE_INTERVAL );
                                // Start the 5s Interval Timer.
                                TC4_DelayMS( five_sec_INTERVAL, DELAY_TIMER_5_sec_INTERVAL );
                            }
                            else
                            {
                                if(TC4_DelayIsComplete( DELAY_TIMER_5_sec_INTERVAL))
                                {
                                    myprintf("%d\r\n",count);
                                    count = 0;
                                }
                                if(TC4_DelayIsComplete( DELAY_TIMER_INFERENCE_INTERVAL))
                                {
                                    count = count + 1;
                                    
                                    // send one data point to the model for accumulation, as the data accumulate as many as model input, it will return 0 or 1, otherwise, it will return negative value 
                                    switch( sml_recognition_run(&ECG_Signal, 1, buffer_init) )
                                    {
                                    case 1:  APP_OLED_ML_Inference("AFib"); 
                                            myprintf("AFib\r\n");
                                            // as the model inference complete one data, it will stop
                                            SensorInference = false;
                                            break;
                                    case 2:  APP_OLED_ML_Inference("Normal");
                                            myprintf("Normal\r\n");
                                            // as the model inference complete one data, it will stop
                                            SensorInference = false;
                                            break;
                                    default: //when data points accumulation is not sufficient for model input, it will do nothing
                                            break;
                                    }
                                    // Re-Start the Inference Interval Timer.
                                    TC4_DelayMS( INFERENCE_INTERVAL, DELAY_TIMER_INFERENCE_INTERVAL );
                                }
                            }
                        }
                            
                        // Select Filter Type
                        if     ( VR1_Pos<=1 ) { ECG_RawFiltered = ECG_Signal; } // No Filter
                        else if( VR1_Pos>=4 ) { ECG_RawFiltered = APP_ECG_MovingAverage( ECG_Signal ); } // Moving Average
                        else                  { ECG_RawFiltered = APP_ECG_IIR( ECG_Signal ); } // IIR Filter
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           
                        // Move in new Filtered ECG data to end of ring buffer
                        ECG_SampleBuffer[ECG_SampleBufferRingIdx]=ECG_RawFiltered;

                        // Output Heart Beat sound and Wave UI in interval of ECG_WAVE_UPDATE_RATE
                        if( ECG_SampleBufferRingIdx%ECG_WAVE_UPDATE_RATE==0 )
                        {
                            APP_ECG_Output( ECG_SampleBuffer, ECG_SampleBufferRingIdx );
                        }

                        // Increase Ring Index
                        ECG_SampleBufferRingIdx = (ECG_SampleBufferRingIdx+1)%ECG_TAKE_SAMPLES;
                    }
                    
                }
            }
            break;

        case BMD101_CODE_DONT_CARE1:     // Don’t Care, 1 byte w/o LENGTH byte
        case BMD101_CODE_DONT_CARE2:     // Don’t Care, 5 bytes w/ LENGTH byte
        case BMD101_CODE_DONT_CARE3:     // Don’t Care, 3 bytes w/ LENGTH byte
        default:                         // Other CODE
            if( pPayload[i]>=0x80 )
            {
                // bytes w/ LENGTH byte
                if( i<Length-1 )
                {
                    // Shift LENGTH bytes
                    i+=(pPayload[i+1]+1);
                }
            }
            else
            {
                // byte w/o LENGTH byte
                i++;
            }
            break;
        } // switch( pPayload[i] )
    } // for( i=0 ; i<Length ; i++ )
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************


/*******************************************************************************
  Function:
    void APP_ECG_Initialize ( void )

  Remarks:
    See prototype in app_ecg.h.
 */

void APP_ECG_Initialize ( void )
{
    /* Place the App state machine in its initial state. */
    app_ecgData.state = APP_ECG_STATE_INIT;
}

/******************************************************************************
  Function:
    void APP_ECG_Tasks ( void )

  Remarks:
    See prototype in app_ecg.h.
 */

void APP_ECG_Tasks ( void )
{
    /* Check the application's current state. */
    switch ( app_ecgData.state )
    {
    /* Application's initial state. */
    case APP_ECG_STATE_INIT:
        if( APP_OLED_Get_State()!=APP_OLED_STATE_UPDATE )
            break;

        // Show Graphic only
        GPL_LayerShow( LAYER_GRAPHIC, GPL_SHOW );
        GPL_LayerShow( LAYER_STRING, GPL_SHOW );

        app_ecgData.state = APP_ECG_STATE_DATA_READ;
        break;

    case APP_ECG_STATE_DATA_READ:
        // Read current RX buffer
        memset( UartReadBuffer, 0, sizeof(UartReadBuffer) );
        UartReadSize = SERCOM2_USART_Read(UartReadBuffer, SERCOM2_USART_ReadCountGet());
        if( UartReadSize )
        {
            UartRingTailSize = sizeof(UartRingBuffer)-UartRingLast;
#if DEBUG_ENABLE
            myprintf("\033[1;1HRX=%04d, Last=%04d, Tail=%04d", UartReadSize, UartRingLast, UartRingTailSize );
#endif
            if( UartRingTailSize<UartReadSize )
            {
                // fill in tail of RX ring buffer
                memcpy( UartRingBuffer+UartRingLast, UartReadBuffer, UartRingTailSize );
                // copy round of RX ring buffer
                memcpy( UartRingBuffer, UartReadBuffer+UartRingTailSize, UartReadSize-UartRingTailSize );
            }
            else
            {
                // fill in tail of RX ring buffer by whole RX buffer
                memcpy( UartRingBuffer+UartRingLast, UartReadBuffer, UartReadSize );
            }
            // BMD101 UART protocol
            // SYNC SYNC pLength payload[] chksum
            // 0xAA 0xAA 0~255   [Date]    8-bit
            for( int i=0 ; i<UartReadSize ; i++, UartRingLast = (UartRingLast+1)%sizeof(UartRingBuffer) )
            {
                switch( ParserState )
                {
                case BMD101_SYNC1:
                    if( UartRingBuffer[UartRingLast]==BMD101_SYNC_BYTE )
                        ParserState = BMD101_SYNC2;
                    break;

                case BMD101_SYNC2:
                    if( UartRingBuffer[UartRingLast]==BMD101_SYNC_BYTE )
                        ParserState = BMD101_PLEN;
                    else
                        ParserState = BMD101_SYNC1;
                    break;

                case BMD101_PLEN:
                    BMD101_pLength = UartRingBuffer[UartRingLast];
                    if( BMD101_pLength<256 )
                    {
#if DEBUG_ENABLE
                        myprintf("\033[2;1HpLength=%04d", BMD101_pLength);
#endif
                        BMD101_payload_idx = 0;
                        BMD101_chksum = 0;
                        memset( BMD101_payload, 0, sizeof(BMD101_payload) );
                        ParserState = BMD101_DATA;
                    }
                    else
                        ParserState = BMD101_SYNC1;
                    break;

                case BMD101_DATA:
                    BMD101_payload[BMD101_payload_idx] = UartRingBuffer[UartRingLast];
                    BMD101_chksum += BMD101_payload[BMD101_payload_idx];
                    BMD101_payload_idx++;
                    if( BMD101_payload_idx>=BMD101_pLength )
                        ParserState = BMD101_CHKSUM;
                    break;

                case BMD101_CHKSUM:
                    BMD101_chksum^=0xFF;
                    if( BMD101_chksum == UartRingBuffer[UartRingLast] )
                    {
                        // Parser the Payload CODEs and display GUI
                        BMD101_CODE_Parser( BMD101_payload, BMD101_pLength );
                    }
#if DEBUG_ENABLE
                    else
                    {
                        // Payload Checksum Error
                        myprintf("\033[5;1HChecksum Err!");
                    }
#endif
                    ParserState = BMD101_SYNC1;
                    break;
                } // End of parser state switch
            } //  End of for loop
        } // End of if( UartReadSize )
        break;

    case APP_ECG_STATE_ERROR:
        app_ecgData.state = APP_ECG_STATE_IDLE;
        break;

    case APP_ECG_STATE_IDLE:
    /* The default state should never be executed. */
    default:
        {
            break;
        }
    }
}


/*******************************************************************************
 End of File
 */

