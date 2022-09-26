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

#include <stdio.h>
#include <string.h>
#include "NSP32.h"

using namespace NanoLambdaNSP32;

/**
 * constructor
 * @param pAdaptor master MCU adaptor
 * @param channel data channel type (SPI or UART)
 */
NSP32::NSP32(IMcuAdaptor *pAdaptor, DataChannelEnum channel)
	: m_pMcuAdaptor(pAdaptor), m_channelType(channel),
		m_isActive(false), m_asyncCmdCode(CodeUnknown),	m_retPacketSize(0),
		m_fwdBufWriteIdx(0), m_isFwdCmdFilled(false)
{
}

/**
 * initialize NSP32
 */
void NSP32::Init()
{
	// initialize master MCU adaptor
	m_pMcuAdaptor->Init();

	// reset NSP32 and check functionality
	Wakeup();
}

/**
 * check if NSP32 is in active mode
 * @return true for active mode; false for standby mode
 */
bool NSP32::IsActive()
{
	return m_isActive;
}

/**
 * wakeup/reset NSP32
 */
void NSP32::Wakeup()
{	
	// continuously reset NSP32 until the functionality check passes
	while(true)
	{
		// generate a pulse signal to reset NSP32 (low active reset)
		m_pMcuAdaptor->PinRstOutputLow();
		m_pMcuAdaptor->DelayMillis(25);	// hold the signal low for a period of time
	
		m_isPinReadyTriggered = false;		// clear the flag, so that we can detect the "ready trigger" later on
	
		m_pMcuAdaptor->PinRstHighInput();
		m_pMcuAdaptor->DelayMillis(50);
	
		// wait until the reboot procedure is done (the "ready trigger" is fired)
		while(!m_isPinReadyTriggered) {}

		// test if the SPI/UART communication is well established
		// send "HELLO" command and check the return packet
		// if the return packet is incorrect, reset again
		if(SendCmd(4 + 1, 4 + 1, CodeHello, 0, true, false, false))
		{
			break;
		}
	}

	// record that NSP32 is active now
	m_isActive = true;
}

/**
 * say hello to NSP32
 * @param userCode user code
 */
void NSP32::Hello(uint8_t userCode)
{
	// if NSP32 is in standby mode, wakeup first
	if(!m_isActive)
	{
		Wakeup();
	}

	SendCmd(4 + 1, 4 + 1, CodeHello, userCode, false, false, true);
}

/**
 * standby NSP32
 * @param userCode user code
 */
void NSP32::Standby(uint8_t userCode)
{
	// if NSP32 is already in standby mode, we just generate the return packet
	if(!m_isActive)
	{
		m_retBuf[0] = CodePrefix0;
		m_retBuf[1] = CodePrefix1;
		m_retBuf[2] = CodeStandby;
		m_retBuf[3] = userCode;
		PlaceChecksum(m_retBuf, 4);
		
		m_retPacketSize = 4 + 1;
		return;
	}

	// make sure NSP32 correctly enters standy mode
	while(true)
	{
		// send command to standby NSP32
		if(SendCmd(4 + 1, 4 + 1, CodeStandby, userCode, false, false, false))
		{
			m_isActive = false;
			return;
		}
		
		// in case we don't get correct respond from NSP32, reset it and retry
		Wakeup();
	}
}

/**
 * get sensor id
 * @param userCode user code
 */
void NSP32::GetSensorId(uint8_t userCode)
{
	// if NSP32 is in standby mode, wakeup first
	if(!m_isActive)
	{
		Wakeup();
	}

	SendCmd(4 + 1, 4 + 5 + 1, CodeGetSensorId, userCode, false, false, true);
}

/**
 * get wavelength
 * @param userCode user code
 */
void NSP32::GetWavelength(uint8_t userCode)
{
	// if NSP32 is in standby mode, wakeup first
	if(!m_isActive)
	{
		Wakeup();
	}

	SendCmd(4 + 1, 8 + 135 * 2 + 1, CodeGetWavelength, userCode, false, false, true);
}

