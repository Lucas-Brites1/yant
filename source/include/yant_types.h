#pragma once
#ifndef YANT_TYPES_H
#define YANT_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

#define nil ((void*)0)
#define returns(something) return something;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef float  f32;
typedef double f64;

typedef size_t   usize;
typedef ptrdiff_t isize;

typedef unsigned char byte;

#endif /* YANT_TYPES_H */
