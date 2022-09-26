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

#ifndef __NANOLAMBDA_NSP32_IMCU_ADAPTOR_H
#define __NANOLAMBDA_NSP32_IMCU_ADAPTOR_H

#include <stdint.h>

namespace NanoLambdaNSP32
{
	/**
	 * This class acts as an interface, providing MCU dependent operations, so that NSP32 class could be adapted to different MCU types
	 */
	class IMcuAdaptor
	{
		/**
		 * NSP32 can access private members of IMcuAdaptor
		 */
		friend class NSP32;

		private:
			/**
			 * initialize adaptor
			 */
			virtual void Init() = 0;

			/**
			 * delay microseconds
			 * @param us microseconds
			 */
			virtual void DelayMicros(uint32_t us) = 0;

			/**
			 * delay milliseconds
			 * @param ms milliseconds
			 */
			virtual void DelayMillis(uint32_t ms) = 0;

			/**
			 * set the reset pin (to reset NSP32) to output mode and set "low"
			 */
			virtual void PinRstOutputLow() = 0;

			/**
			 * set the reset pin (to reset NSP32) "high" and set it to input mode
			 */
			virtual void PinRstHighInput() = 0;

			// ********** the following functions are only used when data channel is SPI **********

			/**
			 * send through SPI (only used when data channel is SPI)
			 * @param pBuf buffer to send
			 * @param len data length
			 */
			virtual void SpiSend(uint8_t *pBuf, uint32_t len) = 0;

			/**
			 * receive from SPI (only used when data channel is SPI)
			 * @param pBuf buffer to receive
			 * @param len data length
			 */
			virtual void SpiReceive(uint8_t *pBuf, uint32_t len) = 0;

			// ********** the following functions are only used when data channel is UART **********

			/**
			 * start to count milliseconds (only used when data channel is UART)
			 */
			virtual void StartMillis() = 0;

			/**
			 * get milliseconds passed since last call to StartMillis() (only used when data channel is UART)
			 * @return milliseconds passed
			 */
			virtual uint32_t GetMillisPassed() = 0;

			/**
			 * see if any bytes available for reading from UART (only used when data channel is UART)
			 * @return true for bytes available; false for none
			 */
			virtual bool UartBytesAvailable() = 0;

			/**
			 * read a single byte from UART (only used when data channel is UART)
			 * @return single byte reading from UART
			 */
			virtual uint8_t UartReadByte() = 0;

			/**
			 * send through UART (only used when data channel is UART)
			 * @param pBuf buffer to send
			 * @param len data length
			 */
			virtual void UartSend(uint8_t *pBuf, uint32_t len) = 0;
	};
};

#endif
