/*									Include Head File														*/
#include "esp8266.h"
#include "stdarg.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "my_math.h"
#include "usart.h"
#include "lcd.h"
#include "delay.h"
#include "ff.h"
#include "rtc.h"
#include "malloc.h"
#include "led.h"
#include "text.h"

FIL fdsts_recive;
UINT readnum;
u8 connect_state;		//服务器连接状态：0：已连接，1：断开
u8 start_flag = 0;

//串口接收缓存区
u8 USART2_RX_BUF[USART2_MAX_RECV_LEN]; 			//接收缓冲,最大USART2_MAX_RECV_LEN个字节.
u8 USART2_TX_BUF[USART2_MAX_SEND_LEN]; 			//发送缓冲,最大USART2_MAX_SEND_LEN字节

//通过判断接收连续2个字符之间的时间差不大于10ms来决定是不是一次连续的数据.
//如果2个字符接收间隔超过10ms,则认为不是1次连续数据.也就是超过10ms没有接收到
//任何数据,则表示此次接收完毕.
//接收到的数据状态
//[15]:0,没有接收到数据;1,接收到了一批数据.
//[14:0]:接收到的数据长度
vu16 USART2_RX_STA=0;

void usart2_Init(u32 bound)
{
    NVIC_InitTypeDef NVIC_InitStructure;
		GPIO_InitTypeDef GPIO_InitStructure;
		USART_InitTypeDef USART_InitStructure;
		
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE); //使能GPIOB时钟
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE);//使能USART3时钟

    USART_DeInit(USART2);  //复位串口2
	
		GPIO_PinAFConfig(GPIOA,GPIO_PinSource2,GPIO_AF_USART2); //GPIOB11复用为USART3
		GPIO_PinAFConfig(GPIOA,GPIO_PinSource3,GPIO_AF_USART2); //GPIOB10复用为USART3	
		
    //USART2_TX   PA2
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2; //PB10
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用功能
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	//速度50MHz
		GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; //推挽复用输出
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; //上拉
		GPIO_Init(GPIOA,&GPIO_InitStructure); //初始化GPIOB11，和GPIOB10

    //USART2_RX	  PA3
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用功能
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	//速度50MHz
		GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; //推挽复用输出
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; //上拉
		GPIO_Init(GPIOA,&GPIO_InitStructure); //初始化GPIOB11，和GPIOB10

    USART_InitStructure.USART_BaudRate = bound;//波特率一般设置为9600;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;//字长为8位数据格式
    USART_InitStructure.USART_StopBits = USART_StopBits_1;//一个停止位
    USART_InitStructure.USART_Parity = USART_Parity_No;//无奇偶校验位
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//无硬件数据流控制
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	//收发模式

    USART_Init(USART2, &USART_InitStructure); //初始化串口	2

    USART_Cmd(USART2, ENABLE);                    //使能串口

    //使能接收中断
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);//开启中断

    //设置中断优先级
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=0;//抢占优先级0
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;		//子优先级2
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
    NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器

    USART2_RX_STA=0;		//清零

}

//串口2,printf 函数
//确保一次发送数据不超过USART2_MAX_SEND_LEN字节
void u2_printf(char* fmt,...)
{
    u16 i,j;
    va_list ap;
    va_start(ap,fmt);
    vsprintf((char*)USART2_TX_BUF,fmt,ap);
    va_end(ap);
    i=strlen((const char*)USART2_TX_BUF);		//此次发送数据的长度
    for(j=0; j<i; j++)							//循环发送数据
    {
        while(USART_GetFlagStatus(USART2,USART_FLAG_TC)==RESET); //循环发送,直到发送完毕
        USART_SendData(USART2,USART2_TX_BUF[j]);
    }
}

