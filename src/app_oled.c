/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app_oled.c

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
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "app_oled.h"
#include "Microchip_Logo.h"
#include "CString.h"
#include "GraphicLib.h"

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_OLED_Initialize function.

    Application strings and buffers are be defined outside this structure.
*/
APP_OLED_DATA app_oledData;

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************
#define LCM_ROW               8    // LCM Text Row count
#define LCM_COL               18   // LCM Text Column count

int8_t LogoX, LogoY;
bool UI_Language = UI_CHINESE; // 0: English, 1:Chinese (BT2 to toogle)

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
APP_OLED_LANG_ID APP_OLED_Get_Language( void )
{
    return UI_Language;
}

// ****************************************************************************
//  ECG GUI
// ****************************************************************************
void APP_OLED_ECG_Detect( uint8_t SignalQaulity )
{
    char OutStr[20];

    GPL_LayerClean( LAYER_STRING );

    if( SignalQaulity==0 )
    {
        GPL_LayerSet( LAYER_STRING );
        if( UI_Language == UI_CHINESE )
            GPL_DrawBitmap((LCM_WIDTH-78)/2, 0, 78, 16, BG_SOLID, CString1);
        else if( UI_Language == UI_ENGLISH )
        {
            sprintf( OutStr, "Put your Finger on" );
            GPL_DrawString((LCM_WIDTH-(strlen(OutStr)*FONT_WIDTH))/2, 0, OutStr, BG_SOLID, TEXT_NORMAL);
        }
    }
}

void APP_OLED_ECG_HeartRate( int16_t nHR )
{
    char OutStr[20];

    GPL_LayerClean( LAYER_STRING );
    GPL_LayerSet( LAYER_STRING );

    if( UI_Language == UI_CHINESE )
    {
        GPL_DrawBitmap( 0,  0, 52, 16, BG_SOLID, CString4);
        sprintf( OutStr, "%3d", nHR );
        GPL_DrawString(56, 0, OutStr, BG_SOLID, TEXT_NORMAL);
        GPL_DrawBitmap(86,  0, 42, 16, BG_SOLID, CString41);
    }
    else
    {
        sprintf( OutStr, "HR   : %3d bpm", nHR );
        GPL_DrawString(0, 0, OutStr, BG_SOLID, TEXT_NORMAL);
    }
}

void APP_OLED_ECG_FilterType( uint8_t FilterType )
{
    char OutStr[20];

    GPL_LayerSet( LAYER_STRING );

    if     ( FilterType<=1 ) { sprintf( OutStr, "No Filter" ); }
    else if( FilterType>=4 ) { sprintf( OutStr, "Moving Avg" ); }
    else                     { sprintf( OutStr, "IIR Filter" ); }
    GPL_DrawString(0, 13, OutStr, BG_SOLID, TEXT_NORMAL);
}

