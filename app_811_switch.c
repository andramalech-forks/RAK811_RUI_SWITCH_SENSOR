#include "rui.h"
#include "board.h"
#include "sensor.h"

static RUI_RETURN_STATUS rui_return_status;
//join cnt
#define JOIN_MAX_CNT 			6
#define LPP_PACKED_PAYLOAD_PORT 	2
#define LPP_DEVICE_PERIOD_CONF_PORT 	11
#define LPP_SENSOR_ENABLE_PORT 		14
#define LPP_CONFIG_MASK_TXPERIOD 	0x02
#define LPP_TXPERIOD_SIZE 		4
#define LPP_SENSOR_ENABLE_SIZE 		1
static uint8_t JoinCnt=0;
RUI_LORA_STATUS_T app_lora_status; //record status 

/*******************************************************************************************
 * The BSP user functions.
 * 
 * *****************************************************************************************/ 

#define SWITCH_1		3
#define SWITCH_2		4
#define SWITCH_3		14
#define SWITCH_4		16

#define LED_1			8
#define I2C_SDA  		19
#define I2C_SCL  		18
#define BAT_LEVEL_CHANNEL	20

#define LED_1_LPP_EN_MASK	0x80
#define SWITCH_1_LPP_EN_MASK	0x01
#define SWITCH_2_LPP_EN_MASK	0x02
#define SWITCH_3_LPP_EN_MASK	0x04
#define SWITCH_4_LPP_EN_MASK	0x08

const uint8_t level[2]={0,1};
static uint8_t extdigints=0x00;
static uint8_t lpp_enable_sensor_mask=0xFF;
static uint8_t lpp_sensor_mask_after_enabled=0x00;

#define low     &level[0]
#define high    &level[1]
RUI_GPIO_ST Led_Red;  //send data successed and join indicator light
RUI_GPIO_ST Bat_level;
RUI_GPIO_ST Switch_One;		// ext interupt switch #1
RUI_GPIO_ST Switch_Two;		// ext interupt switch #2
RUI_GPIO_ST Switch_Three;	// ext interupt switch #3
RUI_GPIO_ST Switch_Four;	// ext interupt switch #4
RUI_I2C_ST I2c_1;
TimerEvent_t Join_Ok_Timer;
TimerEvent_t LoRa_send_ok_Timer;  //LoRa send out indicator light
volatile static bool autosend_flag = false;    //auto send flag
static uint8_t a[80]={};    // Data buffer to be sent by lora
static uint8_t sensor_data_cnt=0;  //send data counter by LoRa 
bool IsJoiningflag= false;  //flag whether joining or not status
bool sample_flag = false;  //flag sensor sample record for print sensor data by AT command 
bool sample_status = false;  //current whether sample sensor completely
bool sendfull = true;  //flag whether send all sensor data 




void rui_lora_autosend_callback(void)  //auto_send timeout event callback
{
    autosend_flag = true;
    IsJoiningflag = false;      
}

void handle_int_sw1(void)
{
	if(extdigints == 0x00 && lpp_enable_sensor_mask & SWITCH_1_LPP_EN_MASK)
	{
		extdigints = extdigints | SWITCH_1_LPP_EN_MASK;
		autosend_flag = true;
		IsJoiningflag = false; 
		RUI_LOG_PRINTF("sw1 int fired! \r\n");
	}
}
void handle_int_sw2(void)
{		
	if(extdigints == 0x00 && lpp_enable_sensor_mask & SWITCH_2_LPP_EN_MASK )
	{
		if(lpp_sensor_mask_after_enabled & SWITCH_2_LPP_EN_MASK )
		{
			// avoid firing after re-enable 
			lpp_sensor_mask_after_enabled = lpp_sensor_mask_after_enabled & ~SWITCH_2_LPP_EN_MASK;
		}
		else
		{
			extdigints = extdigints | SWITCH_2_LPP_EN_MASK;
			autosend_flag = true;
			IsJoiningflag = false; 
			RUI_LOG_PRINTF("sw2 int fired! \r\n");
		}
	}		
}


void OnJoin_Ok_TimerEvent(void)
{  
    rui_timer_stop(&Join_Ok_Timer);
    rui_gpio_rw(RUI_IF_WRITE,&Led_Red, low);

    rui_lora_get_status(false,&app_lora_status);;//The query gets the current device status 
    if(app_lora_status.autosend_status)
    {
        autosend_flag = true;  //set autosend_flag after join LoRaWAN succeeded 
    }
}

