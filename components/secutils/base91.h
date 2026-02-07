/*
 * Copyright (c) 2000-2006 Joachim Henke
 *
 * For conditions of distribution and use, see copyright notice in base91.c
 */

#ifndef BASE91_H
#define BASE91_H 1

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
 extern "C" {
#endif

size_t encodeBase91(const void *, void *, size_t);

size_t decodeBase91(const void *, void *, size_t);

#ifdef __cplusplus
}
#endif

#endif	/* base91.h */