u8 msg_flag;
//串口2中断服务函数
void USART2_IRQHandler(void)
{
	u8 res;
	if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)//接收到数据
	{
		res =USART_ReceiveData(USART2);
		if((USART2_RX_STA&(1<<15))==0)//接收完的一批数据,还没有被处理,则不再接收其他数据
		{
			if(start_flag==0)
			{
				if(USART2_RX_STA<USART2_MAX_RECV_LEN)
				{
					USART2_RX_BUF[USART2_RX_STA++] = res;
				}
			}
			else if(start_flag==1)
			{
				if(res=='@')
				{
					USART2_RX_STA&=0x0000;
					msg_flag = 1;
				}
				if(USART2_RX_STA<USART2_MAX_RECV_LEN)
				{
					USART2_RX_BUF[USART2_RX_STA++] = res;
//					USART2_RX_STA++;
				}
			}
		}
	}
}

//发送一条AT指令，并检测是否收到指定的应答
//*sndStr:发送的命令字符串,当sendStr<0XFF的时候,发送数字(比如发送0X1A),大于的时候发送字符串.
//*searchStr:期待的应答结果,如果为空,则表示不需要等待应答
//outTime:等待时间(单位:1ms)
//返回值:0,发送成功(得到了期待的应答结果)
//       -1,发送失败
s8 sendAT(char *sendStr,char *searchStr,u32 outTime)
{
    u16 i;
    s8 ret = 0;
    char * res =0;
//	outTime=outTime/10;
    cleanReceiveData();//清除接收器
    if((u32)sendStr < 0xFF)
    {
        while((USART2->SR&0X40)==0);//等待上一次数据发送完成
        USART2->DR=(u32)sendStr;
    }
    else
    {
        u2_printf(sendStr);//发送AT指令
        u2_printf("\r\n");//发送回车换行
    }
		delay_us(500);
    if(searchStr && outTime)//当searchStr和outTime不为0时才等待应答
    {
        while((--outTime)&&(res == 0))//等待指定的应答或超时时间到来
        {
            res = my_strstr((char *)USART2_RX_BUF,searchStr);
            if(res!=0)
                break;
            if((i==USART2_MAX_RECV_LEN)||res!=0)		//i何用？？？
                break;
            delay_ms(1);
        }
        if(outTime == 0)
        {
            ret = -1;    //超时
        }
        if(res != 0)//res不为0证明收到指定应答
        {
            ret = 0;
        }
    }
		printf("%s",USART2_RX_BUF);
    delay_ms(50);
		cleanReceiveData();		//清空接收数据缓存
    return ret;
}

//清除接收器
//参数：无
//返回值 无
void cleanReceiveData(void)
{
    u16 i;
    USART2_RX_STA=0;			//接收计数器清零
    for(i = 0; i < USART2_MAX_RECV_LEN; i++)
    {
        USART2_RX_BUF[i] = 0;
    }
}

//在串口接收的字符串中搜索
//返回值		成功返回被查找字符串的首地址
//				失败返回 0
//有一定的BUG，在接收数据中存在0，将会停止查找
char * my_strstr(char *FirstAddr,char *searchStr)
{

    char * ret = 0;

//    if((u3_data_Pack.USART1_RX_STA&(1<<15))!=0)	//接收完成
//    {
//    if((u3_data_Pack.USART3_RX_STA|(0x8FFF)) >= USART3_MAX_RECV_LEN)
//    {
//        u3_data_Pack.USART3_RX_BUF[USART3_MAX_RECV_LEN-1] = '\0';
//    }
    ret = strstr((char *)FirstAddr,searchStr);
    if(ret != 0)
    {
        return ret;
    }
//    }

    return 0;
}

