#include "__RCSwitch.h"
//#include "hw.h"
//#include "tempo.h"
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <intrinsics.h>

extern uint8_t suscess_recevice;

//Tempo t;        //Timers and Delay

Protocol_t __protocol;


int __nReceiverInterrupt;
int __nTransmitterPin;
int __nRepeatTransmit;

unsigned long __nReceivedValue = 0;
unsigned int __nReceivedBitlength = 0;
unsigned int __nReceivedDelay = 0;
unsigned int __nReceivedProtocol = 0;
int __nReceiveTolerance = 60;
unsigned int __nSeparationLimit = 4600;
unsigned int __timings[__RCSWITCH_MAX_CHANGES];
	
/* Format for protocol definitions:
 * {pulselength, Sync bit, "0" bit, "1" bit}
 * 
 * pulselength: pulse length in microseconds, e.g. 350
 * Sync bit: {1, 31} means 1 high pulse and 31 low pulses
 *     (perceived as a 31*pulselength long pulse, total length of sync bit is
 *     32*pulselength microseconds), i.e:
 *      _
 *     | |_______________________________ (don't count the vertical bars)
 * "0" bit: waveform for a data bit of value "0", {1, 3} means 1 high pulse
 *     and 3 low pulses, total length (1+3)*pulselength, i.e:
 *      _
 *     | |___
 * "1" bit: waveform for a data bit of value "1", e.g. {3,1}:
 *      ___
 *     |   |_
 *
 * These are combined to form Tri-State bits when sending or receiving codes.
 */
Protocol_t __proto[] = {
  { 430, {  1, 31 }, {  1,  3 }, {  3,  1 }, false },    // protocol 1
  { 650, {  1, 10 }, {  1,  2 }, {  2,  1 }, false },    // protocol 2
  { 100, { 30, 71 }, {  4, 11 }, {  9,  6 }, false },    // protocol 3
  { 380, {  1,  6 }, {  1,  3 }, {  3,  1 }, false },    // protocol 4
  { 500, {  6, 14 }, {  1,  2 }, {  2,  1 }, false },    // protocol 5
  { 450, { 23,  1 }, {  1,  2 }, {  2,  1 }, true }      // protocol 6 (HT6P20B)
};

enum {
   __numProto = sizeof(__proto) / sizeof(__proto[0])
};

void __RCSwitchInit(void) {
	__nTransmitterPin = -1;
	__setRepeatTransmit(10);
	__setProtocol(1);
	__nReceiverInterrupt = -1;
	__setReceiveTolerance(60);
	__nReceivedValue = 0;
    
    __enableReceive(0);
}

void __setProtocol(int nProtocol) {
  
	if (nProtocol < 1 || nProtocol > __numProto) 
	{
		nProtocol = 1;  // TODO: trigger an error, e.g. "bad protocol" ???
	}
	memcpy(&__protocol, &__proto[nProtocol-1], sizeof(Protocol_t));
}

void __setPulseLength(int nPulseLength) {
	__protocol.pulseLength = nPulseLength;
}

void __setRepeatTransmit(int nRepeatTransmit) {
	__nRepeatTransmit = nRepeatTransmit;
}

void __setReceiveTolerance(int nPercent) {
	__nReceiveTolerance = nPercent;
}

char* __getCodeWordA(const char* sGroup, const char* sDevice, bool bStatus) {
  static char sReturn[13];
  int nReturnPos = 0;

  for (int i = 0; i < 5; i++) {
    sReturn[nReturnPos++] = (sGroup[i] == '0') ? 'F' : '0';
  }

  for (int i = 0; i < 5; i++) {
    sReturn[nReturnPos++] = (sDevice[i] == '0') ? 'F' : '0';
  }

  sReturn[nReturnPos++] = bStatus ? '0' : 'F';
  sReturn[nReturnPos++] = bStatus ? 'F' : '0';

  sReturn[nReturnPos] = '\0';
  return sReturn;
}

/**
 * Encoding for type B switches with two rotary/sliding switches.
 *
 * The code word is a tristate word and with following bit pattern:
 *
 * +-----------------------------+-----------------------------+----------+------------+
 * | 4 bits address              | 4 bits address              | 3 bits   | 1 bit      |
 * | switch group                | switch number               | not used | on / off   |
 * | 1=0FFF 2=F0FF 3=FF0F 4=FFF0 | 1=0FFF 2=F0FF 3=FF0F 4=FFF0 | FFF      | on=F off=0 |
 * +-----------------------------+-----------------------------+----------+------------+
 *
 * @param nAddressCode  Number of the switch group (1..4)
 * @param nChannelCode  Number of the switch itself (1..4)
 * @param bStatus       Whether to switch on (true) or off (false)
 *
 * @return char[13], representing a tristate code word of length 12
 */
