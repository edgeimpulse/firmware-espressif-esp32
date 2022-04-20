//
// JPEG Encoder
//
// written by Larry Bank
// bitbank@pobox.com
// Arduino port started 7/22/2021
// Original JPEG code written 20+ years ago :)
// The goal of this code is to encode JPEG images on embedded systems
//
// Copyright 2021 BitBank Software, Inc. All Rights Reserved.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//===========================================================================
//
#include "JPEGENC.h"

// Include the C code which does the actual work
#include "jpeg.h"

//
// File (SD/MMC) based initialization
//
int JPEGClass::open(const char *szFilename, JPEG_OPEN_CALLBACK *pfnOpen, JPEG_CLOSE_CALLBACK *pfnClose, JPEG_READ_CALLBACK *pfnRead, JPEG_WRITE_CALLBACK *pfnWrite, JPEG_SEEK_CALLBACK *pfnSeek)
{
    memset(&_jpeg, 0, sizeof(JPEGIMAGE));
    _jpeg.pfnRead = pfnRead;
    _jpeg.pfnWrite = pfnWrite;
    _jpeg.pfnSeek = pfnSeek;
    _jpeg.pfnOpen = pfnOpen;
    _jpeg.pfnClose = pfnClose;
    _jpeg.JPEGFile.fHandle = (*pfnOpen)(szFilename);
    _jpeg.pHighWater = &_jpeg.ucFileBuf[JPEG_FILE_BUF_SIZE - 512];
    if (_jpeg.JPEGFile.fHandle == NULL) {
        _jpeg.iError = JPEG_INVALID_FILE;
       return JPEG_INVALID_FILE;
    }
    return JPEG_SUCCESS;

} /* open() */

int JPEGClass::open(uint8_t *pOutput, int iBufferSize)
{
    memset(&_jpeg, 0, sizeof(JPEGIMAGE));
    _jpeg.pOutput = pOutput;
    _jpeg.iBufferSize = iBufferSize;
    _jpeg.pHighWater = &pOutput[iBufferSize - 512];

    return JPEG_SUCCESS;
} /* open() */

//
// return the last error (if any)
//
int JPEGClass::getLastError()
{
    return _jpeg.iError;
} /* getLastError() */
//
// Close the file - not needed when decoding from memory
//
int JPEGClass::close()
{
    JPEGEncodeEnd(&_jpeg);
    if (_jpeg.pfnClose)
        (*_jpeg.pfnClose)(&_jpeg.JPEGFile);
    return _jpeg.iDataSize;
} /* close() */

int JPEGClass::encodeBegin(JPEGENCODE *pEncode, int iWidth, int iHeight, uint8_t ucPixelType, uint8_t ucSubSample, uint8_t ucQFactor)
{
    return JPEGEncodeBegin(&_jpeg, pEncode, iWidth, iHeight, ucPixelType, ucSubSample, ucQFactor);
} /* encodeBegin() */

int JPEGClass::addMCU(JPEGENCODE *pEncode, uint8_t *pPixels, int iPitch)
{
    return JPEGAddMCU(&_jpeg, pEncode, pPixels, iPitch);
} /* addMCU() */
