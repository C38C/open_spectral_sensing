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

/**
 * A console program to demonstrate full functionalities of NSP32.
 *
 * Please open "Serial Monitor" in Arduino IDE, make sure the setting is "NL(newline)" and "115200 baud", and then run the example.
 *
 * [NOTE]: According to your board type, you might need to change the pin number assigned at the beginning of the source code.
 *
 * Typical pin layout used:
 * ---------------------------------------------------------
 *               NSP32      Arduino       Arduino   Arduino   
 *  SPI                     Uno/101       Mega      Nano v3 
 * Signal        Pin          Pin           Pin       Pin     
 * -----------------------------------------------------------
 * Wakeup/Reset  RST          8             49        D8      
 * SPI SSEL      SS           10            53        D10     
 * SPI MOSI      MOSI         11 / ICSP-4   51        D11     
 * SPI MISO      MISO         12 / ICSP-1   50        D12     
 * SPI SCK       SCK          13 / ICSP-3   52        D13     
 * Ready         Ready        2             21        D2
 * ---------------------------------------------------------
 *               NSP32      Arduino       Arduino   Arduino   
 *  UART                    Uno/101       Mega      Nano v3 
 * Signal        Pin          Pin           Pin       Pin     
 * -----------------------------------------------------------
 * Wakeup/Reset  RST          8             49        D8      
 * UART RX       TX           10            53        D10
 * BAUD_SEL[0]   BAUD_SEL[0]  11 / ICSP-4   51        D11     UART baud rate select bit0
 * BAUD_SEL[1]   BAUD_SEL[1]  12 / ICSP-1   50        D12     UART baud rate select bit1
 * UART TX       RX           13 / ICSP-3   52        D13
 * Ready         Ready        2             21        D2
 * 
 */

#include <SoftwareSerial.h>
#include <ArduinoAdaptor.h>
#include <NSP32.h>

using namespace NanoLambdaNSP32;

/***********************************************************************************
 * modify this section to fit your need                                            *
 ***********************************************************************************/

	//#define USE_UART		// define to use UART as data channel, undefine to use SPI
		
	const unsigned int PinRst			= 8;  			// pin Reset
	const unsigned int PinReady			= 2;  			// pin Ready
	
	#ifdef USE_UART		// use UART
		const unsigned int PinRx		= 10;		// pin UART RX (connect to TX of NSP32)
		const unsigned int PinBaudRate0	= 11;		// pin UART baud rate select bit0
		const unsigned int PinBaudRate1	= 12;		// pin UART baud rate select bit1
		const unsigned int PinTx		= 13;		// pin UART TX (connect to RX of NSP32)
	
		SoftwareSerial softSerial(PinRx, PinTx);							// software serial
		ArduinoAdaptor adaptor(PinRst, &softSerial, NSP32::BaudRate9600);	// master MCU adaptor (use 9600 baud rate)
		NSP32 nsp32(&adaptor, NSP32::ChannelUart);							// NSP32 (using UART channel)
	#else				// use SPI
		ArduinoAdaptor adaptor(PinRst);										// master MCU adaptor
		NSP32 nsp32(&adaptor, NSP32::ChannelSpi);							// NSP32 (using SPI channel)
	#endif

/***********************************************************************************/

const unsigned long ConsoleUartBaudRate	= 115200;	// UART baud rate (for "Serial Monitor")

const uint32_t ConsoleBufSize = 20;		// console serial buffer size
uint8_t consoleBuf[ConsoleBufSize];		// console serial buffer

// console serial command state enumeration
enum ConsoleCmdState { Standby = 0, Wakeup, Hello, GetSensorId, GetWavelength, AcqSpectrum, AcqXYZ, ShowCommands };

// console serial command strings
const char* ConsoleCmdStr[] = { "standby", "wakeup", "hello", "sensorid", "wavelength", "spectrum", "xyz", "commands" };

void setup() 
{
	// if data channel == UART, set GPIO to select baud rate
	#ifdef USE_UART
		// bit1 = 0, bit0 = 0 -> 9600 baud rate
		pinMode(PinBaudRate1, OUTPUT);
		digitalWrite(PinBaudRate1, LOW);

		pinMode(PinBaudRate0, OUTPUT);
		digitalWrite(PinBaudRate0, LOW);
	#endif

	// initialize "ready trigger" pin for accepting external interrupt (falling edge trigger)
	pinMode(PinReady, INPUT_PULLUP);     // use pull-up
	attachInterrupt(digitalPinToInterrupt(PinReady), PinReadyTriggerISR, FALLING);  // enable interrupt for falling edge
	
	// initialize serial port for "Serial Monitor"
	Serial.setTimeout(-1);
	Serial.begin(ConsoleUartBaudRate);

	// initialize NSP32
	nsp32.Init();

	// show available console commands to user
	ShowConsoleCommands();
}