//检测网络状态
void Check_Status(void)
{
		u8 state;
		printf("开始检测网络状态\r\n");
		if(sendAT("AT","OK",1000)==0)
		{
				u2_printf("AT+CIPSTATUS\r\n");
				delay_ms(25);
				printf("%s",USART2_RX_BUF);
				printf("%c",USART2_RX_BUF[7]);
				state = USART2_RX_BUF[7] - '0';
				printf("网络状态：%d\r\n",state);
				switch(state)
				{
						case 2:	
							printf("已连接路由器，未连接服务器\r\n");
							connect_state = 1;
							LED0 = 1;
						break;
						case 3:
							printf("已连接服务器\r\n");
							connect_state = 0;
						break;
						case 4:
							printf("与服务器断开连接\r\n");
							connect_state = 1;
							LED0 = 1;
						break;
						case 5:
							printf("与路由器断开连接\r\n");
							connect_state = 2;
							LED0 = 1;
						break;
						default:
							printf("未知故障\r\n");
							connect_state = 3;
							LED0 = 1;
						break;
				}
		}
		else 
		{
				printf("ESP8266模块异常\r\n");
				state = 4;
		}
		printf("网络状态检测完毕\r\n");
		cleanReceiveData();
}


char SSID[] = {"Kiven"};		//路由器SSID
char password[] = {"asd123456"};	//路由器密码
//char ipaddr[]= {"111.231.90.29"};//IP地址
char ipaddr[] = {"47.100.28.6"};
//char ipaddr[]= {"172.20.10.3"};//IP地址
char port[]= {"8086"};				//端口号

void esp8266_reinit(u8 state)
{
		char past[50];
		switch(state)
		{
				case 4:
					sendAT("AT+RST","ready",3000);
				case 3:
					sendAT("AT+RST","ready",3000);
					sendAT("AT","OK",1000);
				case 2:
					if(sendAT("AT+CWMODE=1","OK",1000)==0)
					{
						printf("设置为STA模式\r\n");
					}
					sprintf(past,"AT+CWJAP_DEF=\"%s\",\"%s\"",SSID,password);
					sendAT(past,"OK",2000);
				case 1:
					sprintf((char *)past,"AT+CIPSTART=\"TCP\",\"%s\",%s",ipaddr,port);
					
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
				case 0:
					printf("服务器重连成功\r\n");
				break;
		}
		cleanReceiveData();
}

//ESP8266模块初始化
void esp8266_Init(void)
{
//    u8 i;
//    s8 ret;
    char past[50];			//路由器信息
    usart2_Init(115200);	//初始化串口2波特率为9600
    cleanReceiveData();		//清空接收数据缓存
    sendAT("AT+RST","ready",3000);
		LED0 = 1;
    delay_ms(1000);
		LED0 = 0;
		printf("开始初始化\r\n");
    delay_ms(1000);		//等待模块上电稳定
    
    printf("初始化成功\r\n");
		LCD_ShowString(20,20,200,16,16,"ESP8266 init success!");
		LED0 = 1;
		delay_ms(1000);		//等待模块上电稳定
		sendAT("ATE0","OK",1000);
		if(sendAT("ATE0","OK",1000)==0)
		{
			printf("关闭回显成功\r\n");
		}
		if(sendAT("AT+CWMODE=1","OK",1000)==0)
		{
			printf("设置为STA模式\r\n");
		}
		
		LCD_ShowString(20,40,200,16,16,(u8 *)"ESP8266 set at STA mode");
		
		printf("连接路由器成功\r\n");
		LCD_ShowString(20,60,200,16,16,(u8 *)"Connected router Succese");

		sprintf((char *)past,"AT+CIPSTART=\"TCP\",\"%s\",%s",ipaddr,port);

		if(sendAT(past,"OK",2000)==0)
		{
				printf("服务器连接成功\r\n");
				LED0 = 0;
				LCD_ShowString(20,80,200,16,16,(u8 *)"Server connected");
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
			LCD_ShowString(20,80,200,16,16,(u8 *)"Server disconnected");
			connect_state = 1;
		}
		delay_ms(1000);
		cleanReceiveData();		//清空接收数据缓存
		LCD_Clear(WHITE);
}

