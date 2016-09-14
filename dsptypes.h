/*
 * This file is part of NRAConnector
 * written by Christian Daniel 2016 -- <dg2ndk@afuz.org>
 *
 * The MIT License (MIT)
 * Copyright (c) 2016 Amateurfunk Unterfranken e.V.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * File contents: Basic type definitions for DSP functions
 */

#ifndef INCLUDE_DSPTYPES_H
#define INCLUDE_DSPTYPES_H

#include <QtGlobal>
#include <complex>

#pragma pack(push, 1)
template <class T> struct IQSample {
	T i;
	T q;
} __attribute__((packed));
typedef IQSample<qint16> IQSampleS16;
#pragma pack(pop)

typedef float Real;
typedef std::complex<Real> Complex;

#endif // INCLUDE_DSPTYPES_H
