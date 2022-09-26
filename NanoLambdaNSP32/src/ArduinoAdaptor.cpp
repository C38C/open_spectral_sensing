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

#include <SPI.h>
#include "ArduinoAdaptor.h"

using namespace NanoLambdaNSP32;

/**
 * constructor (for data channel == SPI)
 * @param gpioPinRst reset pin No. (GPIO pin to reset NSP32)
 * @param gpioPinSS chip select pin No. (GPIO pin for SPI chip select)
 */
ArduinoAdaptor::ArduinoAdaptor(uint32_t gpioPinRst, uint32_t gpioPinSS)
	: m_gpioPinRst(gpioPinRst), m_gpioPinSS(gpioPinSS), m_pSoftSerial(0), m_startMillis(0)
{
}

/**
 * constructor (for data channel == UART)
 * @param gpioPinRst reset pin No. (GPIO pin to reset NSP32)
 * @param pSerial pointer to SoftwareSerial (the UART channel)
 * @param baudRate UART baud rate
 */
ArduinoAdaptor::ArduinoAdaptor(uint32_t gpioPinRst, SoftwareSerial *pSerial, NSP32::UartBaudRateEnum baudRate)
	: m_gpioPinRst(gpioPinRst), m_pSoftSerial(pSerial), m_uartBaudRate(baudRate), m_startMillis(0)
{
}

/**
 * initialize adaptor
 */
void ArduinoAdaptor::Init()
{
	if(m_pSoftSerial == 0)
	{
		SPI.setDataMode(SPI_MODE0);
		SPI.setBitOrder(MSBFIRST);
		SPI.begin();
	}
	else
	{
		m_pSoftSerial->begin(m_uartBaudRate);
	}
}

/**
 * delay microseconds
 * @param us microseconds
 */
void ArduinoAdaptor::DelayMicros(uint32_t us)
{
	delayMicroseconds(us);
}

/**
 * delay milliseconds
 * @param ms milliseconds
 */
void ArduinoAdaptor::DelayMillis(uint32_t ms)
{
	delay(ms);
}

/**
 * set the reset pin (to reset NSP32) to output mode and set "low"
 */
void ArduinoAdaptor::PinRstOutputLow()
{	
	pinMode(m_gpioPinRst, OUTPUT);
	digitalWrite(m_gpioPinRst, LOW);
}

/**
 * set the reset pin (to reset NSP32) "high" and set it to input mode
 */
void ArduinoAdaptor::PinRstHighInput()
{
	pinMode(m_gpioPinRst, OUTPUT);
	digitalWrite(m_gpioPinRst, HIGH);
}

/**
 * send through SPI (only used when data channel is SPI)
 * @param pBuf buffer to send
 * @param len data length
 */
void ArduinoAdaptor::SpiSend(uint8_t *pBuf, uint32_t len)
{
	//SPI.setClockDivider(SPI_CLOCK_DIV8);	// set SPI baud rate = 2Mbits/s
	SPI.beginTransaction(SPISettings(2000, MSBFIRST, SPI_MODE0));
	pinMode(m_gpioPinSS, OUTPUT);
	digitalWrite(m_gpioPinSS, LOW);		// SPI SS low
	SPI.transfer(pBuf, len);	// send
	digitalWrite(m_gpioPinSS, HIGH);		// SPI SS high
}

/**
 * receive from SPI (only used when data channel is SPI)
 * @param pBuf buffer to receive
 * @param len data length
 */
void ArduinoAdaptor::SpiReceive(uint8_t *pBuf, uint32_t len)
{
	//SPI.setClockDivider(SPI_CLOCK_DIV4);	// set SPI baud rate = 4Mbits/s
	SPI.beginTransaction(SPISettings(4000, MSBFIRST, SPI_MODE0));
	pinMode(m_gpioPinSS, OUTPUT);
	digitalWrite(m_gpioPinSS, LOW);		// SPI SS low
	SPI.transfer(pBuf, len);	// receive
	digitalWrite(m_gpioPinSS, HIGH);		// SPI SS high
}

/**
 * start to count milliseconds (only used when data channel is UART)
 */
void ArduinoAdaptor::StartMillis()
{
	m_startMillis = millis();
}

/**
 * get milliseconds passed since last call to StartMillis() (only used when data channel is UART)
 * @return milliseconds passed
 */
uint32_t ArduinoAdaptor::GetMillisPassed()
{
	return (uint32_t)(millis() - m_startMillis);
}

/**
 * see if any bytes available for reading from UART (only used when data channel is UART)
 * @return true for bytes available; false for none
 */
bool ArduinoAdaptor::UartBytesAvailable()
{
	return m_pSoftSerial->available();
}

/**
 * read a single byte from UART (only used when data channel is UART)
 * @return single byte reading from UART
 */
uint8_t ArduinoAdaptor::UartReadByte()
{
	return m_pSoftSerial->read();
}

/**
 * send through UART (only used when data channel is UART)
 * @param pBuf buffer to send
 * @param len data length
 */
void ArduinoAdaptor::UartSend(uint8_t *pBuf, uint32_t len)
{
	m_pSoftSerial->write(pBuf, len);
}