void loop() 
{
	switch(GetUserInputConsoleCommand())
	{
		case Standby:			// standby
			nsp32.Standby(0);
			Serial.print(F("standby ok\n\n"));
			break;

		case Wakeup:			// wakeup
			nsp32.Wakeup();	
			Serial.print(F("wakeup ok\n\n"));
			break;

		case Hello:				// hello
			nsp32.Hello(0);	
			Serial.print(F("hello ok\n\n"));
			break;

		case GetSensorId:		// get sensor id
			nsp32.GetSensorId(0);

			// extract sensor id string from the return packet, and print out
			{				
				char szSensorId[15];
				nsp32.ExtractSensorIdStr(szSensorId);
			
				Serial.print(F("sensor id = "));
				Serial.print(szSensorId);
				Serial.print(F("\n\n"));
			}

			break;

		case GetWavelength:		// get wavelength
			nsp32.GetWavelength(0);
			
			// extract wavelength info from the return packet, and print out
			{
				uint32_t i;
				WavelengthInfo info;
				nsp32.ExtractWavelengthInfo(&info);
							
				// num of points
				uint32_t numOfPoints = info.NumOfPoints;
				Serial.print(F("num of points = "));
				Serial.println(numOfPoints);
				
				// wavelength
				uint16_t *pWavelength = info.Wavelength;
				Serial.print(F("wavelength =\t"));
				
				for(i = 0; i < numOfPoints; ++i)
				{
					Serial.print(*pWavelength++);
					Serial.print(F("\t"));
				}
				
				Serial.print(F("\n\n"));
			}

			break;

		case AcqSpectrum:		// spectrum acquisition
			nsp32.AcqSpectrum(0, 32, 3, false);	// integration time = 32; frame avg num = 3; disable AE
			
			// "AcqSpectrum" command takes longer time to execute, the return packet is not immediately available
			// when the acquisition is done, a "ready trigger" will fire, and nsp32.GetReturnPacketSize() will be > 0
			while(nsp32.GetReturnPacketSize() <= 0)
			{
				nsp32.UpdateStatus(); // call UpdateStatus() to check async result
			}
			
			// extract spectrum info from the return packet, and print out
			{
				uint32_t i;
				SpectrumInfo info;
				nsp32.ExtractSpectrumInfo(&info);
				
				// integration time
				Serial.print(F("integration time = "));
				Serial.println(info.IntegrationTime);

				// saturation flag
				Serial.print(F("saturation flag = "));
				Serial.println(info.IsSaturated);
				
				// num of points
				uint32_t numOfPoints = info.NumOfPoints;
				Serial.print(F("num of points = "));
				Serial.println(numOfPoints);
								
				// spectrum data
				float* pSpectrumData = info.Spectrum;
				Serial.print(F("spectrum =\t"));
				
				for(i = 0; i < numOfPoints; ++i)
				{
					Serial.print(*pSpectrumData++, 6);
					Serial.print(F("\t"));
				}
				
				Serial.print(F("\n")); 
				
				// X, Y, Z
				Serial.print(F("X, Y, Z =\t"));			
				Serial.print(info.X, 1);
				Serial.print(F("\t"));
				Serial.print(info.Y, 1);
				Serial.print(F("\t"));
				Serial.print(info.Z, 1);
				Serial.print(F("\n\n"));
			}

			break;

		case AcqXYZ:		// XYZ acquisition
			nsp32.AcqXYZ(0, 32, 3, false);	// integration time = 32; frame avg num = 3; disable AE
			
			// "AcqXYZ" command takes longer time to execute, the return packet is not immediately available
			// when the acquisition is done, a "ready trigger" will fire, and nsp32.GetReturnPacketSize() will be > 0
			while(nsp32.GetReturnPacketSize() <= 0)
			{
				nsp32.UpdateStatus(); // call UpdateStatus() to check async result
			}
			
			// extract XYZ info from the return packet, and print out
			{
				XYZInfo info;
				nsp32.ExtractXYZInfo(&info);

				// integration time
				Serial.print(F("integration time = "));
				Serial.println(info.IntegrationTime);

				// saturation flag
				Serial.print(F("saturation flag = "));
				Serial.println(info.IsSaturated);

				// X, Y, Z
				Serial.print(F("X, Y, Z =\t"));
				Serial.print(info.X, 1);
				Serial.print(F("\t"));
				Serial.print(info.Y, 1);
				Serial.print(F("\t"));
				Serial.print(info.Z, 1);
				Serial.print(F("\n\n"));
			}

			break;
		
		case ShowCommands:	// show available console commands to user
			ShowConsoleCommands();
			break;
	}
}

// get user input command on the console
uint8_t GetUserInputConsoleCommand()
{
	while(true)
	{
		// read characters from console, until the user press ENTER
		uint32_t cmdLen = Serial.readBytesUntil('\n', consoleBuf, ConsoleBufSize);
		
		// if a CR (carriage return) appears at the end, remove it
		if(consoleBuf[cmdLen - 1] == '\r')
		{
			--cmdLen;
		} 
		
		// append a '\0' at the end of the command, to make it a zero-terminated string
		consoleBuf[cmdLen] = '\0';
	
		// compare if the command matches any pre-defined command
		for(uint32_t i = 0; i < 8; ++i)
		{
			if(strcmp(consoleBuf, ConsoleCmdStr[i]) == 0)
			{
				return i;
			}
		}
	
		// if no matches found, print an error message
		Serial.print(F("** invalid command **\n\n"));
	}
}

// show available console commands to user
void ShowConsoleCommands()
{
	Serial.println(F("Usage: type an available command (case sensitive) in the Serial Monitor and send"));
	Serial.println(F("Note: make sure your Serial Monitor setting is 'NL(newline)' and '115200 baud'"));
	Serial.println(F(""));
	Serial.println(F("available commands:"));
	Serial.println(F("------------------------"));
	Serial.println(F("1) standby - standby NSP32"));
	Serial.println(F("2) wakeup - wakeup NSP32"));
	Serial.println(F("3) hello - say hello to NSP32"));
	Serial.println(F("4) sensorid - get sensor id string"));
	Serial.println(F("5) wavelength - get wavelength"));
	Serial.println(F("6) spectrum - start spectrum acquisition and get the result data"));
	Serial.println(F("7) xyz - start XYZ acquisition and get the result data"));
	Serial.println(F("8) commands - show command usage"));
	Serial.println(F(""));
}

void PinReadyTriggerISR()
{
	// make sure to call this function when receiving a ready trigger from NSP32
	nsp32.OnPinReadyTriggered();
}