/**
 * start spectrum acquisition
 * @param userCode user code
 * @param integrationTime integration time
 * @param frameAvgNum frame average num
 * @param enableAE true to enable AE; false to disable AE
 */
void NSP32::AcqSpectrum(uint8_t userCode, uint16_t integrationTime, uint8_t frameAvgNum, bool enableAE)
{
	// if NSP32 is in standby mode, wakeup first
	if(!m_isActive)
	{
		Wakeup();
	}

	m_cmdBuf[4] = (uint8_t)integrationTime;
	m_cmdBuf[5] = (uint8_t)(integrationTime >> 8);
	m_cmdBuf[6] = frameAvgNum;
	m_cmdBuf[7] = enableAE ? 1 : 0;
	m_cmdBuf[8] = 0;	// no active return
	SendCmd(4 + 5 + 1, 4 + 1, CodeAcqSpectrum, userCode, true, true, true);
}

/**
 * start XYZ acquisition
 * @param userCode user code
 * @param integrationTime integration time
 * @param frameAvgNum frame average num
 * @param enableAE true to enable AE; false to disable AE
 */
void NSP32::AcqXYZ(uint8_t userCode, uint16_t integrationTime, uint8_t frameAvgNum, bool enableAE)
{
	// if NSP32 is in standby mode, wakeup first
	if(!m_isActive)
	{
		Wakeup();
	}

	m_cmdBuf[4] = (uint8_t)integrationTime;
	m_cmdBuf[5] = (uint8_t)(integrationTime >> 8);
	m_cmdBuf[6] = frameAvgNum;
	m_cmdBuf[7] = enableAE ? 1 : 0;
	m_cmdBuf[8] = 0;	// no active return
	SendCmd(4 + 5 + 1, 4 + 1, CodeAcqXYZ, userCode, true, true, true);
}

/**
 * "ready trigger" handler (call this function when master MCU receives a ready trigger on GPIO from NSP32)
 */
void NSP32::OnPinReadyTriggered()
{
	m_isPinReadyTriggered = true;
}

/**
 * update status (including checking async results, and processing forward commands)
 */
void NSP32::UpdateStatus()
{
	// check for "AcqSpectrum" async result
	// if "AcqSpectrum" is done, send a command to retrieve the data
	if((m_asyncCmdCode == CodeAcqSpectrum) && m_isPinReadyTriggered)
	{
		SendCmd(4 + 1, 12 + 135 * 4 + 12 + 1, CodeGetSpectrum, m_userCode, false, false, true);
	}

	// check for "AcqXYZ" async result
	// if "AcqXYZ" is done, send a command to retrieve the data
	if((m_asyncCmdCode == CodeAcqXYZ) && m_isPinReadyTriggered)
	{
		SendCmd(4 + 1, 8 + 12 + 1, CodeGetXYZ, m_userCode, false, false, true);
	}
	
	// start processing forward commands
	if(!m_isFwdCmdFilled)
	{
		return;
	}
	
	// if a forward command is filled, copy the command bytes to command buffer
	memcpy(m_cmdBuf, m_fwdBuf, m_fwdCmdLen);
	m_isFwdCmdFilled = false;	// clear the flag, so that new incoming bytes could be accepted
	
	uint8_t userCode = m_cmdBuf[3];		// extract command user code
	
	// process the command based on its function code
	switch(m_cmdBuf[2])
	{
		case CodeHello:			// hello
			Hello(userCode);
			break;
		
		case CodeStandby:		// standby
			Standby(userCode);
			break;
		  		
		case CodeGetSensorId:	// get sensor id
			GetSensorId(userCode);
			break;

		case CodeGetWavelength:	// get wavelength
			GetWavelength(userCode);
			break;
		
		case CodeAcqSpectrum:	// spectrum acquisition
			AcqSpectrum(userCode, ((uint16_t)m_cmdBuf[5] << 8) | ((uint16_t)m_cmdBuf[4]), m_cmdBuf[6], m_cmdBuf[7] != 0);
			break;  

		case CodeAcqXYZ:		// XYZ acquisition
			AcqXYZ(userCode, ((uint16_t)m_cmdBuf[5] << 8) | ((uint16_t)m_cmdBuf[4]), m_cmdBuf[6], m_cmdBuf[7] != 0);
			break;  
	}
}

