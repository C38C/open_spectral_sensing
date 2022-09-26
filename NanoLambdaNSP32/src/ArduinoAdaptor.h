/*
 * Copyright (C) 2019 nanoLambda, Inc.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __NANOLAMBDA_NSP32_ARDUINO_ADAPTOR_H
#define __NANOLAMBDA_NSP32_ARDUINO_ADAPTOR_H

#include <SoftwareSerial.h>
#include "NSP32.h"
#include "IMcuAdaptor.h"

namespace NanoLambdaNSP32
{
	/**
	 * Arduino adaptor for NSP32 class
	 */	
	class ArduinoAdaptor : public IMcuAdaptor
	{
		public:
			ArduinoAdaptor(uint32_t gpioPinRst, uint32_t gpioPinSS);		// constructor (for data channel == SPI)
			ArduinoAdaptor(uint32_t gpioPinRst, SoftwareSerial *pSerial, NSP32::UartBaudRateEnum baudRate);	// constructor (for data channel == UART)

		private:
			uint32_t m_gpioPinRst;					// reset pin No. (GPIO pin to reset NSP32)
			uint32_t m_gpioPinSS;					// the SPI chip select pin
			SoftwareSerial *m_pSoftSerial;			// pointer to SoftwareSerial
			NSP32::UartBaudRateEnum m_uartBaudRate;	// UART baud rate
			uint32_t m_startMillis;					// starting milliseconds

			virtual void Init();					// initialize adaptor
			virtual void DelayMicros(uint32_t us);	// delay microseconds
			virtual void DelayMillis(uint32_t ms);	// delay milliseconds

			virtual void PinRstOutputLow();			// set the reset pin (to reset NSP32) to output mode and set "low"
			virtual void PinRstHighInput();			// set the reset pin (to reset NSP32) "high" and set it to input mode

			// ********** the following functions are only used when data channel is SPI **********

			virtual void SpiSend(uint8_t *pBuf, uint32_t len);		// send through SPI (only used when data channel is SPI)
			virtual void SpiReceive(uint8_t *pBuf, uint32_t len);	// receive from SPI (only used when data channel is SPI)

			// ********** the following functions are only used when data channel is UART **********

			virtual void StartMillis();				// start to count milliseconds (only used when data channel is UART)
			virtual uint32_t GetMillisPassed();		// get milliseconds passed since last call to StartMillis() (only used when data channel is UART)
			virtual bool UartBytesAvailable();		// see if any bytes available for reading from UART (only used when data channel is UART)
			virtual uint8_t UartReadByte();			// read a single byte from UART (only used when data channel is UART)
			virtual void UartSend(uint8_t *pBuf, uint32_t len);		// send through UART (only used when data channel is UART)
	};
};

#endif
