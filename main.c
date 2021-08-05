/**
  ******************************************************************************
  * @file    Project/main.c 
  * @author  MCD Application Team
  * @version V2.2.0
  * @date    30-September-2014
  * @brief   Main program body
   ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */ 


/* Includes ------------------------------------------------------------------*/
#include "stm8s.h"
#include "__RCSwitch.h"

#define RF 1
#define IR 0


uint32_t time_keeper=0, data=0, data_ir=0;

uint32_t data_ir_relay_1=1, data_ir_relay_2=1, data_ir_relay_3=1, data_ir_relay_4=1;
uint32_t data_rf_relay_1=1, data_rf_relay_2=1, data_rf_relay_3=1, data_rf_relay_4=1;

uint8_t mode=0;
uint8_t suscess_recevice=0; // rf
uint16_t count = 0;
uint32_t ir_code_data=0;
uint8_t switch_relay_1,switch_relay_2,switch_relay_3,switch_relay_4;

static void clk_config_16MHz_hsi(void)
{       

        CLK_DeInit();
	CLK_SYSCLKConfig(CLK_PRESCALER_CPUDIV1);
	CLK_SYSCLKConfig(CLK_PRESCALER_HSIDIV1);
	CLK_ClockSwitchConfig(CLK_SWITCHMODE_AUTO, CLK_SOURCE_HSI, DISABLE, CLK_CURRENTCLOCKSTATE_DISABLE);
}

void delay_using_timer4_init(void)
{
	TIM4_TimeBaseInit(TIM4_PRESCALER_128,124);//1ms if fMaster=16Mhz
	TIM4_ClearFlag(TIM4_FLAG_UPDATE);
	TIM4_ITConfig(TIM4_IT_UPDATE,ENABLE);

	enableInterrupts();
	TIM4_Cmd(DISABLE);
}

void delay_isr(void)
{
	if(TIM4_GetITStatus(TIM4_IT_UPDATE)==SET)
	{
		if(time_keeper!=0)
		{
			time_keeper--;
		}
		else
		{
			/* Disable Timer to reduce power consumption */
			TIM4->CR1 &= (uint8_t)(~TIM4_CR1_CEN);
		}
		TIM4_ClearITPendingBit(TIM4_IT_UPDATE);
	}
}


void delay_ms(uint32_t time)
{
	time_keeper=time;

	/* Reset Counter Register value */
	TIM4->CNTR = (uint8_t)(0);

	/* Enable Timer */
	TIM4->CR1 |= TIM4_CR1_CEN;

	while(time_keeper);
}

///////////////////////////////////////////////////////////////////////////////////////////////

static void TIM2_Config(void)
{
 	TIM2_TimeBaseInit(TIM2_PRESCALER_16,65000);//10uS if fMaster=16Mhz
	TIM2_ClearFlag(TIM2_FLAG_UPDATE);
	TIM2_ITConfig(TIM2_IT_UPDATE,DISABLE);
	//enableInterrupts();
	TIM2_Cmd(ENABLE);
}

void record_ir_signal()
{
  static uint32_t time_signal;
  static uint8_t mode_read=0, index=0;
  static uint32_t code=0;
  time_signal = TIM2_GetCounter();
  TIM2_SetCounter(0);
  
  if(((time_signal>12000)&&(time_signal<20000))&&(mode_read==0))
  {
    mode_read=1; // day la bit Start, bat dau hoc lenh
    
  }
  else if(mode_read==1)
  {
    index++;
    
    if((time_signal>1500)&&(time_signal<3000)) // bit 1
    {
      code |= (uint32_t)1<< (32 - index);
    }
    else if (time_signal < 1500) 
    {
      //code *= (uint32_t)0<< (32 - index);
    }
    else if ((time_signal>30000)||(index>31))
    {
      // ket thuc
      suscess_recevice =1;
      ir_code_data = code;
      index = 0;
      time_signal=0;
      code = 0;
      mode_read=0;
      
    }
  }
  else 
  {
    mode_read =0;
  }

}
uint32_t get_data_ir()
{ 
//  static uint32_t data_read, old_data_read;
//  if(suscess_recevice == 1)
//  {
//    old_data_read = data_read;
//    data_read = ir_code_data;
//    suscess_recevice = 0;
//    return data_read;
//  }
//   else return old_data_read;
  return ir_code_data;
}