/**
 * forward a command byte to NSP32 (call this function when master MCU receives a command byte sent from PC/App) (only used when master MCU acts as a forwarder)
 * @param fwd single byte to forward
 */
void NSP32::FwdCmdByte(uint8_t fwd)
{
	// if a command is filled but not yet being processed, reject new incoming bytes
	if(m_isFwdCmdFilled)
	{
		return;
	}
	
	// store incoming bytes to the forward buffer
	// align command prefix code to the beginning of the buffer
	if(((m_fwdBufWriteIdx == 0 && fwd == CodePrefix0) || m_fwdBufWriteIdx > 0) && m_fwdBufWriteIdx < CmdBufSize)
	{
		m_fwdBuf[m_fwdBufWriteIdx++] = fwd;
	}
	
	if(m_fwdBufWriteIdx > 1 && m_fwdBuf[1] != CodePrefix1)
	{
		// if the command prefix code mismatches, clear the buffer
		m_fwdBufWriteIdx = 0;
	}
	else if(m_fwdBufWriteIdx > 2)
	{
		// determine the command length based on command function code
		switch(m_fwdBuf[2])
		{			
			case CodeHello:
			case CodeStandby:
			case CodeGetSensorId:
			case CodeGetWavelength:
				m_fwdCmdLen = 4 + 1;
				break;

			case CodeAcqSpectrum:
			case CodeAcqXYZ:
				m_fwdCmdLen = 4 + 5 + 1;
				break;
			  
			default:    // unrecognized command
				m_fwdCmdLen = 0;
				m_fwdBufWriteIdx = 0;	// clear the buffer
				break;
		}
		
		// if num of bytes in the buffer reaches command length, a full command is filled
		if(m_fwdCmdLen > 0 && m_fwdBufWriteIdx >= m_fwdCmdLen)
		{
			m_fwdBufWriteIdx = 0;    		// reset the buffer write index

			// if checksum is valid, accept it, otherwise discard it
			if(IsChecksumValid(m_fwdBuf, m_fwdCmdLen))
			{
				m_isFwdCmdFilled = true;	// set the "forward command filled" flag
			}
		}
	}
}

/**
 * send command to NSP32
 * @param cmdLen command length (including the checksum)
 * @param retLen expected return packet length (including the checksum)
 * @param cmdCode command function code
 * @param userCode command user code
 * @param keepSilent true to hide the return packet from end users; false to forward the return packet to end users
 * @param waitReadyTrigger true for async commands (commands that take longer time to execute); false for sync commands (commands that return packets immediately)
 * @param errorRetry true to retry on return packet error; false to ignore the error
 * @return true for return packet correctly received; false for packet error
 */
