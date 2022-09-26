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

#ifndef __NANOLAMBDA_NSP32_SPECTRUM_INFO_H
#define __NANOLAMBDA_NSP32_SPECTRUM_INFO_H

#include <stdint.h>

/**
 * spectrum info struct
 */
typedef struct SpectrumInfoStruct
{
	uint16_t IntegrationTime;	/**< integration time */
	bool IsSaturated;			/**< saturation flag */
	uint32_t NumOfPoints;		/**< num of points */
	float* Spectrum;			/**< spectrum data array */
	float X;					/**< X */
	float Y;					/**< Y */
	float Z;					/**< Z */
} SpectrumInfo;

#endif