void OnLoRa_send_ok_TimerEvent(void)
{
    rui_timer_stop(&LoRa_send_ok_Timer);
    rui_gpio_rw(RUI_IF_WRITE,&Led_Red, low);

    rui_lora_get_status(false,&app_lora_status);  //The query gets the current status
    switch(app_lora_status.autosend_status)
    {
        case RUI_AUTO_ENABLE_SLEEP:rui_lora_set_send_interval(RUI_AUTO_ENABLE_SLEEP,app_lora_status.lorasend_interval);  //start autosend_timer after send success
            rui_delay_ms(5);  
            break;
        case RUI_AUTO_ENABLE_NORMAL:rui_lora_set_send_interval(RUI_AUTO_ENABLE_NORMAL,app_lora_status.lorasend_interval);  //start autosend_timer after send success
            break;
        default:break;
    }
}
void bsp_led_init(void)
{
   
    Led_Red.pin_num = LED_1;  
    Led_Red.dir = RUI_GPIO_PIN_DIR_OUTPUT;
    Led_Red.pull = RUI_GPIO_PIN_NOPULL;

    rui_gpio_init(&Led_Red);

    rui_gpio_rw(RUI_IF_WRITE,&Led_Red,high);
    rui_delay_ms(200);
    rui_gpio_rw(RUI_IF_WRITE,&Led_Red,low);

    rui_timer_init(&Join_Ok_Timer,OnJoin_Ok_TimerEvent);
    rui_timer_init(&LoRa_send_ok_Timer,OnLoRa_send_ok_TimerEvent);
    rui_timer_setvalue(&Join_Ok_Timer,2000);
    rui_timer_setvalue(&LoRa_send_ok_Timer,100);
}
void bsp_di_init(void)
{
    Switch_One.pin_num = SWITCH_1;
    Switch_One.dir = RUI_GPIO_PIN_DIR_INPUT;
    Switch_One.pull = RUI_GPIO_PIN_NOPULL;
    rui_gpio_interrupt(true,Switch_One,RUI_GPIO_EDGE_FALL_RAISE,RUI_GPIO_IRQ_LOW_PRIORITY,handle_int_sw1);
    rui_gpio_init(&Switch_One);

    Switch_Two.pin_num = SWITCH_2;
    Switch_Two.dir = RUI_GPIO_PIN_DIR_INPUT;
    Switch_Two.pull = RUI_GPIO_PIN_NOPULL;
    rui_gpio_interrupt(true,Switch_Two,RUI_GPIO_EDGE_FALL_RAISE,RUI_GPIO_IRQ_LOW_PRIORITY,handle_int_sw2);
    rui_gpio_init(&Switch_Two);

    Switch_Three.pin_num = SWITCH_3;
    Switch_Three.dir = RUI_GPIO_PIN_DIR_INPUT;
    Switch_Three.pull = RUI_GPIO_PIN_NOPULL;
    rui_gpio_init(&Switch_Three);

    Switch_Four.pin_num = SWITCH_4;
    Switch_Four.dir = RUI_GPIO_PIN_DIR_INPUT;
    Switch_Four.pull = RUI_GPIO_PIN_NOPULL;
    rui_gpio_init(&Switch_Four);

}
void bsp_adc_init(void)
{
    Bat_level.pin_num = BAT_LEVEL_CHANNEL;
    Bat_level.dir = RUI_GPIO_PIN_DIR_INPUT;
    Bat_level.pull = RUI_GPIO_PIN_NOPULL;
    rui_adc_init(&Bat_level);

}
void bsp_i2c_init(void)
{
    I2c_1.INSTANCE_ID = 1;
    I2c_1.PIN_SDA = I2C_SDA;
    I2c_1.PIN_SCL = I2C_SCL;
    I2c_1.FREQUENCY = RUI_I2C_FREQ_100K;

    if(rui_i2c_init(&I2c_1) != RUI_STATUS_OK)RUI_LOG_PRINTF("I2C init error.\r\n");

    rui_delay_ms(50);

}
void bsp_init(void)
{
    bsp_led_init();
    bsp_di_init();
    bsp_adc_init();
    bsp_i2c_init();
    //BME680_Init();
}


