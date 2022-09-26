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

#ifndef __NANOLAMBDA_NSP32_NSP32_H
#define __NANOLAMBDA_NSP32_NSP32_H

#include <stdint.h>
#include "IMcuAdaptor.h"
#include "WavelengthInfo.h"
#include "SpectrumInfo.h"
#include "XYZInfo.h"

namespace NanoLambdaNSP32
{		
	/**
	 * NSP32 main class
	 */	
	class NSP32
	{
		public:	
			/**
			 * command code enumeration
			 */
			enum CmdCodeEnum
			{
				CodeUnknown			= 0x00,		/**< unknown */
				CodePrefix0			= 0x03,		/**< prefix 0 */
				CodePrefix1			= 0xBB,		/**< prefix 1 */

				CodeHello			= 0x01,		/**< hello */
				CodeStandby			= 0x04,		/**< standby */
				CodeGetSensorId		= 0x06,		/**< get sensor id */
				CodeGetWavelength	= 0x24,		/**< get wavelength */
				CodeAcqSpectrum		= 0x26,		/**< spectrum acquisition */
				CodeGetSpectrum		= 0x28,		/**< get spectrum data */
				CodeAcqXYZ			= 0x2A,		/**< XYZ acquisition */
				CodeGetXYZ			= 0x2C		/**< get XYZ data */
			};

			/**
			 * data channel enumeration
			 */
			enum DataChannelEnum
			{
				ChannelSpi		= 0,		/**< SPI */
				ChannelUart		= 1			/**< UART */
			};

			/**
			 * UART baud rate enumeration
			 */
			enum UartBaudRateEnum
			{
				BaudRate9600	= 9600,		/**< UART baud rate 9600 */
				BaudRate19200	= 19200,	/**< UART baud rate 19200 */
				BaudRate38400	= 38400,	/**< UART baud rate 38400 */
				BaudRate115200	= 115200	/**< UART baud rate 115200 */
			};

			NSP32(IMcuAdaptor *pAdaptor, DataChannelEnum channel);	// constructor
			
			void Init();							// initialize NSP32
			bool IsActive();						// check if NSP32 is in active mode
			void Wakeup();							// wakeup/reset NSP32

			void Hello(uint8_t userCode);			// say hello to NSP32
			void Standby(uint8_t userCode);			// standby NSP32
			void GetSensorId(uint8_t userCode);		// get sensor id
			void GetWavelength(uint8_t userCode);	// get wavelength
			void AcqSpectrum(uint8_t userCode, uint16_t integrationTime, uint8_t frameAvgNum, bool enableAE);	// start spectrum acquisition
			void AcqXYZ(uint8_t userCode, uint16_t integrationTime, uint8_t frameAvgNum, bool enableAE);		// start XYZ acquisition
			
			void OnPinReadyTriggered();				// "ready trigger" handler (call this function when master MCU receives a ready trigger on GPIO from NSP32)
			void UpdateStatus();					// update status (including checking async results, and processing forward commands)

			void FwdCmdByte(uint8_t fwd);			// forward a command byte to NSP32 (call this function when master MCU receives a command byte sent from PC/App) (only used when master MCU acts as a forwarder)
			
			void ClearReturnPacket();				// clear return packet
			uint32_t GetReturnPacketSize();			// get the return packet size (return 0 if the return packet is not yet available or is cleared)
			uint8_t* GetReturnPacketPtr();			// get the pointer to the return packet (return 0 if the return packet is not yet available or is cleared)

			void ExtractSensorIdStr(char *szStrBuf);			// extract sensor id string from the return packet ([NOTE]: the string occupies 15 bytes, including the zero terminated byte)
			void ExtractWavelengthInfo(WavelengthInfo *pInfo);	// extract wavelength info from the return packet
			void ExtractSpectrumInfo(SpectrumInfo *pInfo);		// extract spectrum info from the return packet
			void ExtractXYZInfo(XYZInfo *pInfo);				// extract XYZ info from the return packet
			
		private:
			static const uint32_t CmdBufSize			= 20;		// command buffer size = 20 bytes
			static const uint32_t RetBufSize			= 12 + 135 * 4 + 12 + 1;	// return packet buffer size = 565 bytes (equals to the max return packet length)

			static const uint32_t WakeupPulseHoldUs		= 50;		// wakeup pulse holding time = 50us
			static const uint32_t CmdProcessTimeMs		= 1;		// time interval between "command packet transmission end" and "return packet transmission start" = 1ms (only used when data channel is SPI)
			static const uint32_t CmdRetryIntervalMs	= 150;		// retry interval on packet error = 150ms

			static const uint32_t UartLowestBaudRate	= 9600;		// lowest UART baud rate option = 9600 bps
			static const uint32_t UartTimeoutMs			= 2 * (RetBufSize * 8 * 1000 / UartLowestBaudRate);	// UART trnamission timeout = 941ms (double transmission time for the largest return packet with the lowest UART baud rate)

			IMcuAdaptor *m_pMcuAdaptor;				// pointer to master MCU adaptor
			DataChannelEnum m_channelType;			// data channel type (SPI or UART)

			bool m_isActive;						// "is NSP32 in active mode" flag
			uint8_t m_userCode;						// command user code
			CmdCodeEnum m_asyncCmdCode;				// asynchronous command code (command waiting for async result)
			volatile bool m_isPinReadyTriggered;	// "is ready pin triggered" flag
			uint8_t m_cmdBuf[CmdBufSize];			// command buffer

			uint32_t m_retPacketSize;				// return packet size
			uint8_t m_retBuf[RetBufSize];			// return packet buffer

			uint32_t m_fwdBufWriteIdx;				// forward buffer write index
			uint32_t m_fwdCmdLen;					// forward command length
			bool m_isFwdCmdFilled;					// "is forward command filled" flag
			uint8_t m_fwdBuf[CmdBufSize];			// forward buffer
			
			bool SendCmd(uint32_t cmdLen, uint32_t retLen, CmdCodeEnum cmdCode, uint8_t userCode, bool keepSilent, bool waitReadyTrigger, bool errorRetry);		// send command to NSP32
			void PlaceChecksum(uint8_t *pBuf, uint32_t len);			// calculate checksum and append it to the end of the buffer (use "modular sum" method)
			bool IsChecksumValid(const uint8_t *pBuf, uint32_t len);	// check if the checksum is valid (use "modular sum" method)
	};
};

#endif
