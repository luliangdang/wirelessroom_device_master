#ifndef __ESP_H
#define __ESP_H
#include "sys.h"

#define Device_ID	0x0001			//本机机器号

#define USART2_MAX_RECV_LEN		600					//最大接收缓存字节数
#define USART2_MAX_SEND_LEN		600					//最大发送缓存字节数
#define USART2_MAX_RECV_LEN		600					//最大接收缓存字节数
#define USART2_MAX_SEND_LEN		600					//最大发送缓存字节数
#define USART2_RX_EN 					1						//0,不接收;1,接收.

//服务器传至设备
#define CMD_ONLINE        0x01      //检查联机
#define CMD_SET_TIME			0x02			//设置下位机时间
#define CMD_OPEN					0x03			//开门信号
#define CMD_CONNECT				0x04			//设置连接WiFi

//时间结构体
typedef struct
{
    u8 year[4];
    u8 month[2];
    u8 date[2];
    u8 hour[2];
    u8 minute[2];
    u8 second[2];
}TimeTypeDef;
//时间数据联合体
typedef union
{
    TimeTypeDef TIME;
    u8 time_arrary[12];
}Union_Time;

extern u8  USART2_RX_BUF[USART2_MAX_RECV_LEN]; 		//接收缓冲,最大USART3_MAX_RECV_LEN字节
extern u8  USART2_TX_BUF[USART2_MAX_SEND_LEN]; 		//发送缓冲,最大USART3_MAX_SEND_LEN字节
extern vu16 USART2_RX_STA;   											//接收数据状态
extern u8 connect_state;
extern u8 start_flag;

void usart2_Init(u32 bound);	//串口2初始化
void u2_printf(char* fmt,...);		//串口2发送数据函数
void esp8266_Init(void);			//esp8266初始化

extern s8 sendAT(char *sendStr,char *searchStr,u32 outTime);//发送AT指令函数
extern void cleanReceiveData(void);    //清除接收器数据
extern char * my_strstr(char *FirstAddr,char *searchStr);	//strstr函数
s8 TCP_Server(void);		//配置服务器
void decodeData(void);	//解析服务器信息
void sendBack(u8 CMD_TYPE,ErrorStatus error);	//发送回复应答

void ClearnSDCache(void);		//清除SD缓存
void SendOnline(void);			//发送在线应答
void SetTime(void);					//设置时间
void OpenDoor(void);				//开门指令
void SetNet(void);					//设置连接WiFi

void switch_CMD(void);			//指令调用

#endif




