/*************************************************************************************\
* Copyright (C) CEVA(R) Inc. All rights reserved                                      *
*                                                                                     *
* This information embodies materials and concepts, which are proprietary and         *
* confidential to CEVA Inc., and is made available solely pursuant to the terms       *
* of a written license agreement, or NDA, or another written agreement, as            *
* applicable ("CEVA Agreement"), with CEVA Inc. or any of its subsidiaries ("CEVA").  *
*                                                                                     *
* This information can be used only with the written permission from CEVA, in         *
* accordance with the terms and conditions stipulated in the CEVA Agreement, under    *
* which the information has been supplied and solely as expressly permitted for       *
* the purpose specified in the CEVA Agreement.                                        *
*                                                                                     *
* This information is made available exclusively to licensees or parties that have    *
* received express written authorization from CEVA to download or receive the         *
* information and have agreed to the terms and conditions of the CEVA Agreement.      *
*                                                                                     *
* IF YOU HAVE NOT RECEIVED SUCH EXPRESS AUTHORIZATION AND AGREED TO THE CEVA          *
* AGREEMENT, YOU MAY NOT DOWNLOAD, INSTALL OR USE THIS INFORMATION.                   *
*                                                                                     *
* The information contained in this document is subject to change without notice      *
* and does not represent a commitment on any part of CEVA. Unless specifically        *
* agreed otherwise in the CEVA Agreement, CEVA make no warranty of any kind with      *
* regard to this material, including, but not limited to implied warranties of        *
* merchantability and fitness for a particular purpose whether arising out of law,    *
* custom, conduct or otherwise.                                                       *
*                                                                                     *
* While the information contained herein is assumed to be accurate, CEVA assumes no   *
* responsibility for any errors or omissions contained herein, and assumes no         *
* liability for special, direct, indirect or consequential damage, losses, costs,     *
* charges, claims, demands, fees or expenses, of any nature or kind, which are        *
* incurred in connection with the furnishing, performance or use of this material.    *
*                                                                                     *
* This document contains proprietary information, which is protected by U.S. and      *
* international copyright laws. All rights reserved. No part of this document may     *
* be reproduced, photocopied, or translated into another language without the prior   *
* written consent of CEVA.                                                            *
\*************************************************************************************/

#ifndef _CEVA_TYPEDEF_H
#define _CEVA_TYPEDEF_H


#define SZF_CMPX    2


#define real        re
#define imag        im

typedef long long			int64;
typedef int					int32;
typedef short				int16;
typedef	char				int8;

typedef unsigned long long	uint64;
typedef unsigned int		uint32;
typedef unsigned short		uint16;
typedef unsigned char		uint8;

typedef long long	acc_t;

typedef float float_32;


//complex types
typedef struct
{
	int8 re;
	int8 im;
} cint8;

typedef struct
{
	uint8 re;
	uint8 im;
} cuint8;

typedef struct
{
	int16 re;
	int16 im;
} cint16;


typedef struct
{
	int32 re;
	int32 im;
} cint32;

typedef struct
{
	int64 re;
	int64 im;
} cint64;



typedef struct
{
	float re;
	float im;
} cfloat;

typedef struct
{
	double re;
	double im;
} cdouble;



#ifndef MAX_64
#define MAX_64 (int64)0x7fffffffffffffffLL
#endif

#ifndef MIN_64
#define MIN_64 (int64)0x8000000000000000LL
#endif

#ifndef MAX_32
#define MAX_32 (int32)0x7fffffffL
#endif


#ifndef MIN_32
#define MIN_32 (int32)0x80000000L
#endif

#ifndef MAX_16
#define MAX_16 (int16)0x7fff
#endif

#ifndef MIN_16
#define MIN_16 (int16)0x8000
#endif

float CEVA_DSP_LIB_FLOAT_ISQRT_OOB(float arg_in);

#endif // _CEVA_TYPEDEF_H