#if 1 // Scan Style
#define ECG_WAVE_SIZE        (LCM_WIDTH)
#define ECG_WAVE_HEIGHT      (LCM_HEIGHT-20)
#define ECG_WAVE_START       (0)
#define ECG_WAVE_ZERO        (LCM_HEIGHT)
void APP_OLED_ECG_Wave( int16_t ECG_min, int16_t ECG_max, int16_t ECG_data )
{
    static uint8_t RingIdx = 0;
    static uint8_t ECG_Wave[ECG_WAVE_SIZE];

    GPL_LayerClean( LAYER_GRAPHIC );
    GPL_LayerSet( LAYER_GRAPHIC );
    GPL_SetPenSize( 1 );

    ECG_Wave[RingIdx]  = (ECG_data-ECG_min)*ECG_WAVE_HEIGHT/(ECG_max-ECG_min+1);
    if( ECG_Wave[RingIdx]> ECG_WAVE_HEIGHT ) ECG_Wave[RingIdx] = ECG_WAVE_HEIGHT;
    if( ECG_Wave[RingIdx]< 0               ) ECG_Wave[RingIdx] = 0;

#if 0
    // Find Peak
    if( ECG_Wave[RingIdx]-ECG_Wave[((RingIdx-2)+ECG_WAVE_SIZE)%ECG_WAVE_SIZE] >  8 &&
        ECG_Wave[RingIdx]-ECG_Wave[((RingIdx-1)+ECG_WAVE_SIZE)%ECG_WAVE_SIZE] >  4 &&
        ECG_Wave[RingIdx]-ECG_Wave[((RingIdx+1)+ECG_WAVE_SIZE)%ECG_WAVE_SIZE] >  4 &&
        ECG_Wave[RingIdx]-ECG_Wave[((RingIdx+2)+ECG_WAVE_SIZE)%ECG_WAVE_SIZE] >  8 )
        LED1_Set();
    // Find Valley
    if( ECG_Wave[RingIdx]-ECG_Wave[((RingIdx-2)+ECG_WAVE_SIZE)%ECG_WAVE_SIZE] < -8 &&
        ECG_Wave[RingIdx]-ECG_Wave[((RingIdx-1)+ECG_WAVE_SIZE)%ECG_WAVE_SIZE] < -4 &&
        ECG_Wave[RingIdx]-ECG_Wave[((RingIdx+1)+ECG_WAVE_SIZE)%ECG_WAVE_SIZE] < -4 &&
        ECG_Wave[RingIdx]-ECG_Wave[((RingIdx+2)+ECG_WAVE_SIZE)%ECG_WAVE_SIZE] < -8 )
        LED1_Clear();
#endif

    for( int Col=0 ; Col<ECG_WAVE_SIZE-1 ; Col++ )
    {
        GPL_DrawLine(ECG_WAVE_START+Col, ECG_WAVE_ZERO-ECG_Wave[Col], ECG_WAVE_START+Col+1, ECG_WAVE_ZERO-ECG_Wave[Col+1]);
    }

    // Move to next buffer
    RingIdx = (RingIdx+1)%ECG_WAVE_SIZE;
}
#else // DV Style
#define ECG_WAVE_SIZE        (LCM_WIDTH)
#define ECG_WAVE_HEIGHT      (LCM_HEIGHT-20)
#define ECG_WAVE_START       (LCM_WIDTH)
#define ECG_WAVE_ZERO        (LCM_HEIGHT)
void APP_OLED_ECG_Wave( int16_t ECG_min, int16_t ECG_max, int16_t ECG_data )
{
    static uint8_t RingIdx = 0;
    static int16_t ECG_Wave[ECG_WAVE_SIZE];
    uint8_t CurIdx, PreIdx;

    GPL_LayerClean( LAYER_GRAPHIC );
    GPL_LayerSet( LAYER_GRAPHIC );
    GPL_SetPenSize( 1 );

    ECG_Wave[RingIdx]  = (ECG_data-ECG_min)*ECG_WAVE_HEIGHT/(ECG_max-ECG_min+1);
    if( ECG_Wave[RingIdx]> ECG_WAVE_HEIGHT ) ECG_Wave[RingIdx] = ECG_WAVE_HEIGHT;
    if( ECG_Wave[RingIdx]< 0               ) ECG_Wave[RingIdx] = 0;

    for( int Col=0 ; Col<ECG_WAVE_SIZE-1 ; Col++ )
    {
        CurIdx = (RingIdx-Col+ECG_WAVE_SIZE)%ECG_WAVE_SIZE;
        PreIdx = (RingIdx-1-Col+ECG_WAVE_SIZE)%ECG_WAVE_SIZE;
        GPL_DrawLine(ECG_WAVE_START-Col, ECG_WAVE_ZERO-ECG_Wave[CurIdx], ECG_WAVE_START-Col-1, ECG_WAVE_ZERO-ECG_Wave[PreIdx]);
    }

    // Move to next buffer
    RingIdx = (RingIdx+1)%ECG_WAVE_SIZE;
}
#endif