char* __getCodeWordB(int nAddressCode, int nChannelCode, bool bStatus) {
  static char sReturn[13];
  int nReturnPos = 0;

  if (nAddressCode < 1 || nAddressCode > 4 || nChannelCode < 1 || nChannelCode > 4) {
    return 0;
  }

  for (int i = 1; i <= 4; i++) {
    sReturn[nReturnPos++] = (nAddressCode == i) ? '0' : 'F';
  }

  for (int i = 1; i <= 4; i++) {
    sReturn[nReturnPos++] = (nChannelCode == i) ? '0' : 'F';
  }

  sReturn[nReturnPos++] = 'F';
  sReturn[nReturnPos++] = 'F';
  sReturn[nReturnPos++] = 'F';

  sReturn[nReturnPos++] = bStatus ? 'F' : '0';

  sReturn[nReturnPos] = '\0';
  return sReturn;
}

/**
 * Like getCodeWord (Type C = Intertechno)
 */
char* __getCodeWordC(char sFamily, int nGroup, int nDevice, bool bStatus) {
  static char sReturn[13];
  int nReturnPos = 0;

  int nFamily = (int)sFamily - 'a';
  if ( nFamily < 0 || nFamily > 15 || nGroup < 1 || nGroup > 4 || nDevice < 1 || nDevice > 4) {
    return 0;
  }
  
  // encode the family into four bits
  sReturn[nReturnPos++] = (nFamily & 1) ? 'F' : '0';
  sReturn[nReturnPos++] = (nFamily & 2) ? 'F' : '0';
  sReturn[nReturnPos++] = (nFamily & 4) ? 'F' : '0';
  sReturn[nReturnPos++] = (nFamily & 8) ? 'F' : '0';

  // encode the device and group
  sReturn[nReturnPos++] = ((nDevice-1) & 1) ? 'F' : '0';
  sReturn[nReturnPos++] = ((nDevice-1) & 2) ? 'F' : '0';
  sReturn[nReturnPos++] = ((nGroup-1) & 1) ? 'F' : '0';
  sReturn[nReturnPos++] = ((nGroup-1) & 2) ? 'F' : '0';

  // encode the status code
  sReturn[nReturnPos++] = '0';
  sReturn[nReturnPos++] = 'F';
  sReturn[nReturnPos++] = 'F';
  sReturn[nReturnPos++] = bStatus ? 'F' : '0';

  sReturn[nReturnPos] = '\0';
  return sReturn;
}

/**
 * Encoding for the REV Switch Type
 *
 * The code word is a tristate word and with following bit pattern:
 *
 * +-----------------------------+-------------------+----------+--------------+
 * | 4 bits address              | 3 bits address    | 3 bits   | 2 bits       |
 * | switch group                | device number     | not used | on / off     |
 * | A=1FFF B=F1FF C=FF1F D=FFF1 | 1=0FF 2=F0F 3=FF0 | 000      | on=10 off=01 |
 * +-----------------------------+-------------------+----------+--------------+
 *
 * Source: http://www.the-intruder.net/funksteckdosen-von-rev-uber-arduino-ansteuern/
 *
 * @param sGroup        Name of the switch group (A..D, resp. a..d) 
 * @param nDevice       Number of the switch itself (1..3)
 * @param bStatus       Whether to switch on (true) or off (false)
 *
 * @return char[13], representing a tristate code word of length 12
 */
char* __getCodeWordD(char sGroup, int nDevice, bool bStatus) {
  static char sReturn[13];
  int nReturnPos = 0;

  // sGroup must be one of the letters in "abcdABCD"
  int nGroup = (sGroup >= 'a') ? (int)sGroup - 'a' : (int)sGroup - 'A';
  if ( nGroup < 0 || nGroup > 3 || nDevice < 1 || nDevice > 3) {
    return 0;
  }

  for (int i = 0; i < 4; i++) {
    sReturn[nReturnPos++] = (nGroup == i) ? '1' : 'F';
  }

  for (int i = 1; i <= 3; i++) {
    sReturn[nReturnPos++] = (nDevice == i) ? '1' : 'F';
  }

  sReturn[nReturnPos++] = '0';
  sReturn[nReturnPos++] = '0';
  sReturn[nReturnPos++] = '0';

  sReturn[nReturnPos++] = bStatus ? '1' : '0';
  sReturn[nReturnPos++] = bStatus ? '0' : '1';

  sReturn[nReturnPos] = '\0';
  return sReturn;
}

void __enableReceive(int interrupt) {
	__nReceiverInterrupt = interrupt;
	__enableReceive1();
}