uint8_t lpp_cnt=0;  //record lpp package count
typedef struct 
{   uint8_t startcnt;
    uint8_t size;
}lpp_data_t;
lpp_data_t lpp_data[10];
void user_lora_send(void)
{
    uint8_t dr;
    uint16_t ploadsize;
    static uint16_t temp_cnt=0;
    uint16_t temp_size=0;  //send package size 
    uint8_t* Psend_start;
    if(autosend_flag)
    {
        autosend_flag = false;
        rui_lora_get_dr(&dr,&ploadsize);
        if(ploadsize < sensor_data_cnt)
        {
            sendfull = false;  //need subcontract send
            Psend_start = &a[lpp_data[temp_cnt].startcnt];  
            if(lpp_data[temp_cnt].size > ploadsize)
            {
                RUI_LOG_PRINTF("ERROR: RUI_AT_LORA_LENGTH_ERROR %d\r\n",RUI_AT_LORA_LENGTH_ERROR);
                sample_status = false;
                sendfull = true;
                lpp_cnt = 0;
                temp_cnt = 0;
                sensor_data_cnt=0; 
                rui_lora_get_status(false,&app_lora_status); 
                switch(app_lora_status.autosend_status)
                {
                    case RUI_AUTO_ENABLE_SLEEP:rui_lora_set_send_interval(RUI_AUTO_ENABLE_SLEEP,app_lora_status.lorasend_interval);  //start autosend_timer after send success
                        break;
                    case RUI_AUTO_ENABLE_NORMAL:rui_lora_set_send_interval(RUI_AUTO_ENABLE_NORMAL,app_lora_status.lorasend_interval);  //start autosend_timer after send success
                        break;
                    default:break;
                }
                return; 
            }                        
            for(;temp_cnt <= lpp_cnt; temp_cnt++)
            {
                // UartPrint("lpp_cnt:%d, temp_cnt:%d,temp_size:%d,lpp_data[%d].size:%d,Msize:%d\r\n",lpp_cnt,temp_cnt,temp_size,temp_cnt,lpp_data[temp_cnt].size,ploadsize);
                if(ploadsize < (temp_size + lpp_data[temp_cnt].size))
                {                                                              
                    rui_return_status = rui_lora_send(8,Psend_start,temp_size);
                    switch(rui_return_status)
                    {
                        case RUI_STATUS_OK:return;
                        default: RUI_LOG_PRINTF("[LoRa]: send error %d\r\n",rui_return_status);  
                            autosend_flag = true;                                      
                            break;
                    }                
                }else
                {                   
                    if(temp_cnt == lpp_cnt)
                    {
                        rui_return_status = rui_lora_send(8,Psend_start,temp_size);
                        switch(rui_return_status)
                        {
                            case RUI_STATUS_OK:RUI_LOG_PRINTF("[LoRa]: send out\r\n");
                                sample_status = false;
                                sendfull = true;
                                lpp_cnt = 0;
                                temp_cnt = 0;
                                sensor_data_cnt=0; 
                                return;
                                break;
                            default: RUI_LOG_PRINTF("[LoRa]: send error %d\r\n",rui_return_status);  
                                autosend_flag = true;                                      
                                break;
                        } 
                    }else 
                    {
                        temp_size += lpp_data[temp_cnt].size; 
                    }
                }                   
            }
        }else
        {
            // small payloads!
	    rui_return_status = rui_lora_send(LPP_PACKED_PAYLOAD_PORT,a,sensor_data_cnt);
            switch(rui_return_status)
            {
                case RUI_STATUS_OK:RUI_LOG_PRINTF("[LoRa]: send out\r\n");
                    sample_status = false;
                    sendfull = true;
                    lpp_cnt = 0;
                    sensor_data_cnt=0; 
                    break;
                default: RUI_LOG_PRINTF("[LoRa]: send error %d\r\n",rui_return_status);
                    rui_lora_get_status(false,&app_lora_status); 
                    switch(app_lora_status.autosend_status)
                    {
                        case RUI_AUTO_ENABLE_SLEEP:rui_lora_set_send_interval(RUI_AUTO_ENABLE_SLEEP,app_lora_status.lorasend_interval);  //start autosend_timer after send success
                            break;
                        case RUI_AUTO_ENABLE_NORMAL:rui_lora_set_send_interval(RUI_AUTO_ENABLE_NORMAL,app_lora_status.lorasend_interval);  //start autosend_timer after send success
                            break;
                        default:break;
                    }
                    break;
            }               
        }
    }
                        
}

