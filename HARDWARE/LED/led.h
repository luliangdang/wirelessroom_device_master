#ifndef __LED_H
#define __LED_H
#include "sys.h"

//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32F407开发板
//LED驱动代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2014/5/2
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2014-2024
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	


//LED端口定义
/***************************************
*LED0闪烁一次表示初始化成功
*LED0亮起表示连接服务器成功
*LED1亮起接收数据
*LED1亮起后熄灭表示正常处理数据
*LED1闪烁两次表示接收数据有误
***************************************/
#define LED0 PFout(9)		// LED0		
#define LED1 PFout(10)	// LED1

#define DS0	 PFout(4)		//Light0
#define	DS1  PFout(5)		//Light1
#define DR0  PFout(6)		//Door

void LED_Init(void);//初始化

#endif
