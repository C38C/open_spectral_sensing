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
 * A clean and simple example for beginners to start with NSP32.
 *
 * Please open "Serial Monitor" in Arduino IDE, make sure the setting is "115200 baud", and then run the example.
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
 *
 */

#include <ArduinoAdaptor.h>
#include <NSP32.h>

using namespace NanoLambdaNSP32;

/***********************************************************************************
 * modify this section to fit your need                                            *
 ***********************************************************************************/

	const unsigned int PinRst	= 8;  	// pin Reset
	const unsigned int PinReady	= 2;  	// pin Ready

/***********************************************************************************/

ArduinoAdaptor adaptor(PinRst);				// master MCU adaptor
NSP32 nsp32(&adaptor, NSP32::ChannelSpi);	// NSP32 (using SPI channel)

void setup() 
{
	// initialize "ready trigger" pin for accepting external interrupt (falling edge trigger)
	pinMode(PinReady, INPUT_PULLUP);     // use pull-up
	attachInterrupt(digitalPinToInterrupt(PinReady), PinReadyTriggerISR, FALLING);  // enable interrupt for falling edge
	
	// initialize serial port for "Serial Monitor"
	Serial.begin(115200);
	
	// initialize NSP32
	nsp32.Init();

	// =============== standby ===============
	nsp32.Standby(0);

	// =============== wakeup ===============
	nsp32.Wakeup();

	// =============== hello ===============
	nsp32.Hello(0);

	// =============== get sensor id ===============
	nsp32.GetSensorId(0);

	char szSensorId[15];					// sensor id string buffer
	nsp32.ExtractSensorIdStr(szSensorId);	// now we have sensor id string in szSensorId[]

	Serial.print(F("sensor id = "));
	Serial.println(szSensorId);

	// =============== get wavelength ===============
	nsp32.GetWavelength(0);

	WavelengthInfo infoW;
	nsp32.ExtractWavelengthInfo(&infoW);	// now we have all wavelength info in infoW, we can use e.g. "infoW.Wavelength" to access the wavelength data array

	Serial.print(F("first element of wavelength = "));
	Serial.println(infoW.Wavelength[0]);
	
	// =============== spectrum acquisition ===============
	nsp32.AcqSpectrum(0, 32, 3, false);		// integration time = 32; frame avg num = 3; disable AE
	
	// "AcqSpectrum" command takes longer time to execute, the return packet is not immediately available
	// when the acquisition is done, a "ready trigger" will fire, and nsp32.GetReturnPacketSize() will be > 0
	while(nsp32.GetReturnPacketSize() <= 0)
	{
		// TODO: can go to sleep, and wakeup when "ready trigger" interrupt occurs
		
		nsp32.UpdateStatus();	// call UpdateStatus() to check async result
	}
	
	SpectrumInfo infoS;
	nsp32.ExtractSpectrumInfo(&infoS);		// now we have all spectrum info in infoW, we can use e.g. "infoS.Spectrum" to access the spectrum data array

	Serial.print(F("first element of spectrum = "));
	Serial.println(infoS.Spectrum[0], 6);
}

void loop() 
{
}

void PinReadyTriggerISR()
{
	// make sure to call this function when receiving a ready trigger from NSP32
	nsp32.OnPinReadyTriggered();
}