extern bsp_sensor_data_t bsp_sensor;
void app_loop(void)
{
    int temp=0;         
    int x,y,z;
    uint8_t digvalue;
    uint8_t digbit;       
    rui_lora_get_status(false,&app_lora_status);
    if(app_lora_status.IsJoined)  //if LoRaWAN is joined
    {
        if(autosend_flag) 
        {
            autosend_flag=false;
            rui_delay_ms(5);               

            BoardBatteryMeasureVolage(&bsp_sensor.voltage);
            bsp_sensor.voltage=bsp_sensor.voltage/1000.0;   //convert mV to V
            RUI_LOG_PRINTF("Battery Voltage = %d.%d V \r\n",(uint32_t)(bsp_sensor.voltage), (uint32_t)((bsp_sensor.voltage)*1000-((int32_t)(bsp_sensor.voltage)) * 1000));
            temp=(uint16_t)round(bsp_sensor.voltage*100.0);
            lpp_data[lpp_cnt].startcnt = sensor_data_cnt;
    
	    //a[sensor_data_cnt++]=0x08;
            //a[sensor_data_cnt++]=0x02;
            //a[sensor_data_cnt++]=(temp&0xffff) >> 8;
            //a[sensor_data_cnt++]=temp&0xff;	

    	    digvalue=0x00;        
	    
	    rui_gpio_rw( RUI_IF_READ, &Switch_One, &digbit );
            if( digbit == 0 )
            {digvalue = digvalue | SWITCH_1_LPP_EN_MASK; }
            rui_gpio_rw( RUI_IF_READ, &Switch_Two, &digbit );
            if( digbit == 0 )
            {digvalue = digvalue | SWITCH_2_LPP_EN_MASK; }
	    digvalue = digvalue | extdigints<<4;

	    //LPP frame
	    a[sensor_data_cnt++]=0x00;			// Digital Inputs	(IPSO 3200)
	    a[sensor_data_cnt++]=digvalue;		// 7-4 irqs fired, 3-0 read  value
	    a[sensor_data_cnt++]=0x02;			// Analog Input		(IPSO 3202)
	    a[sensor_data_cnt++]=(temp&0xffff) >> 8;
	    a[sensor_data_cnt++]=temp&0xff;	

	    lpp_data[lpp_cnt].size = sensor_data_cnt - lpp_data[lpp_cnt].startcnt;	
            lpp_cnt++;		

	// switch sensor info here!
            if(1==0)
            {
                lpp_data[lpp_cnt].startcnt = sensor_data_cnt;
                a[sensor_data_cnt++]=0x07;
                a[sensor_data_cnt++]=0x68;
                a[sensor_data_cnt++]=( bsp_sensor.humidity / 500 ) & 0xFF;
                lpp_data[lpp_cnt].size = sensor_data_cnt - lpp_data[lpp_cnt].startcnt;
                lpp_cnt++;

                lpp_data[lpp_cnt].startcnt = sensor_data_cnt;	
                a[sensor_data_cnt++]=0x06;
                a[sensor_data_cnt++]=0x73;
                a[sensor_data_cnt++]=(( bsp_sensor.pressure / 10 ) >> 8 ) & 0xFF;
                a[sensor_data_cnt++]=(bsp_sensor.pressure / 10 ) & 0xFF;
                lpp_data[lpp_cnt].size = sensor_data_cnt - lpp_data[lpp_cnt].startcnt;
                lpp_cnt++;
			
                lpp_data[lpp_cnt].startcnt = sensor_data_cnt;
                a[sensor_data_cnt++]=0x02;
                a[sensor_data_cnt++]=0x67;
                a[sensor_data_cnt++]=(( bsp_sensor.temperature / 10 ) >> 8 ) & 0xFF;
                a[sensor_data_cnt++]=(bsp_sensor.temperature / 10 ) & 0xFF;
                lpp_data[lpp_cnt].size = sensor_data_cnt - lpp_data[lpp_cnt].startcnt;
                lpp_cnt++;

                lpp_data[lpp_cnt].startcnt = sensor_data_cnt;
                a[sensor_data_cnt++] = 0x04;
				a[sensor_data_cnt++] = 0x02; //analog output
				a[sensor_data_cnt++] = (((int32_t)(bsp_sensor.resis / 10)) >> 8) & 0xFF;
				a[sensor_data_cnt++] = ((int32_t)(bsp_sensor.resis / 10 )) & 0xFF;
                lpp_data[lpp_cnt].size = sensor_data_cnt - lpp_data[lpp_cnt].startcnt;
                lpp_cnt++;
            }


	        if(sensor_data_cnt != 0)
            { 
                sample_status = true;                   
                sample_flag = true;
                RUI_LOG_PRINTF("\r\n");
                autosend_flag = true;
                user_lora_send();                               
            }	
            else 
            {                
                rui_lora_set_send_interval(RUI_AUTO_DISABLE,0);  //stop it auto send data if no sensor data.
            }
        }
    }else if(app_lora_status.autosend_status != RUI_AUTO_DISABLE) 
    {
        if(IsJoiningflag == false)
        {
            IsJoiningflag = true;
            if(app_lora_status.join_mode == RUI_OTAA)
            {
                rui_return_status = rui_lora_join();
                switch(rui_return_status)
                {
                    case RUI_STATUS_OK:RUI_LOG_PRINTF("OTAA Join Start...\r\n");break;
                    case RUI_LORA_STATUS_PARAMETER_INVALID:RUI_LOG_PRINTF("ERROR: RUI_AT_LORA_PARAMETER_INVALID %d\r\n",RUI_AT_LORA_PARAMETER_INVALID);
                        rui_lora_get_status(false,&app_lora_status);  //The query gets the current status 
                        switch(app_lora_status.autosend_status)
                        {
                            case RUI_AUTO_ENABLE_SLEEP:rui_lora_set_send_interval(RUI_AUTO_ENABLE_SLEEP,app_lora_status.lorasend_interval);  //start autosend_timer after send success
                                break;
                            case RUI_AUTO_ENABLE_NORMAL:rui_lora_set_send_interval(RUI_AUTO_ENABLE_NORMAL,app_lora_status.lorasend_interval);  //start autosend_timer after send success
                                break;
                            default:break;
                        } 
                        break;
                    default: RUI_LOG_PRINTF("ERROR: LORA_STATUS_ERROR %d\r\n",rui_return_status);
                        if(app_lora_status.lora_dr > 1)rui_lora_set_dr(app_lora_status.lora_dr-1);
                        rui_lora_get_status(false,&app_lora_status); 
                        switch(app_lora_status.autosend_status)
                        {
                            case RUI_AUTO_ENABLE_SLEEP:rui_lora_set_send_interval(RUI_AUTO_ENABLE_SLEEP,app_lora_status.lorasend_interval);  //start autosend_timer after send success
                                break;
                            case RUI_AUTO_ENABLE_NORMAL:rui_lora_set_send_interval(RUI_AUTO_ENABLE_NORMAL,app_lora_status.lorasend_interval);  //start autosend_timer after send success
                                break;
                            default:break;
                        }
                        break;
                }            
            }
        }
    }	
}

