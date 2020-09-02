#include "rui.h"
#include "sensor.h"

bsp_sensor_data_t bsp_sensor;
/********************************************************************************************************
 * get Battery Volage
********************************************************************************************************/
// #define VREFINT_CAL       ( *( uint16_t* )0x1FF80078 )  //VREF calibration value
extern RUI_GPIO_ST Bat_level;
uint32_t BoardBatteryMeasureVolage( float *voltage )
{
    uint16_t vdiv = 0;
	rui_adc_get(&Bat_level,&vdiv);
    *voltage = (5 * vdiv )/3;
}