/*********************************************************************
*功    能：解析接收数据
*入口参数：
*出口参数：
*********************************************************************/
void decodeData(void)
{
		u8 i,j;
		int strlen = -1;	//数据长度
		u8 strbuf[100];		//数据缓存
		for(i=0;USART2_RX_BUF[i]!='\0';i++)
		{
				//找到数据头
				if(USART2_RX_BUF[i]=='@'&&strlen<0)
				{
						strlen = i;
//						strlen = 0;
//						for(;strlen<USART2_RX_STA;strlen++)
//						{
//								str[strlen]=USART2_RX_BUF[strlen+i];
//						}
//						printf();
				}
//				if(strlen>=0)
//				{
//					strbuf[strlen++] = USART2_RX_BUF[strlen+i];
//				}
		}
		printf("strlen=%d\r\n",strlen);
		for(j=0;j<50;j++)
		{
				strbuf[j] = USART2_RX_BUF[j+strlen];
		}
		printf("j=%d\r\n",j);
		printf("strbuf:%s\r\n",strbuf);
		if(j!=0 && strlen>=0)
		{
				if(f_open(&fdsts_recive,"Receive.txt",FA_WRITE|FA_CREATE_ALWAYS)==FR_OK)
				{
						f_lseek(&fdsts_recive,0);                         				 //移动文件指针
						f_write(&fdsts_recive,strbuf,j,&readnum);		 //命令
						f_close(&fdsts_recive);
						printf("成功写入数据\r\n");
				}
				switch_CMD();
		}
		else
		{
				LED1 = 1;
				delay_ms(50);
				LED1 = 0;
				delay_ms(50);
				LED1 = 1;
		}
		cleanReceiveData();		//清空接收数据缓存
}

/*********************************************************************
*功    能：发送回复应答
*入口参数：
*出口参数：
*********************************************************************/
void sendBack(u8 CMD_TYPE,u8 error)
{
		char past[50];
		char date[20];
		memset(past,0,50);
		memset(date,0,20);
		sprintf(past,"@D%1d%04d%1d",CMD_TYPE,Device_ID,error);
//		sprintf(date,"%4d-%02d-%02d %02d:%02d:%02d",calendar.w_year,
//																								calendar.w_month,
//																								calendar.w_date,
//																								calendar.hour,
//																								calendar.min,
//																								calendar.sec);
		u2_printf("AT+CIPMODE=1\r\n");
		delay_ms(15);
		u2_printf("AT+CIPSEND\r\n");
		delay_ms(15);
		printf("data:%s",past);
		u2_printf("%s\r\n",past);
		delay_ms(25);
		u2_printf("+++");
		delay_ms(15);
}

/*********************************************************************
*功    能：清空SD卡缓存信息
*入口参数：
*出口参数：
*********************************************************************/
void ClearnSDCache(void)
{
    if(f_open(&fdsts_recive,"Receive.txt",FA_WRITE)==FR_OK)
    {
        f_lseek(&fdsts_recive,0);           //移动文件指针
        f_truncate(&fdsts_recive);			//截断文件
        f_close(&fdsts_recive);
    }
}

/*********************************************************************
*功    能：发送在线应答
*入口参数：
*出口参数：
*********************************************************************/
void SendOnline(void)
{
		u8 device_id[6];
		u8 device = 0;
		memset(device_id,0,6);
		if(f_open(&fdsts_recive,"Receive.txt",FA_READ)==FR_OK)
    {
        f_lseek(&fdsts_recive,3);                          			//移动文件指针
        f_read(&fdsts_recive,device_id,4,&readnum);	//读取设定时间
        f_close(&fdsts_recive);
    }
//		printf("device_id:%s\r\n",device_id);
		for(u8 i=0;i<4;i++)
		{
				device_id[i] = device_id[i]-48;
				device = device*10+device_id[i];
		}
		printf("%04d",device);
		if(device==Device_ID)
		{
				LED1 = 1;
				ClearnSDCache();         /*清空SD卡缓存信息*/
				sendBack(CMD_ONLINE,1);
		}
		else
		{
				LED1 = 1;
				delay_ms(50);
				LED1 = 0;
				delay_ms(50);
				LED1 = 1;
		}
}