void __enableReceive1() {
    
    if (__nReceiverInterrupt != -1) 
    {
        __nReceivedValue = 0;
        __nReceivedBitlength = 0;
  }
}

bool __IsCmd_RFavailable() {
  return __nReceivedValue != 0;
}

void __resetAvailable() {
  __nReceivedValue = 0;
}

unsigned long __getReceivedValue() {
  return __nReceivedValue;
}

unsigned int __getReceivedBitlength() {
  return __nReceivedBitlength;
}

unsigned int __getReceivedDelay() {
  return __nReceivedDelay;
}

unsigned int __getReceivedProtocol() {
  return __nReceivedProtocol;
}

unsigned int* __getReceivedRawdata() {
  return __timings;
}

/* helper function for the receiveProtocol method */
unsigned int __diff(int A, int B) {
  return abs(A - B);
}

bool __receiveProtocol(const int p, unsigned int changeCount) {

  
   
    Protocol_t pro;
    memcpy(&pro, &__proto[p-1], sizeof(Protocol_t));


    unsigned long code = 0;
    //Assuming the longer pulse length is the pulse captured in timings[0]
    const unsigned int syncLengthInPulses =  ((pro.syncFactor.low) > (pro.syncFactor.high)) ? (pro.syncFactor.low) : (pro.syncFactor.high);
    const unsigned int delay = __timings[0] / syncLengthInPulses;
    const unsigned int delayTolerance = delay * __nReceiveTolerance / 100;
    
    /* For protocols that start low, the sync period looks like
     *               _________
     * _____________|         |XXXXXXXXXXXX|
     *
     * |--1st dur--|-2nd dur-|-Start data-|
     *
     * The 3rd saved duration starts the data.
     *
     * For protocols that start high, the sync period looks like
     *
     *  ______________
     * |              |____________|XXXXXXXXXXXXX|
     *
     * |-filtered out-|--1st dur--|--Start data--|
     *
     * The 2nd saved duration starts the data
     */
    const unsigned int firstDataTiming = (pro.invertedSignal) ? (2) : (1);

    for (unsigned int i = firstDataTiming; i < changeCount - 1; i += 2) 
    {
        
        code <<= 1;
        if (__diff(__timings[i], delay * pro.zero.high) < delayTolerance && __diff(__timings[i + 1], delay * pro.zero.low) < delayTolerance) 
        {
          
            // zero
        } 
        else if (__diff(__timings[i], delay * pro.one.high) < delayTolerance && __diff(__timings[i + 1], delay * pro.one.low) < delayTolerance) 
        {
            // one
            code |= 1;
        } 
        else {
            // Failed
            return false;
        }
    }

    if (changeCount > 7) // ignore very short transmissions: no device sends them, so this must be noise
    {
        __nReceivedValue     = code;
        __nReceivedBitlength = (changeCount - 1) / 2;
        __nReceivedDelay     = delay;
        __nReceivedProtocol  = p;
    }

    return true;
}

void __handleInterrupt() {
   
    //GPIO_WriteHigh(GPIOC, GPIO_PIN_7);
    static unsigned int changeCount = 0;
    static unsigned long lastTime = 0;
    static unsigned int repeatCount = 0;
    
    static unsigned int duration;
    
    duration = TIM2_GetCounter();
    TIM2_SetCounter(0);
    //const long time = TIM2_GetCounter();
    //const unsigned int duration = time - lastTime;

    if (duration > __nSeparationLimit) 
    {
        // A long stretch without signal level change occurred. This could
        // be the gap between two transmission.
        if (__diff(duration, __timings[0]) < 200) 
        {
            // This long signal is close in length to the long signal which
            // started the previously recorded timings; this suggests that
            // it may indeed by a a gap between two transmissions (we assume
            // here that a sender will send the signal multiple times,
            // with roughly the same gap between them).
           
            repeatCount++;
            if (repeatCount == 2) 
            {
              
                for(unsigned int i = 1; i <= __numProto; i++) 
                {
                    
                    if (__receiveProtocol(i, changeCount)) 
                    {
                        //GPIOD->ODR ^= (1<<3);
                        // receive succeeded for protocol i
                        suscess_recevice =1;
                        break;
                    }
                }
                repeatCount = 0;
            }
        }
        changeCount = 0;
    }
 
    // detect overflow
    if (changeCount >= __RCSWITCH_MAX_CHANGES) 
    {
        changeCount = 0;
        repeatCount = 0;
    }

    __timings[changeCount++] = duration;
    //GPIO_WriteLow(GPIOC, GPIO_PIN_7);
    //lastTime = time;  
    //TIM2_SetCounter(0);
}