bool NSP32::SendCmd(uint32_t cmdLen, uint32_t retLen, CmdCodeEnum cmdCode, uint8_t userCode, bool keepSilent, bool waitReadyTrigger, bool errorRetry)
{
	m_cmdBuf[0] = CodePrefix0;
	m_cmdBuf[1] = CodePrefix1;
	m_cmdBuf[2] = cmdCode;
	m_cmdBuf[3] = userCode;
	PlaceChecksum(m_cmdBuf, cmdLen - 1);	// add checksum

	m_retPacketSize = 0;
	m_asyncCmdCode = waitReadyTrigger ? cmdCode : CodeUnknown;
	m_userCode = userCode;
	m_isPinReadyTriggered = false;

	// backup the command buffer, in case we need to retry
	// (some MCU will overwrite m_cmdBuf while SpiSend(), so we need to backup)
	uint8_t cmdBufBackup[CmdBufSize];
	memcpy(cmdBufBackup, m_cmdBuf, cmdLen);

	while(true)
	{
		bool isTimeout = false;
		
		// some MCU will send the buffer content (which contains the data of previous return packet) when SpiReceive(), 
		// so we have to clear it first, to prevent NSP32 from receiving "fake" command
		memset(m_retBuf, 0, RetBufSize);
		
		switch(m_channelType)
		{
			case ChannelSpi:
				m_pMcuAdaptor->SpiSend(m_cmdBuf, cmdLen);		// send the command to NSP32 (through SPI)
				m_pMcuAdaptor->DelayMillis(CmdProcessTimeMs);	// wait for a short processing time
				m_pMcuAdaptor->SpiReceive(m_retBuf, retLen);	// get the return packet
				break;
				
			case ChannelUart:
				// clear UART RX buffer (by reading out all remaining bytes in the buffer)
				while(m_pMcuAdaptor->UartBytesAvailable())
				{
					m_pMcuAdaptor->UartReadByte();
				}
				
				m_pMcuAdaptor->UartSend(m_cmdBuf, cmdLen);		// send the command to NSP32 (through UART)
				m_pMcuAdaptor->StartMillis();					// start to count milliseconds (for timeout detection)
				
				// read expected length of bytes (return packet) from UART
				uint32_t writeIdx = 0;
				
				while(writeIdx < retLen)
				{
					// if we can't receive the expected length of bytes within a period of time, timeout occurs
					if(m_pMcuAdaptor->GetMillisPassed() > UartTimeoutMs)
					{
						isTimeout = true;
						break;
					}

					// read bytes from UART
					while(m_pMcuAdaptor->UartBytesAvailable() && writeIdx < retLen)
					{
						m_retBuf[writeIdx++] = m_pMcuAdaptor->UartReadByte();
					}
				}
				
				break;
		}				

		if(!isTimeout)
		{
			// check if the return packet is valid		
			if(m_retBuf[0] == CodePrefix0 && m_retBuf[1] == CodePrefix1 && 
				m_retBuf[2] == cmdCode && m_retBuf[3] == userCode &&
				IsChecksumValid(m_retBuf, retLen))
			{
				// if keep silent, hide the return packet from end users
				// if not keep silent, let end users know the return packet is available
				m_retPacketSize = keepSilent ? 0 : retLen;

				return true;
			}
		}
		
		// deal with the packet error situation
		if(errorRetry)
		{
			// if packet error, get ready to retry
			memcpy(m_cmdBuf, cmdBufBackup, cmdLen);				// restore the command buffer from backup
			m_pMcuAdaptor->DelayMillis(CmdRetryIntervalMs);		// delay a short interval before retry
		}
		else
		{
			// ignore the packet error and return directly
			return false;
		}
	}
}

/**
 * calculate checksum and append it to the end of the buffer (use "modular sum" method)
 * @param pBuf buffer
 * @param len data length (excluding the checksum)
 */
void NSP32::PlaceChecksum(uint8_t *pBuf, uint32_t len)
{
	uint8_t checksum = 0;

	// sum all bytes (excluding the checksum)
	while(len-- > 0)
	{
		checksum += *pBuf++;
	}

	// take two's complement, and append the checksum to the end
	*pBuf = (~checksum) + 1;
}

/**
 * check if the checksum is valid (use "modular sum" method)
 * @param pBuf buffer
 * @param len data length (including the checksum)
 * @return true for valid; false for invalid
 */
bool NSP32::IsChecksumValid(const uint8_t *pBuf, uint32_t len)
{
	uint8_t checksum = 0;

	// sum all bytes (including the checksum)
	while(len -- > 0)
	{
		checksum += *pBuf++;
	}

	// if the summation equals to 0, the checksum is valid
	return checksum == 0;
}

/**
 * clear return packet
 */