// ****************************************************************************
//  AI/ML GUI
// ****************************************************************************
void APP_OLED_ML_Inference( char *OutStr )
{
    GPL_LayerSet( LAYER_STRING );
    GPL_DrawString((LCM_WIDTH-strlen(OutStr)*7), 25-FONT_HEIGHT_REAL, OutStr, BG_SOLID, TEXT_NORMAL);
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_OLED_Initialize ( void )

  Remarks:
    See prototype in app_oled.h.
 */

void APP_OLED_Initialize ( void )
{
    app_oledData.state = APP_OLED_STATE_INIT;
}

APP_OLED_STATES APP_OLED_Get_State ( void )
{
    return app_oledData.state;
}

/******************************************************************************
  Function:
    void APP_OLED_Tasks ( void )

  Remarks:
    See prototype in app_oled.h.
 */
void APP_OLED_Tasks ( void )
{
    switch ( app_oledData.state )
    {
        case APP_OLED_STATE_INIT:
        {
            // LCM initial @ LCM.c
            if( GPL_ScreenInit()==false )
            {
                app_oledData.state = APP_OLED_STATE_ERROR;
                break;
            }

            // Initial Splash Bitmap location and Layer
            LogoX = 128;
            LogoY = 0;

            // Show Graphic only
            GPL_LayerShow( LAYER_GRAPHIC, GPL_SHOW );
            GPL_LayerShow( LAYER_STRING, GPL_HIDE );

            app_oledData.state = APP_OLED_STATE_SPLASH;
            break;
        }

        case APP_OLED_STATE_SPLASH:
        {
            // OLED LCM Logo Animation start
            if( (LogoX-=4)>=0 )
            {
                // Fill LCM with LOGO
                GPL_LayerSet( LAYER_GRAPHIC );
                GPL_ScreenClean();
                GPL_DrawBitmap(LogoX, LogoY, LCM_WIDTH, LCM_HEIGHT, BG_SOLID, Microchip_Logo);
                GPL_ScreenUpdate();
            }
            else
            {
                TC4_DelayMS( SPLASH_WAIT_DELAY, DELAY_TIMER_SPLASH_WAIT );
                app_oledData.state = APP_OLED_STATE_WAIT_SPLASH_COMPLETE;
            }
            break;
        }

        case APP_OLED_STATE_WAIT_SPLASH_COMPLETE:
        {
            if( TC4_DelayIsComplete( DELAY_TIMER_SPLASH_WAIT ) )
            {
                // Screen Clean
                GPL_LayerSet( LAYER_GRAPHIC );
                GPL_ScreenClean();
                GPL_ScreenUpdate();

                TC4_DelayMS( GRPAHIC_UPDATE_DELAY, DELAY_TIMER_GRPAHIC_UPDATE );

                // Show all layer
                GPL_LayerShow( LAYER_GRAPHIC, GPL_SHOW );
                GPL_LayerShow( LAYER_STRING, GPL_SHOW );

                app_oledData.state = APP_OLED_STATE_UPDATE;
            }
            break;
        }

        case APP_OLED_STATE_UPDATE:
        {
            if( !BT2_Get() )
            {
                while(!BT2_Get());
                UI_Language = !UI_Language;
            }

            if( TC4_DelayIsComplete( DELAY_TIMER_GRPAHIC_UPDATE ) )
            {
                GPL_ScreenUpdate();

                TC4_DelayMS( GRPAHIC_UPDATE_DELAY, DELAY_TIMER_GRPAHIC_UPDATE );
            }

            break;
        }

        case APP_OLED_STATE_ERROR:
        {
            app_oledData.state = APP_OLED_STATE_IDLE;
            break;
        }

        case APP_OLED_STATE_IDLE:
        default:
        {
            break;
        }
    }
}


/*******************************************************************************
 End of File
 */