/*********************************************************************
*功    能：设置时间
*入口参数：
*出口参数：
*时间：2015年9月18日 22:12:15
*********************************************************************/
void SetTime(void)
{
		u16 year;
		u8 CMD;
		u8 mon,day,hour,min,sec,week;
		ErrorStatus error;
		u8 device_id[6];
		u8 device = 0;
		Union_Time Gettime;
		memset(Gettime.time_arrary,0,14);
		memset(device_id,0,6);	//清零
		//读取设备号
		if(f_open(&fdsts_recive,"Receive.txt",FA_READ)==FR_OK)
    {
        f_lseek(&fdsts_recive,3);                   //移动文件指针
        f_read(&fdsts_recive,device_id,4,&readnum);	//读取设定时间
        f_close(&fdsts_recive);
    }
		//读取控制命令
		if(f_open(&fdsts_recive,"Receive.txt",FA_READ)==FR_OK)
    {
        f_lseek(&fdsts_recive,21);              //移动文件指针
        f_read(&fdsts_recive,&CMD,1,&readnum);	//读取设定时间
        f_close(&fdsts_recive);
    }
		CMD -= '0';
//		printf("device_id:%s\r\n",device_id);
		for(u8 i=0;i<4;i++)
		{
				device_id[i] = device_id[i]-48;
				device = device*10+device_id[i];
		}
		printf("%04d",device);
		if(device==Device_ID)
		{
				if(f_open(&fdsts_recive,"Receive.txt",FA_READ)==FR_OK)
				{
						f_lseek(&fdsts_recive,7);                          		//移动文件指针
						f_read(&fdsts_recive,Gettime.time_arrary,14,&readnum);	//读取设定时间
						f_close(&fdsts_recive);
				}
				printf("%s\r\n",Gettime.time_arrary);
				for(u8 i=0;i<14;i++)
				{
						Gettime.time_arrary[i] -= 48;
				}
				
				year=(Gettime.TIME.year[0])*1000;     			//计算时间
				year+=(Gettime.TIME.year[1])*100;
				year+=(Gettime.TIME.year[2])*10;
				year+=(Gettime.TIME.year[3]);

				mon=(Gettime.TIME.month[0])*10;
				mon+=(Gettime.TIME.month[1]);
			
				day=(Gettime.TIME.date[0])*10;
				day+=(Gettime.TIME.date[1]);
				
				week = RTC_Get_Week(year,mon,day);
			
				hour=(Gettime.TIME.hour[0])*10;
				hour+=(Gettime.TIME.hour[1]);
			
				min=(Gettime.TIME.minute[0])*10;
				min+=(Gettime.TIME.minute[1]);
			
				sec=(Gettime.TIME.second[0])*10;
				sec+=(Gettime.TIME.second[1]);
				
				if(year>2018&&mon<13&&day<32&&hour<24&&min<60&&sec<60)
				{
						switch(CMD)
						{
							case 1:
							{
									error = RTC_Set_Time(hour,min,sec,RTC_H12_AM);	//设置时间
									printf("set-error:%d\r\n",error);
									error = RTC_Set_Date(year-2000,mon,day,week);		//设置日期
									printf("set-error:%d\r\n",error);
									
									RTC_Get();
									LED1 = 1;
									sendBack(CMD_SET_TIME,error);		//发送时间设置应答
									break;
							}
							case 2:
							{
									printf("设置闹钟A\r\n");
									RTC_Set_AlarmA(week,hour,min,sec);
									sendBack(CMD_SET_TIME,error);		//发送时间设置应答
									break;
							}
						}
				}
		}
		else
		{
				LED1 = 1;
				delay_ms(50);
				LED1 = 0;
				delay_ms(50);
				LED1 = 1;
				sendBack(CMD_SET_TIME,0);		//发送时间设置应答
		}
		ClearnSDCache();            								//清空SD卡缓存信息
}