void NSP32::ClearReturnPacket()
{
	m_retPacketSize = 0;
}

/**
 * get the return packet size
 * @return return packet size (return 0 if the return packet is not yet available or is cleared)
 */
uint32_t NSP32::GetReturnPacketSize()
{
	return m_retPacketSize;
}
		
/**
 * get the pointer to the return packet
 * @return pointer to the return packet (return 0 if the return packet is not yet available or is cleared)
 */
uint8_t* NSP32::GetReturnPacketPtr()
{
	return m_retPacketSize <= 0 ? 0 : m_retBuf;
}

/**
 * extract sensor id string from the return packet ([NOTE]: the string occupies 15 bytes, including the zero terminated byte)
 * @param szStrBuf pointer to the destination string buffer
 */
void NSP32::ExtractSensorIdStr(char *szStrBuf)
{
	if(m_retPacketSize <= 0 || m_retBuf[2] != CodeGetSensorId)
	{
		// set to empty string if the return packet is not yet available or is cleared, or packet type mismatches
		szStrBuf[0] = '\0';
	}
	else
	{
		// format sensor id string
		sprintf(szStrBuf, "%02X-%02X-%02X-%02X-%02X", m_retBuf[4], m_retBuf[5], m_retBuf[6], m_retBuf[7], m_retBuf[8]);
	}
}

/**
 * extract wavelength info from the return packet
 * @param pInfo pointer to the wavelength info struct
 */
void NSP32::ExtractWavelengthInfo(WavelengthInfo *pInfo)
{
	if(m_retPacketSize <= 0 || m_retBuf[2] != CodeGetWavelength)
	{
		// clear the info structure if the return packet is not yet available or is cleared, or packet type mismatches
		memset((void*)pInfo, 0, sizeof(WavelengthInfo));
	}
	else
	{
		pInfo->NumOfPoints = *(uint32_t*)(m_retBuf + 4);
		pInfo->Wavelength = (uint16_t*)(m_retBuf + 8);
	}
}

/**
 * extract spectrum info from the return packet
 * @param pInfo pointer to the spectrum info struct
 */
void NSP32::ExtractSpectrumInfo(SpectrumInfo *pInfo)
{
	if(m_retPacketSize <= 0 || m_retBuf[2] != CodeGetSpectrum)
	{
		// clear the info structure if the return packet is not yet available or is cleared, or packet type mismatches
		memset((void*)pInfo, 0, sizeof(SpectrumInfo));
	}
	else
	{
		pInfo->IntegrationTime = *(uint16_t*)(m_retBuf + 4);
		pInfo->IsSaturated = *(m_retBuf + 6) == 1;
		pInfo->NumOfPoints = *(uint32_t*)(m_retBuf + 8);
		pInfo->Spectrum = (float*)(m_retBuf + 12);
		pInfo->X = *(float*)(m_retBuf + 12 + 135 * 4);
		pInfo->Y = *(float*)(m_retBuf + 12 + 135 * 4 + 4);
		pInfo->Z = *(float*)(m_retBuf + 12 + 135 * 4 + 8);
	}
}

/**
 * extract XYZ info from the return packet
 * @param pInfo pointer to the XYZ info struct
 */
void NSP32::ExtractXYZInfo(XYZInfo *pInfo)
{
	if(m_retPacketSize <= 0 || m_retBuf[2] != CodeGetXYZ)
	{
		// clear the info structure if the return packet is not yet available or is cleared, or packet type mismatches
		memset((void*)pInfo, 0, sizeof(XYZInfo));
	}
	else
	{
		pInfo->IntegrationTime = *(uint16_t*)(m_retBuf + 4);
		pInfo->IsSaturated = *(m_retBuf + 6) == 1;
		pInfo->X = *(float*)(m_retBuf + 8);
		pInfo->Y = *(float*)(m_retBuf + 8 + 4);
		pInfo->Z = *(float*)(m_retBuf + 8 + 8);
	}
}