/*******************************************************************************************
 * LoRaMac callback functions
 * * void LoRaReceive_callback(RUI_RECEIVE_T* Receive_datapackage);//LoRaWAN callback if receive data 
 * * void LoRaP2PReceive_callback(RUI_LORAP2P_RECEIVE_T *Receive_P2Pdatapackage);//LoRaP2P callback if receive data 
 * * void LoRaWANJoined_callback(uint32_t status);//LoRaWAN callback after join server request
 * * void LoRaWANSendsucceed_callback(RUI_MCPS_T status);//LoRaWAN call back after send data complete
 * *****************************************************************************************/ 
void LoRaReceive_callback(RUI_RECEIVE_T* Receive_datapackage)
{
    char hex_str[3] = {0}; 
    uint32_t txperiod;
    uint16_t rui_txperiod;
    uint8_t sens_ch_mask;
    RUI_LOG_PRINTF("at+recv=%d,%d,%d,%d", Receive_datapackage->Port, Receive_datapackage->Rssi, Receive_datapackage->Snr, Receive_datapackage->BufferSize);   
    
    if ((Receive_datapackage->Buffer != NULL) && Receive_datapackage->BufferSize) {

        RUI_LOG_PRINTF(":");
        for (int i = 0; i < Receive_datapackage->BufferSize; i++) {
            sprintf(hex_str, "%02x", Receive_datapackage->Buffer[i]);
            RUI_LOG_PRINTF("%s", hex_str); 
        }
	// *********************************
	// LPP Device Period Configuration
	// *********************************
	if(Receive_datapackage->Port == LPP_DEVICE_PERIOD_CONF_PORT && Receive_datapackage->Buffer[0] == LPP_CONFIG_MASK_TXPERIOD && Receive_datapackage->BufferSize == LPP_TXPERIOD_SIZE + 1)
	{
		RUI_LOG_PRINTF("\r\n");
		txperiod = Receive_datapackage->Buffer[1] << 24 | Receive_datapackage->Buffer[2] << 16 | Receive_datapackage->Buffer[3] << 8 | Receive_datapackage->Buffer[4];

		if( txperiod < 0x00010000)
		{
			rui_txperiod = txperiod;

			rui_lora_get_status(false,&app_lora_status);  //The query gets the current status
    			switch(app_lora_status.autosend_status)
    			{
        			case RUI_AUTO_ENABLE_SLEEP:
				rui_return_status=rui_lora_set_send_interval(RUI_AUTO_ENABLE_SLEEP,rui_txperiod);  //start autosend_timer after send success
            			//rui_delay_ms(5);  
            			break;
        			case RUI_AUTO_ENABLE_NORMAL:
				rui_return_status=rui_lora_set_send_interval(RUI_AUTO_ENABLE_NORMAL,rui_txperiod);  //start autosend_timer after send success
            			break;
        			default:break;
    }
			//rui_return_status=rui_lora_set_send_interval(,rui_txperiod);
			switch(rui_return_status)
                	{
				case RUI_STATUS_OK:RUI_LOG_PRINTF("TX period configuration to %d seconds.",txperiod);break;
        			default: RUI_LOG_PRINTF("[LoRa]: autosend config error %d\r\n",rui_return_status);break;
			}
		}
		else
		{
			RUI_LOG_PRINTF("Error: TX period exceded 65535." );
		}												
	}
	// *********************************
	// LPP Enable/disable sensors 
	// *********************************
	if(Receive_datapackage->Port == LPP_SENSOR_ENABLE_PORT && Receive_datapackage->BufferSize == LPP_SENSOR_ENABLE_SIZE)
	{
		RUI_LOG_PRINTF("\r\n");		
		sens_ch_mask = Receive_datapackage->Buffer[0];
		RUI_LOG_PRINTF("Received channel mask:" );
		for (int i = 0; i < 8; i++) 
		{
			if( (sens_ch_mask >> (7-i)) & 0x01 )
				{RUI_LOG_PRINTF("1"); }
			else
				{RUI_LOG_PRINTF("0");}            		
        	}
		lpp_enable_sensor_mask = sens_ch_mask;

		if(lpp_enable_sensor_mask & SWITCH_2_LPP_EN_MASK)
		{			
			Switch_Two.pin_num = SWITCH_2;
    			Switch_Two.dir = RUI_GPIO_PIN_DIR_INPUT;
    			Switch_Two.pull = RUI_GPIO_PIN_NOPULL;
			lpp_sensor_mask_after_enabled = lpp_sensor_mask_after_enabled | SWITCH_2_LPP_EN_MASK;    			
    			rui_gpio_init(&Switch_Two);
			RUI_LOG_PRINTF("\r\nsw2 int enabled \r\n");
		}
		else
		{
			Switch_Two.pin_num = SWITCH_2;
    			Switch_Two.dir = RUI_GPIO_PIN_DIR_OUTPUT;
    			Switch_Two.pull = RUI_GPIO_PIN_NOPULL;
    			rui_gpio_init(&Switch_Two);
			rui_gpio_rw(RUI_IF_WRITE,&Switch_Two,low);
			RUI_LOG_PRINTF("\r\nsw2 int disabled \r\n");
		}
	}

    }
    RUI_LOG_PRINTF("\r\n");
}
void LoRaP2PReceive_callback(RUI_LORAP2P_RECEIVE_T *Receive_P2Pdatapackage)
{
    char hex_str[3]={0};
    RUI_LOG_PRINTF("at+recv=%d,%d,%d:", Receive_P2Pdatapackage -> Rssi, Receive_P2Pdatapackage -> Snr, Receive_P2Pdatapackage -> BufferSize); 
    for(int i=0;i < Receive_P2Pdatapackage -> BufferSize; i++)
    {
        sprintf(hex_str, "%02X", Receive_P2Pdatapackage -> Buffer[i]);
        RUI_LOG_PRINTF("%s",hex_str);
    }
    RUI_LOG_PRINTF("\r\n");    
}
void LoRaWANJoined_callback(uint32_t status)
{
    static int8_t dr; 
    if(status)  //Join Success
    {
        JoinCnt = 0;
        IsJoiningflag = false;
        RUI_LOG_PRINTF("OK Join Success\r\n");
        rui_gpio_rw(RUI_IF_WRITE,&Led_Red, high);
        rui_timer_start(&Join_Ok_Timer);        
    }else 
    {        
        if(JoinCnt<JOIN_MAX_CNT) // Join was not successful. Try to join again
        {
            JoinCnt++;
            RUI_LOG_PRINTF("[LoRa]:Join retry Cnt:%d\n",JoinCnt);
            rui_lora_join();                    
        }
        else   //Join failed
        {
            RUI_LOG_PRINTF("ERROR: %d\r\n",RUI_AT_LORA_INFO_STATUS_JOIN_FAIL); 
			rui_lora_get_status(false,&app_lora_status);  //The query gets the current status 
			rui_lora_set_send_interval(RUI_AUTO_ENABLE_SLEEP,app_lora_status.lorasend_interval);  //start autosend_timer after send success
            JoinCnt=0;   
        }          
    }    
}
void LoRaWANSendsucceed_callback(RUI_MCPS_T mcps_type,RUI_RETURN_STATUS status)
{
    if(sendfull == false)
    {
        autosend_flag = true;
        return;
    }

    if(status == RUI_STATUS_OK)
    {
        switch( mcps_type )
        {
            case RUI_MCPS_UNCONFIRMED:
            {
                RUI_LOG_PRINTF("OK \r\n");
                break;
            }
            case RUI_MCPS_CONFIRMED:
            {
                RUI_LOG_PRINTF("OK \r\n");
                break;
            }
            case RUI_MCPS_MULTICAST:
            {
                RUI_LOG_PRINTF("OK \r\n");
                break;           
            }
            case RUI_MCPS_PROPRIETARY:
            {
                RUI_LOG_PRINTF("OK \r\n");
                break;
            }
            default:             
                break;
        }
    
	if(lpp_enable_sensor_mask & LED_1_LPP_EN_MASK)
	{
		rui_gpio_rw(RUI_IF_WRITE,&Led_Red, high);	    	  
	}    	
	rui_timer_start(&LoRa_send_ok_Timer);

    }else if(status != RUI_AT_LORA_INFO_STATUS_ADDRESS_FAIL)RUI_LOG_PRINTF("ERROR: %d\r\n",status);   
	
    

}


