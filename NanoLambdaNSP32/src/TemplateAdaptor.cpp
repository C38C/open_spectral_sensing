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

#include "TemplateAdaptor.h"

using namespace NanoLambdaNSP32;

/**
 * constructor (for data channel == SPI)
 * @param gpioPinRst reset pin No. (GPIO pin to reset NSP32)
 */
TemplateAdaptor::TemplateAdaptor(uint32_t gpioPinRst)
{
	// TODO: Initialize member variables
}

/**
 * constructor (for data channel == UART)
 * @param gpioPinRst reset pin No. (GPIO pin to reset NSP32)
 * @param baudRate UART baud rate
 */
TemplateAdaptor::TemplateAdaptor(uint32_t gpioPinRst, NSP32::UartBaudRateEnum baudRate)
{
	// TODO: Initialize member variables
}

/**
 * initialize adaptor
 */
void TemplateAdaptor::Init()
{
	// TODO: Do some initialization tasks (e.g. initialize SPI or UART)
}

/**
 * delay microseconds
 * @param us microseconds
 */
void TemplateAdaptor::DelayMicros(uint32_t us)
{
	// TODO: Let MCU delay "us" microseconds
}

/**
 * delay milliseconds
 * @param ms milliseconds
 */
void TemplateAdaptor::DelayMillis(uint32_t ms)
{
	// TODO: Let MCU delay "ms" milliseconds
}

/**
 * set the reset pin (to reset NSP32) to output mode and set "low"
 */
void TemplateAdaptor::PinRstOutputLow()
{	
	// TODO: Configure the reset pin as output push-pull
	
	// TODO: Let the reset pin output LOW
}

/**
 * set the reset pin (to reset NSP32) "high" and set it to input mode
 */
void TemplateAdaptor::PinRstHighInput()
{
	// TODO: Let the reset pin output HIGH

	// TODO: Configure the reset pin as input
}

/**
 * send through SPI (only used when data channel is SPI)
 * @param pBuf buffer to send
 * @param len data length
 */
void TemplateAdaptor::SpiSend(uint8_t *pBuf, uint32_t len)
{
	// ***** no need to implement if not using SPI channel *****
	
	// TODO: Set SPI baud rate = 2Mbits/s
	
	// TODO: Let SPI SS pin output LOW

	// TODO: Send data through SPI

	// TODO: Let SPI SS pin output HIGH
}

/**
 * receive from SPI (only used when data channel is SPI)
 * @param pBuf buffer to receive
 * @param len data length
 */
void TemplateAdaptor::SpiReceive(uint8_t *pBuf, uint32_t len)
{
	// ***** no need to implement if not using SPI channel *****
	
	// TODO: Set SPI baud rate = 4Mbits/s
	
	// TODO: Let SPI SS pin output LOW

	// TODO: Receive data from SPI

	// TODO: Let SPI SS pin output HIGH
}

/**
 * start to count milliseconds (only used when data channel is UART)
 */
void TemplateAdaptor::StartMillis()
{
	// ***** no need to implement if not using UART channel *****
	
	// TODO: Record current system time as the start_time
}

/**
 * get milliseconds passed since last call to StartMillis() (only used when data channel is UART)
 * @return milliseconds passed
 */
uint32_t TemplateAdaptor::GetMillisPassed()
{
	// ***** no need to implement if not using UART channel *****
	
	// TODO: Calculate milliseconds_passed = (current_system_time - start_time), and return the result
	// [NOTE] You need to deal with the overflow situation (i.e. when current_system_time < start_time, it means a timer counter overflow occurs)
	
	return 0;
}

/**
 * see if any bytes available for reading from UART (only used when data channel is UART)
 * @return true for bytes available; false for none
 */
bool TemplateAdaptor::UartBytesAvailable()
{
	// ***** no need to implement if not using UART channel *****
	
	// TODO: return true if any bytes available for reading from UART, otherwise return false

	return false;
}

/**
 * read a single byte from UART (only used when data channel is UART)
 * @return single byte reading from UART
 */
uint8_t TemplateAdaptor::UartReadByte()
{
	// ***** no need to implement if not using UART channel *****
	
	// TODO: read one single byte from UART and return it

	return 0;
}

/**
 * send through UART (only used when data channel is UART)
 * @param pBuf buffer to send
 * @param len data length
 */
void TemplateAdaptor::UartSend(uint8_t *pBuf, uint32_t len)
{
	// ***** no need to implement if not using UART channel *****

	// TODO: Send data through UART
}