void reset_data_ir()
{
  ir_code_data = 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////
void check_mode()
{
  
  if(GPIO_ReadInputPin(GPIOA, GPIO_PIN_1)==0x00)
  {
    mode = IR;
  }
  else 
  {
    mode = RF;
  }
}

void init_detec_sign()
{
  if(mode == IR)
  {
    GPIO_Init(GPIOD, GPIO_PIN_6, GPIO_MODE_IN_FL_IT);    // SIGNAL IR
    EXTI_SetExtIntSensitivity(EXTI_PORT_GPIOD, EXTI_SENSITIVITY_FALL_ONLY); // INIT FOR IR SIGN
  }
  else 
  {
    GPIO_Init(GPIOD, GPIO_PIN_5, GPIO_MODE_IN_FL_IT);    // SIGNAL RF315
    EXTI_SetExtIntSensitivity(EXTI_PORT_GPIOD, EXTI_SENSITIVITY_RISE_FALL);  // INIT FOR RF SIGN
  }
}

///////////// //////////////////////////////////////////////////////////////
// READ WRITE FLASH

void write_flash_data_ir(uint8_t index, uint32_t data)
{
     FLASH_DeInit();
     FLASH_SetProgrammingTime(FLASH_PROGRAMTIME_STANDARD);
     FLASH_Unlock(FLASH_MEMTYPE_DATA);
     for(int i=0; i<4; i++)
     {
     FLASH_EraseByte(0x4000+index*4+i);
     }
     FLASH_ProgramWord(0x4000+index*4,data);
     FLASH_Lock(FLASH_MEMTYPE_DATA);
}
void write_flash_data_rf(uint8_t index, uint32_t data)
{ 
     FLASH_DeInit();
     FLASH_SetProgrammingTime(FLASH_PROGRAMTIME_STANDARD);
     FLASH_Unlock(FLASH_MEMTYPE_DATA);
     for(int i=0; i<4; i++)
     {
     FLASH_EraseByte(0x4040+index*4+i);
     }
     FLASH_ProgramWord(0x4040+index*4,data);
     FLASH_Lock(FLASH_MEMTYPE_DATA);
  
}

uint32_t read_flash_data_ir(uint8_t index)
{
     uint8_t data[4];
     uint32_t data32;
     for(int i=0; i<4; i++)
     {
       data[i] = FLASH_ReadByte(0x4000+index*4+i);
     }
     data32 = data[0];
     data32 = (data32 <<8 ) + data[1];
     data32 = (data32 <<8 ) + data[2];
     data32 = (data32 <<8 ) + data[3];
     return data32;
}
uint32_t read_flash_data_rf(uint8_t index)
{
     uint8_t data[4];
     uint32_t data32;

     for(int i=0; i<4; i++)
     {
       data[i] = FLASH_ReadByte(0x4040+index*4+i);
     }
     data32 = data[0];
     data32 = (data32 <<8 ) + data[1];
     data32 = (data32 <<8 ) + data[2];
     data32 = (data32 <<8 ) + data[3];
     return data32;
}
//////////////////////////////////////////////////////////////////////////////////////


/*
doc code data, dieu khien data, xoa data

*/


// CONTROL RELAY TOGGLE
void relay1_toggle()
{
  GPIO_WriteReverse(GPIOD, GPIO_PIN_2);
}
void relay2_toggle()
{
  GPIO_WriteReverse(GPIOD, GPIO_PIN_3);
}
void relay3_toggle()
{
  GPIO_WriteReverse(GPIOD, GPIO_PIN_4);
}
void relay4_toggle()
{
  GPIO_WriteReverse(GPIOA, GPIO_PIN_2);
}
/////////////////////////////////////////////////////////////////

// CONTROL RELAY PUSH
void relay_push()
{
  
}

//////////////////////////////////////////////////////////////////////
void scan_button_relay()
{
  if(GPIO_ReadInputPin(GPIOC, GPIO_PIN_4)==0)
  {
    
    GPIO_WriteHigh(GPIOD, GPIO_PIN_2);
    if(mode == RF)
    {
    data = 0;
    while (__IsCmd_RFavailable())
           {
             // count = 0;
             data = __getReceivedValue();
             __resetAvailable();
            } 
    if((data != data_rf_relay_1)&& (data != 0))
    {
      data_rf_relay_1 = data;
      write_flash_data_rf(1, data_rf_relay_1);
      while(GPIO_ReadInputPin(GPIOC, GPIO_PIN_4)==0)
      {
         GPIO_WriteReverse(GPIOD, GPIO_PIN_2);
         delay_ms(400);
      }
    }
    }
    else
    {
      data_ir_relay_1 = 0;
      
      suscess_recevice = 0;
      reset_data_ir();
      while(data_ir_relay_1 == 0)
      {
        data_ir_relay_1 = get_data_ir();
      }
      write_flash_data_ir(1, data_ir_relay_1);
      while(GPIO_ReadInputPin(GPIOC, GPIO_PIN_4)==0)
      {
         GPIO_WriteReverse(GPIOD, GPIO_PIN_2);
         delay_ms(400);
      }
    }
  }
  
 /////////////////////////////////////////////////////////
  if(GPIO_ReadInputPin(GPIOC, GPIO_PIN_5)==0)
  {
    
    GPIO_WriteHigh(GPIOD, GPIO_PIN_3);
    if(mode == RF)
    {
    data = 0;
    while (__IsCmd_RFavailable())
           {
             // count = 0;
             data = __getReceivedValue();
             __resetAvailable();
            } 
    if((data != data_rf_relay_2)&& (data != 0))
    {
      data_rf_relay_2 = data;
      write_flash_data_rf(2, data_rf_relay_2);
      while(GPIO_ReadInputPin(GPIOC, GPIO_PIN_5)==0)
      {
         GPIO_WriteReverse(GPIOD, GPIO_PIN_3);
         delay_ms(400);
      }
    }
    }
    else
    {
      data_ir_relay_2 = 0;
      
      suscess_recevice = 0;
      reset_data_ir();
      while(data_ir_relay_2 == 0)
      {
        data_ir_relay_2 = get_data_ir();
      }
      write_flash_data_ir(2, data_ir_relay_2);
      while(GPIO_ReadInputPin(GPIOC, GPIO_PIN_5)==0)
      {
         GPIO_WriteReverse(GPIOD, GPIO_PIN_3);
         delay_ms(400);
      }
    }
  }
  
 ///////////////////////////////////////////////////////
   if(GPIO_ReadInputPin(GPIOC, GPIO_PIN_6)==0)
  {
    
    GPIO_WriteHigh(GPIOD, GPIO_PIN_4);
    if(mode == RF)
    {
    data = 0;
    while (__IsCmd_RFavailable())
           {
             // count = 0;
             data = __getReceivedValue();
             __resetAvailable();
            } 
    if((data != data_rf_relay_3)&& (data != 0))
    {
      data_rf_relay_3 = data;
      write_flash_data_rf(3, data_rf_relay_3);
      while(GPIO_ReadInputPin(GPIOC, GPIO_PIN_6)==0)
      {
         GPIO_WriteReverse(GPIOD, GPIO_PIN_4);
         delay_ms(400);
      }
    }
    }
    else
    {
      data_ir_relay_3 = 0;
      
      suscess_recevice = 0;
      reset_data_ir();
      while(data_ir_relay_3 == 0)
      {
        data_ir_relay_3 = get_data_ir();
      }
      write_flash_data_ir(3, data_ir_relay_3);
      while(GPIO_ReadInputPin(GPIOC, GPIO_PIN_6)==0)
      {
         GPIO_WriteReverse(GPIOD, GPIO_PIN_4);
         delay_ms(400);
      }
    }
  }
 ////////////////////////////////////////////////////////////
  
  if(GPIO_ReadInputPin(GPIOC, GPIO_PIN_7)==0)
  {
    
    GPIO_WriteHigh(GPIOA, GPIO_PIN_2);
    if(mode == RF)
    {
    data = 0;
    while (__IsCmd_RFavailable())
           {
             // count = 0;
             data = __getReceivedValue();
             __resetAvailable();
            } 
    if((data != data_rf_relay_4)&& (data != 0))
    {
      data_rf_relay_4 = data;
      write_flash_data_rf(4, data_rf_relay_4);
      while(GPIO_ReadInputPin(GPIOC, GPIO_PIN_7)==0)
      {
         GPIO_WriteReverse(GPIOA, GPIO_PIN_2);
         delay_ms(400);
      }
    }
    }
    else
    {
      data_ir_relay_4 = 0;
      
      suscess_recevice = 0;
      reset_data_ir();
      while(data_ir_relay_4 == 0)
      {
        data_ir_relay_4 = get_data_ir();
      }
      write_flash_data_ir(4, data_ir_relay_4);
      while(GPIO_ReadInputPin(GPIOC, GPIO_PIN_7)==0)
      {
         GPIO_WriteReverse(GPIOA, GPIO_PIN_2);
         delay_ms(400);
      }
    }
  }
}


void scan_switch_relay_mode()
{
  if(GPIO_ReadInputPin(GPIOC, GPIO_PIN_3)==0) switch_relay_4 = 1;
  else switch_relay_4 = 0;
  
  if(GPIO_ReadInputPin(GPIOB, GPIO_PIN_4)==0) switch_relay_3 = 1;
  else switch_relay_3 = 0;
  
  if(GPIO_ReadInputPin(GPIOB, GPIO_PIN_5)==0) switch_relay_2 = 1;
  else switch_relay_2 = 0;
  
  if(GPIO_ReadInputPin(GPIOA, GPIO_PIN_3)==0) switch_relay_1 = 1;
  else switch_relay_1 = 0;
}

void control_relay_for_rf_mode()
{
  if(switch_relay_1 == 1)
  {
    if(data == data_rf_relay_1)
   {
     GPIO_WriteHigh(GPIOD, GPIO_PIN_2); 
   }
  else 
   {
     GPIO_WriteLow(GPIOD, GPIO_PIN_2);
   }
  }
  else
  {
    if(data == data_rf_relay_1)
            {
             GPIO_WriteReverse(GPIOD, GPIO_PIN_2); 
             delay_ms(200);
             data = 0;
             __resetAvailable();
            }
  }
///////////////////////////////////////////////////////  
  if(switch_relay_2 == 1)
  {
    if(data == data_rf_relay_2)
   {
     GPIO_WriteHigh(GPIOD, GPIO_PIN_3); 
   }
  else 
   {
     GPIO_WriteLow(GPIOD, GPIO_PIN_3);
   }
  }
  else
  {
    if(data == data_rf_relay_2)
            {
             GPIO_WriteReverse(GPIOD, GPIO_PIN_3); 
             delay_ms(200);
             data = 0;
             __resetAvailable();
            }
  }
  //////////////////////////////////////
  if(switch_relay_3 == 1)
  {
    if(data == data_rf_relay_3)
   {
     GPIO_WriteHigh(GPIOD, GPIO_PIN_4); 
   }
  else 
   {
     GPIO_WriteLow(GPIOD, GPIO_PIN_4);
   }
  }
  else
  {
    if(data == data_rf_relay_3)
            {
             GPIO_WriteReverse(GPIOD, GPIO_PIN_4); 
             delay_ms(200);
             data = 0;
             __resetAvailable();
            }
  }
  /////////////////////////////////////////////////
  if(switch_relay_4 == 1)
  {
    if(data == data_rf_relay_4)
   {
     GPIO_WriteHigh(GPIOA, GPIO_PIN_2); 
   }
  else 
   {
     GPIO_WriteLow(GPIOA, GPIO_PIN_2);
   }
  }
  else
  {
    if(data == data_rf_relay_4)
            {
             GPIO_WriteReverse(GPIOA, GPIO_PIN_2); 
             delay_ms(200);
             data = 0;
             __resetAvailable();
            }
  }
}

void control_relay_for_ir_mode()
{
    if(data_ir == data_ir_relay_1)
    {
      GPIO_WriteReverse(GPIOD, GPIO_PIN_2); 
      delay_ms(200);
      data_ir = 0;
      reset_data_ir();
    }
    else if(data_ir == data_ir_relay_2)
    {
      GPIO_WriteReverse(GPIOD, GPIO_PIN_3);
      delay_ms(200);
      data_ir = 0;
      reset_data_ir();
    }
    else if(data_ir == data_ir_relay_3)
    {
      GPIO_WriteReverse(GPIOD, GPIO_PIN_4); 
      delay_ms(200);
      data_ir = 0;
      reset_data_ir();
    }
    else if(data_ir == data_ir_relay_4)
    {
      GPIO_WriteReverse(GPIOA, GPIO_PIN_2);
      delay_ms(200);
      data_ir = 0;
      reset_data_ir();
    }
    
}


void main(void)
{
  clk_config_16MHz_hsi();
  //GPIO_Init(GPIOD, GPIO_PIN_5, GPIO_MODE_IN_FL_IT);    // SIGNAL RF315
  //GPIO_Init(GPIOD, GPIO_PIN_6, GPIO_MODE_IN_FL_IT);    // SIGNAL IR
  GPIO_Init(GPIOA, GPIO_PIN_1, GPIO_MODE_IN_PU_NO_IT); // SELECT 
  GPIO_Init(GPIOC, GPIO_PIN_4, GPIO_MODE_IN_PU_NO_IT); //BUTTON RELAY1
  GPIO_Init(GPIOC, GPIO_PIN_5, GPIO_MODE_IN_PU_NO_IT); //BUTTON RELAY2
  GPIO_Init(GPIOC, GPIO_PIN_6, GPIO_MODE_IN_PU_NO_IT); //BUTTON RELAY3
  GPIO_Init(GPIOC, GPIO_PIN_7, GPIO_MODE_IN_PU_NO_IT); //BUTTON RELAY4
  GPIO_Init(GPIOC, GPIO_PIN_3, GPIO_MODE_IN_PU_NO_IT); //MODE RELAY1
  GPIO_Init(GPIOB, GPIO_PIN_4, GPIO_MODE_IN_PU_NO_IT); //MODE RELAY2
  GPIO_Init(GPIOB, GPIO_PIN_5, GPIO_MODE_IN_PU_NO_IT); //MODE RELAY3
  GPIO_Init(GPIOA, GPIO_PIN_3, GPIO_MODE_IN_PU_NO_IT); //MODE RELAY4
  GPIO_Init(GPIOD, GPIO_PIN_2, GPIO_MODE_OUT_PP_LOW_FAST); //RELAY1
  GPIO_Init(GPIOD, GPIO_PIN_3, GPIO_MODE_OUT_PP_LOW_FAST); //RELAY2
  GPIO_Init(GPIOD, GPIO_PIN_4, GPIO_MODE_OUT_PP_LOW_FAST); //RELAY3
  GPIO_Init(GPIOA, GPIO_PIN_2, GPIO_MODE_OUT_PP_LOW_FAST); //RELAY4
  check_mode();
  init_detec_sign();
  TIM2_Config();
  ITC_SetSoftwarePriority(ITC_IRQ_TIM4_OVF,ITC_PRIORITYLEVEL_1);
  ITC_SetSoftwarePriority(ITC_IRQ_PORTD,ITC_PRIORITYLEVEL_3);
  delay_using_timer4_init();
  if(mode == RF)
  {
    __RCSwitchInit();
  __enableReceive1();
  }
  enableInterrupts();
  data_rf_relay_1 = read_flash_data_rf(1);
  data_rf_relay_2 = read_flash_data_rf(2);
  data_rf_relay_3 = read_flash_data_rf(3);
  data_rf_relay_4 = read_flash_data_rf(4);
  data_ir_relay_1 = read_flash_data_ir(1);
  data_ir_relay_2 = read_flash_data_ir(2);
  data_ir_relay_3 = read_flash_data_ir(3);
  data_ir_relay_4 = read_flash_data_ir(4);
  
  while (1)
  {
    if(mode == IR)
    {
      data_ir = get_data_ir();
      control_relay_for_ir_mode();
    }
    else
    {
      if(count > 10000) 
        {
         data =0;
         count = 0;
        }
      while (__IsCmd_RFavailable())
           {
             count = 0;
             data = __getReceivedValue();
             __resetAvailable();
            }
      count++;
      control_relay_for_rf_mode();
      scan_switch_relay_mode();
    }
    scan_button_relay();
  }
  
}

#ifdef USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
  * @param file: pointer to the source file name
  * @param line: assert_param error line source number
  * @retval : None
  */
void assert_failed(u8* file, u32 line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