void LoRaP2PSendsucceed_callback(void)
{
    RUI_LOG_PRINTF("OK \r\n");    
}

/*******************************************************************************************
 * The RUI is used to receive data from uart.
 * 
 * *****************************************************************************************/ 
void rui_uart_recv(RUI_UART_DEF uart_def, uint8_t *pdata, uint16_t len)
{
    switch(uart_def)
    {
        case RUI_UART1://process code if RUI_UART1 work at RUI_UART_UNVARNISHED
            rui_lora_get_status(false,&app_lora_status);
            if(app_lora_status.IsJoined)  //if LoRaWAN is joined
            {
                rui_lora_send(8,pdata,len);
            }else
            {
                RUI_LOG_PRINTF("ERROR: %d\r\n",RUI_AT_LORA_NO_NETWORK_JOINED);
            }
             
            break;
		case RUI_UART3://process code if RUI_UART3 received data ,the len is always 1
            /*****************************************************************************
             * user code 
            ******************************************************************************/
        //    RUI_LOG_PRINTF("%c",*pdata);
            break;
        default:break;
    }
}

/*******************************************************************************************
 * sleep and wakeup callback
 * 
 * *****************************************************************************************/
void bsp_sleep(void)
{
    /*****************************************************************************
             * user process code before enter sleep
    ******************************************************************************/
   extdigints=0x00;

} 
void bsp_wakeup(void)
{
    /*****************************************************************************
             * user process code after exit sleep
    ******************************************************************************/
    sendfull = true;  //clear subcontract send flag
    sample_status = false;  //clear sample flag
    bsp_i2c_init();
    //BME680_Init();
    rui_delay_ms(50);
}

