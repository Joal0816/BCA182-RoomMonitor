#ifndef _STDINT_H
#define _STDINT_H
typedef signed char int8_t; typedef unsigned char uint8_t;
typedef short int16_t; typedef unsigned short uint16_t;
typedef int int32_t; typedef unsigned int uint32_t;
typedef long long int64_t; typedef unsigned long long uint64_t;
typedef long intptr_t; typedef unsigned long uintptr_t;
typedef long intmax_t; typedef unsigned long uintmax_t;
typedef int int_fast8_t; typedef unsigned int uint_fast8_t;
#define UINT32_MAX 0xFFFFFFFFU
#define INT32_MAX  0x7FFFFFFF
#define NULL ((void*)0)
#endif