/*********************************************************************
*功    能：开门指令
*入口参数：
*出口参数：
*********************************************************************/
void OpenDoor(void)
{
		u8 device_id[6];
		u8 userid[12];
		u32 device = 0;
		u8 CMD=0;
		memset(device_id,0,6);
		memset(userid,0,12);
		if(f_open(&fdsts_recive,"Receive.txt",FA_READ)==FR_OK)
		{
				f_lseek(&fdsts_recive,3);                         //移动文件指针
				f_read(&fdsts_recive,&device_id,4,&readnum);						//读取指令
				f_close(&fdsts_recive);
		}
//		printf("%s\r\n",device_id);
		for(u8 i=0;i<4;i++)
		{
				device_id[i] = device_id[i]-48;
				device = device*10+device_id[i];
		}
		printf("deviceid:%04d\r\n",device);
		if(device==Device_ID)
		{
				printf("地址匹配成功\r\n");
				if(f_open(&fdsts_recive,"Receive.txt",FA_READ)==FR_OK)
				{
						f_lseek(&fdsts_recive,18);                         //移动文件指针
						f_read(&fdsts_recive,&CMD,1,&readnum);						//读取指令
						f_close(&fdsts_recive);
				}
				CMD -= '0';
				TIM_SetCounter(TIM3, 0);
				LCD_LED = 1;
				printf("CMD:%d\r\n",CMD);
				switch(CMD)
				{
						case 0:		//正常开门
						{
								if(f_open(&fdsts_recive,"Receive.txt",FA_READ)==FR_OK)
								{
										f_lseek(&fdsts_recive,7);                         //移动文件指针
										f_read(&fdsts_recive,&userid,11,&readnum);						//读取指令
										f_close(&fdsts_recive);
								}
								DS0 = 1;
								DS1 = 1;
								DR0 = 0;
								TIM_Cmd(TIM4, ENABLE);
								printf("userid:%s\r\n",userid);
								printf("正常开门\r\n");
								LCD_Fill(230,150,320,180,WHITE);
								Show_Str(230,150,100,16,(u8 *)"欢迎光临！",16,0);
								sendBack(CMD_OPEN,0);
								break;
						}
						case 1:		//错误代码1--未注册
						{
								printf("尚未注册\r\n");
								LCD_Fill(230,150,320,180,WHITE);
								Show_Str(230,150,100,16,(u8 *)"尚未注册！",16,0);
								sendBack(CMD_OPEN,1);
								break;
						}
						case 2:		//错误代码2--未预约
						{
								printf("尚未预约\r\n");
								LCD_Fill(230,150,320,180,WHITE);
								Show_Str(230,150,100,16,(u8 *)"尚未预约！",16,0);
								sendBack(CMD_OPEN,2);
								break;
						}
						case 3:		//错误代码3--未到预约时间
						{
								if(f_open(&fdsts_recive,"Receive.txt",FA_READ)==FR_OK)
								{
										f_lseek(&fdsts_recive,7);                         //移动文件指针
										f_read(&fdsts_recive,&userid,11,&readnum);						//读取指令
										f_close(&fdsts_recive);
								}
								printf("userid:%s\r\n",userid);
								printf("未到预约时间\r\n");
								LCD_Fill(230,150,320,180,WHITE);
								Show_Str(230,150,100,16,(u8 *)"未到预约时间！",16,0);
								sendBack(CMD_OPEN,3);
								break;
						}
						case 4:		//错误代码4--验证码过期
						{
								if(f_open(&fdsts_recive,"Receive.txt",FA_READ)==FR_OK)
								{
										f_lseek(&fdsts_recive,7);                         //移动文件指针
										f_read(&fdsts_recive,&userid,11,&readnum);						//读取指令
										f_close(&fdsts_recive);
								}
								printf("userid:%s\r\n",userid);
								printf("预约已过期\r\n");
								LCD_Fill(230,150,320,180,WHITE);
								Show_Str(230,150,100,16,(u8 *)"预约已失效！",16,0);
								sendBack(CMD_OPEN,4);
								break;
						}
						default:break;
				}
				LED1 = 1;
		}
		else
		{
				LED1 = 1;
				delay_ms(50);
				LED1 = 0;
				delay_ms(50);
				LED1 = 1;
				sendBack(CMD_OPEN,5);
		}
}