/*******************************************************************************************
 * the app_main function
 * *****************************************************************************************/ 
void main(void)
{
    rui_uart_mode_config(RUI_UART3,RUI_UART_USER); //Forces UART3 to be configuGreen in RUI_UART_USER mode
    rui_init();
    bsp_init();
    
    /*******************************************************************************************
     * Register LoRaMac callback function
     * 
     * *****************************************************************************************/
    rui_lora_register_recv_callback(LoRaReceive_callback);  
    rui_lorap2p_register_recv_callback(LoRaP2PReceive_callback);
    rui_lorajoin_register_callback(LoRaWANJoined_callback); 
    rui_lorasend_complete_register_callback(LoRaWANSendsucceed_callback); 
    rui_lorap2p_complete_register_callback(LoRaP2PSendsucceed_callback);

    /*******************************************************************************************
     * Register Sleep and Wakeup callback function
     * 
     * *****************************************************************************************/
    rui_sensor_register_callback(bsp_wakeup,bsp_sleep);

    /*******************************************************************************************    
     *The query gets the current status 
    * 
    * *****************************************************************************************/ 
    rui_lora_get_status(false,&app_lora_status);	

    /*******************************************************************************************    
     *Init OK ,print board status and auto join LoRaWAN
    * 
    * *****************************************************************************************/  
    switch(app_lora_status.work_mode)
	{
		case RUI_LORAWAN:
            if(app_lora_status.autosend_status)RUI_LOG_PRINTF("autosend_interval: %us\r\n", app_lora_status.lorasend_interval);
            RUI_LOG_PRINTF("Current work_mode:LoRaWAN,");
            if(app_lora_status.join_mode == RUI_OTAA)
            {
                RUI_LOG_PRINTF(" join_mode:OTAA,");  
                if(app_lora_status.MulticastEnable)
                {
                    RUI_LOG_PRINTF(" MulticastEnable:true.\r\n");
                }else
                {
                    RUI_LOG_PRINTF(" MulticastEnable: false,");
                switch(app_lora_status.class_status)
                {
                    case RUI_CLASS_A:RUI_LOG_PRINTF(" Class: A\r\n");
                        break;
                    case RUI_CLASS_B:RUI_LOG_PRINTF(" Class: B\r\n");
                        break;
                    case RUI_CLASS_C:RUI_LOG_PRINTF(" Class: C\r\n");
                        break;
                    default:break;
                }              
                }            
            }else if(app_lora_status.join_mode == RUI_ABP)
            {
                RUI_LOG_PRINTF(" join_mode:ABP,");
                if(app_lora_status.MulticastEnable)
                {
                    RUI_LOG_PRINTF(" MulticastEnable:true.\r\n");
                }else
                {
                    RUI_LOG_PRINTF(" MulticastEnable: false,");
                }
                switch(app_lora_status.class_status)
                {
                    case RUI_CLASS_A:RUI_LOG_PRINTF(" Class: A\r\n");
                        break;
                    case RUI_CLASS_B:RUI_LOG_PRINTF(" Class: B\r\n");
                        break;
                    case RUI_CLASS_C:RUI_LOG_PRINTF(" Class: C\r\n");
                        break;
                    default:break;
                } 
                if(rui_lora_join() == RUI_STATUS_OK)//join LoRaWAN by ABP mode
                {
                    LoRaWANJoined_callback(1);  //ABP mode join success
                }
            }
			break;
		default: break;
	}   
    RUI_LOG_PRINTF("Initialization OK \r\n");

    while(1)
    {       
        rui_lora_get_status(false,&app_lora_status);//The query gets the current status 
        rui_running();
        switch(app_lora_status.work_mode)
        {
            case RUI_LORAWAN:
                if(!sample_status)app_loop();
                else user_lora_send();				
                break;
            case RUI_P2P:
                /*************************************************************************************
                 * user code at LoRa P2P mode
                *************************************************************************************/
                break;
            default :break;
        }
    }
}
