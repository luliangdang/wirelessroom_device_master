#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "lcd.h"
#include "key.h"
#include "malloc.h"
#include "sdio_sdcard.h"
#include "ff.h"
#include "exfuns.h"
#include "fontupd.h"
#include "text.h"
#include "piclib.h"
#include "touch.h"
#include "key_board.h"
#include "timer.h"
#include "esp8266.h"
#include "rtc.h"
#include "string.h"
#include "dht11.h"

u8 str[10] = {'0','1','2','3','4','5','6','7','8','9'};

extern u8 start_flag;
extern u8 dht11_flag;
extern u8 esp_flag;
extern u8 msg_flag;

//主函数
int main(void)
{
		u8 res;
		u8 key;
		u8 inputlen;		//输入长度
		u8 inputstr[7];		//最大输入6个字符+结束符
		char past[100];			//路由器信息
		char ipaddr[] = {"47.100.28.6"};
		char port[]= {"8086"};				//端口号
		
		start_flag = 0;
		dht11_flag = 0;
		esp_flag = 0;
		u8 temperature;	//温度
		u8 humidity; 		//湿度
		
		NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置系统中断优先级分组2
		delay_init(168);
		uart_init(115200);
		LED_Init();					//初始化LED  
		LCD_Init();					//LCD初始化  
		KEY_Init();					//按键初始化
		My_RTC_Init();			//RTC时钟初始化
//		RTC_Set_WakeUp(RTC_WakeUpClock_CK_SPRE_16bits,0);		//配置WAKE UP中断,1秒钟中断一次
		DHT11_Init();				//DHT11初始化
		
		tp_dev.init();			//触摸屏初始化
//		SD_Init();					//初始化SDIO接口
		my_mem_init(SRAMIN);	//初始化内部内存池
		my_mem_init(SRAMCCM);	//初始化CCM内存池
		exfuns_init();				//为fatfs相关变量申请内存  
		f_mount(fs[0],"0:",1); //挂载SD卡
		esp8266_Init();			//esp8266初始化
		memset(inputstr,0,7);	//全部清零
		inputlen=0;				//输入长度为0
//		piclib_init();

//		res=f_opendir(&picdir,(const TCHAR*)"0:/PICTURE"); 	//打开目录
//		if(res==FR_OK)
//		{
//			LCD_Clear(BLACK);
//			ai_load_picfile((u8 *)"0:/PICTURE/11407990822.JPG",0,0,120,160,1);
//			delay_ms(1000);
//		}
	
		key_load_ui(20,60);
		RTC_Get();
		TIM3_Int_Init(5000-1,8400-1);
		TIM4_Int_Init(5000-1,8400-1);
		USART2_RX_STA = 0;
		cleanReceiveData();		//清空接收数据缓存
		start_flag = 1;
		dht11_flag = 1;
		msg_flag = 0;
	
		while(1)
		{
				//读取屏幕键盘输入
				key = get_keynum(20,60);
				if(key)
				{
						TIM_Cmd(TIM3, DISABLE);  //使能TIMx
						TIM_SetCounter(TIM3, 0);
//						printf("key:%d\r\n",key);
						//回退
						if(key==4)
						{
							if(inputlen)inputlen--;
							inputstr[inputlen]='\0';//添加结束符
						}
						else if(key==13)
						{
							key_load_ui(20,60);
						}
						else if(key==15)
						{
							start_flag = 0;
							LED0 = 1;
							u2_printf("AT+CIPCLOSE\r\n");
							delay_ms(500);
							sprintf((char *)past,"AT+CIPSTART=\"TCP\",\"%s\",%s",ipaddr,port);
//							printf("%s\r\n",past);
							if(sendAT(past,"OK",2000)==0)
							{
									printf("服务器连接成功\r\n");
									LED0 = 0;
									connect_state = 0;
									sprintf(past,"@D1%04d",Device_ID);
									u2_printf("+++");
									delay_ms(25);
									u2_printf("AT+CIPMODE=1\r\n");
									delay_ms(15);
									u2_printf("AT+CIPSEND\r\n");
									delay_ms(15);
									u2_printf("%s\r\n",past);
									delay_ms(25);
									u2_printf("+++");
									delay_ms(15);
							}
							else
							{
								printf("服务器连接失败\r\n");
								LED0 = 1;
								connect_state = 1;
							}
							delay_ms(1000);
							cleanReceiveData();		//清空接收数据缓存
							start_flag = 1;
						}
						//确认
						else if(key==12&&inputlen==6)
						{
							char past[50];
							sprintf(past,"@D3%04d%s",Device_ID,inputstr);
							u2_printf("+++");
							delay_ms(25);
							u2_printf("AT+CIPMODE=1\r\n");
							delay_ms(15);
							u2_printf("AT+CIPSEND\r\n");
							delay_ms(15);
							u2_printf("%s\r\n",past);
							delay_ms(25);
							u2_printf("+++");
							delay_ms(15);
							inputlen = 0;
//							inputstr[inputlen]='\0';
							memset(inputstr,0,7);	//全部清零
						}
						//取消
						else if(key==8)
						{
							inputlen = 0;
//							inputstr[inputlen]='\0';
							memset(inputstr,0,7);	//全部清零
						}
						//输入密码
						else if((key%4)<4&&inputlen<6&&key!=14)
						{
							switch(key/4)
							{
								case 0:
									inputstr[inputlen]=str[key];//输入字符
								break;
								case 1:
									inputstr[inputlen]=str[3+key%4];//输入字符
								break;
								case 2:
									inputstr[inputlen]=str[6+key%4];//输入字符
								break;
								default:
									continue;
							}
							if(inputlen<6)
							{
								inputlen++;
								inputstr[inputlen]='\0';
							}
						}
						//输入0字符
						else if(key==14&&inputlen<6)
						{
							inputstr[inputlen] = '0';
							if(inputlen<6)
							{
								inputlen++;
								inputstr[inputlen]='\0';
							}
						}
						LCD_Fill(25,30,125,50,WHITE);
						Show_Str(25,30,100,16,inputstr,16,1);
						TIM_Cmd(TIM3, ENABLE);  //使能TIMx
				}
				//接收服务器信息
				if(USART2_RX_STA&&msg_flag==1)
				{
						USART2_RX_STA = 0;
						msg_flag = 0;
						LED1 = 0;
						TIM_Cmd(TIM3, DISABLE);  //使能TIMx
						TIM_SetCounter(TIM3, 0);
						printf("%s\r\n",USART2_RX_BUF);
						decodeData();
						cleanReceiveData();
						TIM_Cmd(TIM3, ENABLE);  //使能TIMx
				}
				else if(USART2_RX_STA)
				{
						USART2_RX_STA = 0;
						printf("%s",USART2_RX_BUF);
						sendBack(CMD_ONLINE,0);
				}
				//定时读取温湿度
				if(dht11_flag==1)
				{
						dht11_flag = 0;
						DHT11_Read_Data(&temperature,&humidity);
						LCD_ShowNum(270,90,temperature,2,16);
						LCD_ShowNum(270,110,humidity,2,16);
						if(temperature>50)
						{
								char past[50];
								sprintf(past,"@D3%04d%03d",Device_ID,temperature);
								u2_printf("+++");
								delay_ms(25);
								u2_printf("AT+CIPMODE=1\r\n");
								delay_ms(15);
								u2_printf("AT+CIPSEND\r\n");
								delay_ms(15);
								u2_printf("%s\r\n",past);
								delay_ms(25);
								u2_printf("+++");
								delay_ms(15);
						}
				}
				if(USART_RX_STA&0x8000)
				{
						USART_RX_STA = 0;
						printf("%s\r\n",USART_RX_BUF);
						if(my_strstr((char *)USART_RX_BUF,"check") !=0)
						{
								esp_flag = 1;
						}
				}
				//定时检查网络状态
				if(esp_flag==1)
				{
						esp_flag = 0;
						start_flag = 0;
						Check_Status();
						if(connect_state !=0)	esp8266_reinit(connect_state);
						start_flag = 1;
						cleanReceiveData();
				}
				delay_ms(10);
		}
		return 0;
}




