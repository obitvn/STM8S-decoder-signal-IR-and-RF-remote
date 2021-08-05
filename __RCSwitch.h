#ifndef __RC_SWITCH_H__
#define __RC_SWITCH_H__

#include <stdbool.h>
#include "stm8s.h"
//#include <iostm8s003k3.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
//#include <intrinsics.h>
//#include <inttypes.h>

// Number of maximum High/Low changes per packet.
// We can handle up to (unsigned long) => 32 bit * 2 H/L changes per bit + 2 for sync
#define __RCSWITCH_MAX_CHANGES 67

typedef struct{
	uint8_t high;
	uint8_t low;
}HighLow_t;

typedef struct{
	int pulseLength;
	HighLow_t syncFactor;
	HighLow_t zero;
	HighLow_t one;
	bool invertedSignal;
}Protocol_t;





extern void __RCSwitchInit(void);
extern void __setProtocol(int nProtocol);
extern void __setPulseLength(int nPulseLength);
extern void __setRepeatTransmit(int nRepeatTransmit);
extern void __setReceiveTolerance(int nPercent);
extern char* __getCodeWordA(const char* sGroup, const char* sDevice, bool bStatus);
extern char* __getCodeWordB(int nAddressCode, int nChannelCode, bool bStatus);
extern char* __getCodeWordC(char sFamily, int nGroup, int nDevice, bool bStatus);
extern char* __getCodeWordD(char sGroup, int nDevice, bool bStatus);
extern void __enableReceive(int interrupt);
extern void __enableReceive1();
extern bool __IsCmd_RFavailable();
extern void __resetAvailable();
extern unsigned long __getReceivedValue();
extern unsigned int __getReceivedBitlength();
extern unsigned int __getReceivedDelay();
extern unsigned int __getReceivedProtocol();
extern unsigned int* __getReceivedRawdata();
extern unsigned int __diff(int A, int B);
extern bool __receiveProtocol(const int p, unsigned int changeCount);
extern void __handleInterrupt();

    

#endif
