#ifndef	__ESP8266_H
#define	__ESP8266_H

#ifdef USE_STDPERIPH_DRIVER

#include "stdint.h"
#include "string.h"
#undef USE_HAL_DRIVER  // ����HAL
/*MQTT ���Ӳ���*/
//#define clientId "a1Zw81YmcId.ESP8266|securemode=2\\,signmethod=hmacsha256\\,timestamp=1694524012615|"
//#define username "ESP8266&a1Zw81YmcId"
//#define passwd "a8bed5d35c6c689437c6ef2fa8d360044879460f6e6647fb4b8f985180ad42ca"
//#define mqttHostUrl "a1Zw81YmcId.iot-as-mqtt.cn-shanghai.aliyuncs.com"
//#define port "1883"
//�ͻ�
#define post "/k16e93sVlLN/esp01s/user/esp01s"
#define post_name "LightSwitch"

//#define MQTT_set	"AT+MQTTUSERCFG=0,1,\"NULL\",\"esp8266&k16e92KZo4M\",\"47276e0d24efac0b551315af837c0431f5590515c7144bef7330bcac6bc037e9\",0,0,\"\""
//#define MQTT_Client "AT+MQTTCLIENTID=0,\"k16e92KZo4M.esp8266|securemode=2\\,signmethod=hmacsha256\\,timestamp=1714541771601|\""
//#define MQTT_Pass "AT+MQTTCONN=0,\"iot-06z00dazkl34gjg.mqtt.iothub.aliyuncs.com\",1883,1"

#define MQTT_set	"AT+MQTTUSERCFG=0,1,\"NULL\",\"esp01s&k16e93sVlLN\",\"4ac4f12ef4b322f3212b281e7addd35477f3a2ebf27fccf80dac6678201d4b2c\",0,0,\"\""
#define MQTT_Client "AT+MQTTCLIENTID=0,\"k16e93sVlLN.esp01s|securemode=2\\,signmethod=hmacsha256\\,timestamp=1744624949805|\""
#define MQTT_Pass "AT+MQTTCONN=0,\"iot-06z00dazkl34gjg.mqtt.iothub.aliyuncs.com\",1883,1"

#define WIFI_Name "5132"
#define WIFI_Pass "53288837"
extern unsigned char Property_Data[];		//���ݱ�������
void ESP8266_Init(void);
unsigned char ESP8266_Re(unsigned char *ACK_AT);
void ESP8266_Send(char *property,int Data);
void ESP8266_Received(char *PRO);
uint8_t esp8266_send_cmd(char *cmd,char *ack,uint16_t waittime);
uint8_t esp8266_check_cmd(char *str);
//void delay_ms_01s(uint16_t ms);

#endif
#endif