/*********************************************************************
*功    能：设置连接WiFi
*入口参数：
*出口参数：
*********************************************************************/
void SetNet(void)
{
		u8 device_id[6];
		u32 device = 0;
		memset(device_id,0,4);
		if(f_open(&fdsts_recive,"Receive.txt",FA_READ)==FR_OK)
		{	 	
				f_lseek(&fdsts_recive,3);                         //移动文件指针
				f_read(&fdsts_recive,&device_id,4,&readnum);						//读取指令
				f_close(&fdsts_recive);
		}
		printf("%s\r\n",device_id);
		for(u8 i=0;i<4;i++)
		{
				device_id[i] = device_id[i]-48;
				device = device*10+device_id[i];
		}
		printf("deviceid:%4d\r\n",device);
		if(device == Device_ID)
		{
				LED0 = 1;
				sendBack(CMD_CONNECT,1);
		}
		else
		{
				LED1 = 1;
				delay_ms(50);
				LED1 = 0;
				delay_ms(50);
				LED1 = 1;
				sendBack(CMD_CONNECT,0);
		}
}

/*********************************************************************
*功    能：指令选择
*入口参数：
*出口参数：
*********************************************************************/
void switch_CMD(void)
{
    u8 CMD=0;
    if(f_open(&fdsts_recive,"Receive.txt",FA_READ)==FR_OK)
    {
        f_lseek(&fdsts_recive,2);                         //移动文件指针
        f_read(&fdsts_recive,&CMD,1,&readnum);						//读取指令
        f_close(&fdsts_recive);
    }
		CMD -= '0';
//	CMD=CMD_DELETE_PHOTO;
    switch(CMD)			//指令选择
    {
				case CMD_ONLINE:		{	SendOnline();	break;	}	//发送在线应答
				case CMD_SET_TIME:	{	SetTime();		break;	}	//设置时间
				case CMD_OPEN:			{	OpenDoor();		break;	}	//开门指令
				case CMD_CONNECT:		{ SetNet();			break;	}	//切换连接WiFi
//        case CMD_ADD_USER: 			{   adduser();  		 			break;	} //添加用户
//        case CMD_DELETE_USER: 	{   deleteuse(); 		 			break; 	}	//删除指定用户
//        case CMD_ONLINE: 				{   Sendonline(); 		 		break; 	}	//发送在线联机应答
//        case CMD_GET_USER_LIST: {   Uploaduserlist(); 	 	break; 	}	//获取下位机用户列表
//        case CMD_GET_ALL_LIST:  {   Uploadalluserlist();	break;	}	//获取下位机用户全部信息
//        case CMD_GET_USER_NUM:	{	 	Uploadusernum();	 		break;	}	//获取用户总数
//        case CMD_SET_TIME :			{   settime();			 			break; 	}	//设置时间
//        case CMD_GTE_I_O : 			{   Upload_access();	 		break; 	}	//上传个人进出门信息
//				case CMD_SAVE_PHOTO:		{	 	Save_photo();		 			break;	}	//存储照片
//				case CMD_DELETE_PHOTO:	{	 	deletephoto();		 		break;	}	//删除照片
//				case CMD_MEMBER:				{	 	new_member();		 			break;	}	//新建人员信息
//				case CMD_CHECK_PHOTO:		{	 	check_photo();		 		break;	}	//查寻图片是否存在

        default:
				{
						LED1 = 1; 
						delay_ms(50);
						LED1 = 0; 
						delay_ms(50);	
						LED1 = 1; 
						delay_ms(50);
						sendBack(CMD_ONLINE,1);
						break;
				}
    }
		ClearnSDCache();            								//清空SD卡缓存信息
}


