/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COLOR_CONVERTER_H_

#define COLOR_CONVERTER_H_

#include <sys/types.h>

#include <stdint.h>
#include <utils/Errors.h>

#include <OMX_Video.h>

namespace android {

struct ColorConverter {
    ColorConverter(OMX_COLOR_FORMATTYPE from, OMX_COLOR_FORMATTYPE to);
    ~ColorConverter();

    bool isValid() const;

    status_t convert(
            const void *srcBits,
            size_t srcWidth, size_t srcHeight,
            size_t srcCropLeft, size_t srcCropTop,
            size_t srcCropRight, size_t srcCropBottom,
            void *dstBits,
            size_t dstWidth, size_t dstHeight,
            size_t dstCropLeft, size_t dstCropTop,
            size_t dstCropRight, size_t dstCropBottom);

private:
    struct BitmapParams {
        BitmapParams(
                void *bits,
                size_t width, size_t height,
                size_t cropLeft, size_t cropTop,
                size_t cropRight, size_t cropBottom,
                OMX_COLOR_FORMATTYPE colorFromat);

        size_t cropWidth() const;
        size_t cropHeight() const;

        void *mBits;
        OMX_COLOR_FORMATTYPE mColorFormat;
        size_t mWidth, mHeight;
        size_t mCropLeft, mCropTop, mCropRight, mCropBottom;
        size_t mBpp, mStride;
    };

    OMX_COLOR_FORMATTYPE mSrcFormat, mDstFormat;
    uint8_t *mClip;

    uint8_t *initClip();

    status_t convertCbYCrY(
            const BitmapParams &src, const BitmapParams &dst);

    status_t convertYUV420Planar(
            const BitmapParams &src, const BitmapParams &dst);

    status_t convertYUV420PlanarUseLibYUV(
            const BitmapParams &src, const BitmapParams &dst);

    status_t convertQCOMYUV420SemiPlanar(
            const BitmapParams &src, const BitmapParams &dst);

    status_t convertYUV420SemiPlanar(
            const BitmapParams &src, const BitmapParams &dst);

    status_t convertTIYUV420PackedSemiPlanar(
            const BitmapParams &src, const BitmapParams &dst);

    void writeToDst(void *dst_ptr, uint8_t *kAdjustedClip, bool uncropped,
            signed r1, signed g1, signed b1,
            signed r2, signed g2, signed b2);

    ColorConverter(const ColorConverter &);
    ColorConverter &operator=(const ColorConverter &);
#ifdef MTK_HARDWARE
    status_t convertYUVToRGBHW(
	const BitmapParams &src, const BitmapParams &dst);
    void dumpColorConverterData(const char * filepath,
	const void * buffer, size_t size, const char * propty);
#endif
};

}  // namespace android

#endif  // COLOR_CONVERTER_H_
