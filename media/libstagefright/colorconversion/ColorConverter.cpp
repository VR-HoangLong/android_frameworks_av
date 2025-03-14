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

//#define LOG_NDEBUG 0
#define LOG_TAG "ColorConverter"
#include <utils/Log.h>

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/ColorConverter.h>
#include <media/stagefright/MediaErrors.h>

#include "libyuv/convert_from.h"
#include "libyuv/video_common.h"

#ifdef MTK_HARDWARE
#include <OMX_IVCommon.h>
#include <cutils/properties.h>
#include <DpBlitStream.h>
#include <stdio.h>
#include <utils/Timers.h>
#include <inttypes.h>

//static const int OMX_MTK_COLOR_FormatYV12 = 0x7F000200;
#endif

#define USE_LIBYUV

namespace android {

ColorConverter::ColorConverter(
        OMX_COLOR_FORMATTYPE from, OMX_COLOR_FORMATTYPE to)
    : mSrcFormat(from),
      mDstFormat(to),
      mClip(NULL) {
}

ColorConverter::~ColorConverter() {
    delete[] mClip;
    mClip = NULL;
}

bool ColorConverter::isValid() const {
#ifdef MTK_HARDWARE
ALOGD("***isValid() mSrcFormat=0x%x,mDstFormat=0x%x",mSrcFormat,mDstFormat);
    if ((mDstFormat != OMX_COLOR_Format16bitRGB565) &&
        (mDstFormat != OMX_COLOR_Format32bitARGB8888)) {
        return ERROR_UNSUPPORTED;
    }
    if ((mSrcFormat == OMX_COLOR_Format32bitARGB8888) ||
        (mSrcFormat == OMX_MTK_COLOR_FormatYV12))
        return true;
#endif
    switch (mSrcFormat) {
        case OMX_COLOR_FormatYUV420Planar:
            return mDstFormat == OMX_COLOR_Format16bitRGB565
                    || mDstFormat == OMX_COLOR_Format32BitRGBA8888
                    || mDstFormat == OMX_COLOR_Format32bitBGRA8888;

        case OMX_COLOR_FormatCbYCrY:
        case OMX_QCOM_COLOR_FormatYVU420SemiPlanar:
        case OMX_COLOR_FormatYUV420SemiPlanar:
        case OMX_TI_COLOR_FormatYUV420PackedSemiPlanar:
            return mDstFormat == OMX_COLOR_Format16bitRGB565;
#ifdef MTK_HARDWARE
        case OMX_COLOR_Format32bitARGB8888:
        case OMX_MTK_COLOR_FormatYV12:
        case OMX_COLOR_FormatVendorMTKYUV:
        case OMX_COLOR_FormatVendorMTKYUV_FCM:
            return true;
#endif

        default:
            return false;
    }
}

ColorConverter::BitmapParams::BitmapParams(
        void *bits,
        size_t width, size_t height,
        size_t cropLeft, size_t cropTop,
        size_t cropRight, size_t cropBottom,
        OMX_COLOR_FORMATTYPE colorFromat)
    : mBits(bits),
      mColorFormat(colorFromat),
      mWidth(width),
      mHeight(height),
      mCropLeft(cropLeft),
      mCropTop(cropTop),
      mCropRight(cropRight),
      mCropBottom(cropBottom) {
    switch(mColorFormat) {
    case OMX_COLOR_Format16bitRGB565:
        mBpp = 2;
        mStride = 2 * mWidth;
        break;

    case OMX_COLOR_Format32bitBGRA8888:
    case OMX_COLOR_Format32BitRGBA8888:
        mBpp = 4;
        mStride = 4 * mWidth;
        break;

    case OMX_COLOR_FormatYUV420Planar:
    case OMX_COLOR_FormatCbYCrY:
    case OMX_QCOM_COLOR_FormatYVU420SemiPlanar:
    case OMX_COLOR_FormatYUV420SemiPlanar:
    case OMX_TI_COLOR_FormatYUV420PackedSemiPlanar:
        mBpp = 1;
        mStride = mWidth;
        break;

    default:
        ALOGE("Unsupported color format %d", mColorFormat);
        mBpp = 1;
        mStride = mWidth;
        break;
    }
}

size_t ColorConverter::BitmapParams::cropWidth() const {
    return mCropRight - mCropLeft + 1;
}

size_t ColorConverter::BitmapParams::cropHeight() const {
    return mCropBottom - mCropTop + 1;
}

status_t ColorConverter::convert(
        const void *srcBits,
        size_t srcWidth, size_t srcHeight,
        size_t srcCropLeft, size_t srcCropTop,
        size_t srcCropRight, size_t srcCropBottom,
        void *dstBits,
        size_t dstWidth, size_t dstHeight,
        size_t dstCropLeft, size_t dstCropTop,
        size_t dstCropRight, size_t dstCropBottom) {
    BitmapParams src(
            const_cast<void *>(srcBits),
            srcWidth, srcHeight,
            srcCropLeft, srcCropTop, srcCropRight, srcCropBottom, mSrcFormat);

    BitmapParams dst(
            dstBits,
            dstWidth, dstHeight,
            dstCropLeft, dstCropTop, dstCropRight, dstCropBottom, mDstFormat);

    status_t err;

    switch (mSrcFormat) {
        case OMX_COLOR_FormatYUV420Planar:
#ifdef USE_LIBYUV
            err = convertYUV420PlanarUseLibYUV(src, dst);
#else
            err = convertYUV420Planar(src, dst);
#endif
            break;

        case OMX_COLOR_FormatCbYCrY:
            err = convertCbYCrY(src, dst);
            break;

        case OMX_QCOM_COLOR_FormatYVU420SemiPlanar:
            err = convertQCOMYUV420SemiPlanar(src, dst);
            break;

        case OMX_COLOR_FormatYUV420SemiPlanar:
            err = convertYUV420SemiPlanar(src, dst);
            break;

        case OMX_TI_COLOR_FormatYUV420PackedSemiPlanar:
            err = convertTIYUV420PackedSemiPlanar(src, dst);
            break;

        default:
        {
            CHECK(!"Should not be here. Unknown color conversion.");
            break;
        }
    }

    return err;
}

status_t ColorConverter::convertCbYCrY(
        const BitmapParams &src, const BitmapParams &dst) {
    // XXX Untested

    uint8_t *kAdjustedClip = initClip();

    if (!((src.mCropLeft & 1) == 0
        && src.cropWidth() == dst.cropWidth()
        && src.cropHeight() == dst.cropHeight())) {
        return ERROR_UNSUPPORTED;
    }

    uint16_t *dst_ptr = (uint16_t *)dst.mBits
        + dst.mCropTop * dst.mWidth + dst.mCropLeft;

    const uint8_t *src_ptr = (const uint8_t *)src.mBits
        + (src.mCropTop * dst.mWidth + src.mCropLeft) * 2;

    for (size_t y = 0; y < src.cropHeight(); ++y) {
        for (size_t x = 0; x < src.cropWidth(); x += 2) {
            signed y1 = (signed)src_ptr[2 * x + 1] - 16;
            signed y2 = (signed)src_ptr[2 * x + 3] - 16;
            signed u = (signed)src_ptr[2 * x] - 128;
            signed v = (signed)src_ptr[2 * x + 2] - 128;

            signed u_b = u * 517;
            signed u_g = -u * 100;
            signed v_g = -v * 208;
            signed v_r = v * 409;

            signed tmp1 = y1 * 298;
            signed b1 = (tmp1 + u_b) / 256;
            signed g1 = (tmp1 + v_g + u_g) / 256;
            signed r1 = (tmp1 + v_r) / 256;

            signed tmp2 = y2 * 298;
            signed b2 = (tmp2 + u_b) / 256;
            signed g2 = (tmp2 + v_g + u_g) / 256;
            signed r2 = (tmp2 + v_r) / 256;

            uint32_t rgb1 =
                ((kAdjustedClip[r1] >> 3) << 11)
                | ((kAdjustedClip[g1] >> 2) << 5)
                | (kAdjustedClip[b1] >> 3);

            uint32_t rgb2 =
                ((kAdjustedClip[r2] >> 3) << 11)
                | ((kAdjustedClip[g2] >> 2) << 5)
                | (kAdjustedClip[b2] >> 3);

            if (x + 1 < src.cropWidth()) {
                *(uint32_t *)(&dst_ptr[x]) = (rgb2 << 16) | rgb1;
            } else {
                dst_ptr[x] = rgb1;
            }
        }

        src_ptr += src.mWidth * 2;
        dst_ptr += dst.mWidth;
    }

    return OK;
}

status_t ColorConverter::convertYUV420PlanarUseLibYUV(
        const BitmapParams &src, const BitmapParams &dst) {
    if (!((src.mCropLeft & 1) == 0
            && src.cropWidth() == dst.cropWidth()
            && src.cropHeight() == dst.cropHeight())) {
        return ERROR_UNSUPPORTED;
    }

    uint8_t *dst_ptr = (uint8_t *)dst.mBits
        + dst.mCropTop * dst.mStride + dst.mCropLeft * dst.mBpp;

    const uint8_t *src_y =
        (const uint8_t *)src.mBits + src.mCropTop * src.mStride + src.mCropLeft;

    const uint8_t *src_u =
        (const uint8_t *)src.mBits + src.mStride * src.mHeight
        + (src.mCropTop / 2) * (src.mStride / 2) + (src.mCropLeft / 2);

    const uint8_t *src_v =
        src_u + (src.mStride / 2) * (src.mHeight / 2);

    switch (mDstFormat) {
    case OMX_COLOR_Format16bitRGB565:
        libyuv::I420ToRGB565(src_y, src.mStride, src_u, src.mStride / 2, src_v, src.mStride / 2,
                (uint8 *)dst_ptr, dst.mStride, src.cropWidth(), src.cropHeight());
        break;

    case OMX_COLOR_Format32BitRGBA8888:
        libyuv::ConvertFromI420(src_y, src.mStride, src_u, src.mStride / 2, src_v, src.mStride / 2,
                (uint8 *)dst_ptr, dst.mStride, src.cropWidth(), src.cropHeight(), libyuv::FOURCC_ABGR);
        break;

    case OMX_COLOR_Format32bitBGRA8888:
        libyuv::ConvertFromI420(src_y, src.mStride, src_u, src.mStride / 2, src_v, src.mStride / 2,
                (uint8 *)dst_ptr, dst.mStride, src.cropWidth(), src.cropHeight(), libyuv::FOURCC_ARGB);
        break;

    default:
        return ERROR_UNSUPPORTED;
    }

    return OK;
}

void ColorConverter::writeToDst(
        void *dst_ptr, uint8_t *kAdjustedClip, bool uncropped,
        signed r1, signed g1, signed b1,
        signed r2, signed g2, signed b2) {
    switch (mDstFormat) {
    case OMX_COLOR_Format16bitRGB565:
    {
        uint32_t rgb1 =
            ((kAdjustedClip[r1] >> 3) << 11)
            | ((kAdjustedClip[g1] >> 2) << 5)
            | (kAdjustedClip[b1] >> 3);

        if (uncropped) {
            uint32_t rgb2 =
                ((kAdjustedClip[r2] >> 3) << 11)
                | ((kAdjustedClip[g2] >> 2) << 5)
                | (kAdjustedClip[b2] >> 3);

            *(uint32_t *)dst_ptr = (rgb2 << 16) | rgb1;
        } else {
            *(uint16_t *)dst_ptr = rgb1;
        }
        break;
    }
    case OMX_COLOR_Format32BitRGBA8888:
    {
        ((uint32_t *)dst_ptr)[0] =
                (kAdjustedClip[r1])
                | (kAdjustedClip[g1] << 8)
                | (kAdjustedClip[b1] << 16)
                | (0xFF << 24);

        if (uncropped) {
            ((uint32_t *)dst_ptr)[1] =
                    (kAdjustedClip[r2])
                    | (kAdjustedClip[g2] << 8)
                    | (kAdjustedClip[b2] << 16)
                    | (0xFF << 24);
        }
        break;
    }
    case OMX_COLOR_Format32bitBGRA8888:
    {
        ((uint32_t *)dst_ptr)[0] =
                (kAdjustedClip[b1])
                | (kAdjustedClip[g1] << 8)
                | (kAdjustedClip[r1] << 16)
                | (0xFF << 24);

        if (uncropped) {
            ((uint32_t *)dst_ptr)[1] =
                    (kAdjustedClip[b2])
                    | (kAdjustedClip[g2] << 8)
                    | (kAdjustedClip[r2] << 16)
                    | (0xFF << 24);
        }
        break;
    }
    default:
        break;
    }
}
status_t ColorConverter::convertYUV420Planar(
        const BitmapParams &src, const BitmapParams &dst) {
    if (!((src.mCropLeft & 1) == 0
            && src.cropWidth() == dst.cropWidth()
            && src.cropHeight() == dst.cropHeight())) {
        return ERROR_UNSUPPORTED;
    }

    uint8_t *kAdjustedClip = initClip();

    uint8_t *dst_ptr = (uint8_t *)dst.mBits
        + dst.mCropTop * dst.mStride + dst.mCropLeft * dst.mBpp;

    const uint8_t *src_y =
        (const uint8_t *)src.mBits + src.mCropTop * src.mStride + src.mCropLeft;

    const uint8_t *src_u =
        (const uint8_t *)src.mBits + src.mStride * src.mHeight
        + (src.mCropTop / 2) * (src.mStride / 2) + src.mCropLeft / 2;

    const uint8_t *src_v =
        src_u + (src.mStride / 2) * (src.mHeight / 2);

    for (size_t y = 0; y < src.cropHeight(); ++y) {
        for (size_t x = 0; x < src.cropWidth(); x += 2) {
            // B = 1.164 * (Y - 16) + 2.018 * (U - 128)
            // G = 1.164 * (Y - 16) - 0.813 * (V - 128) - 0.391 * (U - 128)
            // R = 1.164 * (Y - 16) + 1.596 * (V - 128)

            // B = 298/256 * (Y - 16) + 517/256 * (U - 128)
            // G = .................. - 208/256 * (V - 128) - 100/256 * (U - 128)
            // R = .................. + 409/256 * (V - 128)

            // min_B = (298 * (- 16) + 517 * (- 128)) / 256 = -277
            // min_G = (298 * (- 16) - 208 * (255 - 128) - 100 * (255 - 128)) / 256 = -172
            // min_R = (298 * (- 16) + 409 * (- 128)) / 256 = -223

            // max_B = (298 * (255 - 16) + 517 * (255 - 128)) / 256 = 534
            // max_G = (298 * (255 - 16) - 208 * (- 128) - 100 * (- 128)) / 256 = 432
            // max_R = (298 * (255 - 16) + 409 * (255 - 128)) / 256 = 481

            // clip range -278 .. 535

            signed y1 = (signed)src_y[x] - 16;
            signed y2 = (signed)src_y[x + 1] - 16;

            signed u = (signed)src_u[x / 2] - 128;
            signed v = (signed)src_v[x / 2] - 128;

            signed u_b = u * 517;
            signed u_g = -u * 100;
            signed v_g = -v * 208;
            signed v_r = v * 409;

            signed tmp1 = y1 * 298;
            signed b1 = (tmp1 + u_b) / 256;
            signed g1 = (tmp1 + v_g + u_g) / 256;
            signed r1 = (tmp1 + v_r) / 256;

            signed tmp2 = y2 * 298;
            signed b2 = (tmp2 + u_b) / 256;
            signed g2 = (tmp2 + v_g + u_g) / 256;
            signed r2 = (tmp2 + v_r) / 256;

            bool uncropped = x + 1 < src.cropWidth();
            (void)writeToDst(dst_ptr + x * dst.mBpp,
                    kAdjustedClip, uncropped, r1, g1, b1, r2, g2, b2);
        }

        src_y += src.mStride;

        if (y & 1) {
            src_u += src.mStride / 2;
            src_v += src.mStride / 2;
        }

        dst_ptr += dst.mStride;
    }

    return OK;
}

status_t ColorConverter::convertQCOMYUV420SemiPlanar(
        const BitmapParams &src, const BitmapParams &dst) {
    uint8_t *kAdjustedClip = initClip();

    if (!((src.mCropLeft & 1) == 0
            && src.cropWidth() == dst.cropWidth()
            && src.cropHeight() == dst.cropHeight())) {
        return ERROR_UNSUPPORTED;
    }

    uint16_t *dst_ptr = (uint16_t *)dst.mBits
        + dst.mCropTop * dst.mWidth + dst.mCropLeft;

    const uint8_t *src_y =
        (const uint8_t *)src.mBits + src.mCropTop * src.mWidth + src.mCropLeft;

    const uint8_t *src_u =
        (const uint8_t *)src_y + src.mWidth * src.mHeight
        + src.mCropTop * src.mWidth + src.mCropLeft;

    for (size_t y = 0; y < src.cropHeight(); ++y) {
        for (size_t x = 0; x < src.cropWidth(); x += 2) {
            signed y1 = (signed)src_y[x] - 16;
            signed y2 = (signed)src_y[x + 1] - 16;

            signed u = (signed)src_u[x & ~1] - 128;
            signed v = (signed)src_u[(x & ~1) + 1] - 128;

            signed u_b = u * 517;
            signed u_g = -u * 100;
            signed v_g = -v * 208;
            signed v_r = v * 409;

            signed tmp1 = y1 * 298;
            signed b1 = (tmp1 + u_b) / 256;
            signed g1 = (tmp1 + v_g + u_g) / 256;
            signed r1 = (tmp1 + v_r) / 256;

            signed tmp2 = y2 * 298;
            signed b2 = (tmp2 + u_b) / 256;
            signed g2 = (tmp2 + v_g + u_g) / 256;
            signed r2 = (tmp2 + v_r) / 256;

            uint32_t rgb1 =
                ((kAdjustedClip[b1] >> 3) << 11)
                | ((kAdjustedClip[g1] >> 2) << 5)
                | (kAdjustedClip[r1] >> 3);

            uint32_t rgb2 =
                ((kAdjustedClip[b2] >> 3) << 11)
                | ((kAdjustedClip[g2] >> 2) << 5)
                | (kAdjustedClip[r2] >> 3);

            if (x + 1 < src.cropWidth()) {
                *(uint32_t *)(&dst_ptr[x]) = (rgb2 << 16) | rgb1;
            } else {
                dst_ptr[x] = rgb1;
            }
        }

        src_y += src.mWidth;

        if (y & 1) {
            src_u += src.mWidth;
        }

        dst_ptr += dst.mWidth;
    }

    return OK;
}

status_t ColorConverter::convertYUV420SemiPlanar(
        const BitmapParams &src, const BitmapParams &dst) {
    // XXX Untested

    uint8_t *kAdjustedClip = initClip();

    if (!((src.mCropLeft & 1) == 0
            && src.cropWidth() == dst.cropWidth()
            && src.cropHeight() == dst.cropHeight())) {
        return ERROR_UNSUPPORTED;
    }

    uint16_t *dst_ptr = (uint16_t *)dst.mBits
        + dst.mCropTop * dst.mWidth + dst.mCropLeft;

    const uint8_t *src_y =
        (const uint8_t *)src.mBits + src.mCropTop * src.mWidth + src.mCropLeft;

    const uint8_t *src_u =
        (const uint8_t *)src_y + src.mWidth * src.mHeight
        + src.mCropTop * src.mWidth + src.mCropLeft;

    for (size_t y = 0; y < src.cropHeight(); ++y) {
        for (size_t x = 0; x < src.cropWidth(); x += 2) {
            signed y1 = (signed)src_y[x] - 16;
            signed y2 = (signed)src_y[x + 1] - 16;

            signed v = (signed)src_u[x & ~1] - 128;
            signed u = (signed)src_u[(x & ~1) + 1] - 128;

            signed u_b = u * 517;
            signed u_g = -u * 100;
            signed v_g = -v * 208;
            signed v_r = v * 409;

            signed tmp1 = y1 * 298;
            signed b1 = (tmp1 + u_b) / 256;
            signed g1 = (tmp1 + v_g + u_g) / 256;
            signed r1 = (tmp1 + v_r) / 256;

            signed tmp2 = y2 * 298;
            signed b2 = (tmp2 + u_b) / 256;
            signed g2 = (tmp2 + v_g + u_g) / 256;
            signed r2 = (tmp2 + v_r) / 256;

            uint32_t rgb1 =
                ((kAdjustedClip[b1] >> 3) << 11)
                | ((kAdjustedClip[g1] >> 2) << 5)
                | (kAdjustedClip[r1] >> 3);

            uint32_t rgb2 =
                ((kAdjustedClip[b2] >> 3) << 11)
                | ((kAdjustedClip[g2] >> 2) << 5)
                | (kAdjustedClip[r2] >> 3);

            if (x + 1 < src.cropWidth()) {
                *(uint32_t *)(&dst_ptr[x]) = (rgb2 << 16) | rgb1;
            } else {
                dst_ptr[x] = rgb1;
            }
        }

        src_y += src.mWidth;

        if (y & 1) {
            src_u += src.mWidth;
        }

        dst_ptr += dst.mWidth;
    }

    return OK;
}

status_t ColorConverter::convertTIYUV420PackedSemiPlanar(
        const BitmapParams &src, const BitmapParams &dst) {
    uint8_t *kAdjustedClip = initClip();

    if (!((src.mCropLeft & 1) == 0
            && src.cropWidth() == dst.cropWidth()
            && src.cropHeight() == dst.cropHeight())) {
        return ERROR_UNSUPPORTED;
    }

    uint16_t *dst_ptr = (uint16_t *)dst.mBits
        + dst.mCropTop * dst.mWidth + dst.mCropLeft;

    const uint8_t *src_y = (const uint8_t *)src.mBits;

    const uint8_t *src_u =
        (const uint8_t *)src_y + src.mWidth * (src.mHeight - src.mCropTop / 2);

    for (size_t y = 0; y < src.cropHeight(); ++y) {
        for (size_t x = 0; x < src.cropWidth(); x += 2) {
            signed y1 = (signed)src_y[x] - 16;
            signed y2 = (signed)src_y[x + 1] - 16;

            signed u = (signed)src_u[x & ~1] - 128;
            signed v = (signed)src_u[(x & ~1) + 1] - 128;

            signed u_b = u * 517;
            signed u_g = -u * 100;
            signed v_g = -v * 208;
            signed v_r = v * 409;

            signed tmp1 = y1 * 298;
            signed b1 = (tmp1 + u_b) / 256;
            signed g1 = (tmp1 + v_g + u_g) / 256;
            signed r1 = (tmp1 + v_r) / 256;

            signed tmp2 = y2 * 298;
            signed b2 = (tmp2 + u_b) / 256;
            signed g2 = (tmp2 + v_g + u_g) / 256;
            signed r2 = (tmp2 + v_r) / 256;

            uint32_t rgb1 =
                ((kAdjustedClip[r1] >> 3) << 11)
                | ((kAdjustedClip[g1] >> 2) << 5)
                | (kAdjustedClip[b1] >> 3);

            uint32_t rgb2 =
                ((kAdjustedClip[r2] >> 3) << 11)
                | ((kAdjustedClip[g2] >> 2) << 5)
                | (kAdjustedClip[b2] >> 3);

            if (x + 1 < src.cropWidth()) {
                *(uint32_t *)(&dst_ptr[x]) = (rgb2 << 16) | rgb1;
            } else {
                dst_ptr[x] = rgb1;
            }
        }

        src_y += src.mWidth;

        if (y & 1) {
            src_u += src.mWidth;
        }

        dst_ptr += dst.mWidth;
    }

    return OK;
}

uint8_t *ColorConverter::initClip() {
    static const signed kClipMin = -278;
    static const signed kClipMax = 535;

    if (mClip == NULL) {
        mClip = new uint8_t[kClipMax - kClipMin + 1];

        for (signed i = kClipMin; i <= kClipMax; ++i) {
            mClip[i - kClipMin] = (i < 0) ? 0 : (i > 255) ? 255 : (uint8_t)i;
        }
    }

    return &mClip[-kClipMin];
}

#ifdef MTK_HARDWARE
status_t ColorConverter::convertYUVToRGBHW(const BitmapParams &src, const BitmapParams &dst)
{
    ALOGD("srcWidth(%zu), srcHeight(%zu), srcCropLeft(%zu), srcCropTop(%zu), srcCropRight(%zu), srcCropBottom(%zu)",
       src.mWidth, src.mHeight, src.mCropLeft, src.mCropTop, src.mCropRight, src.mCropBottom);
    ALOGD("dstWidth(%zu), dstHeight(%zu), dstCropLeft(%zu), dstCropTop(%zu), dstCropRight(%zu), dstCropBottom(%zu)",
       dst.mWidth, dst.mHeight, dst.mCropLeft, dst.mCropTop, dst.mCropRight, dst.mCropBottom);
    DpBlitStream *blitStream = new DpBlitStream();
    //int srcWidth = src.cropWidth();
    //int srcHeight = src.cropHeight();
    unsigned int srcWStride = src.mWidth;
    unsigned int srcHStride = src.mHeight;

    DpRect srcRoi;
    srcRoi.x = 0;
    srcRoi.y = 0;
    srcRoi.w = dst.mWidth;
    srcRoi.h = dst.mHeight;

    ALOGD("src stride aligned, w(%d), h(%d)", srcWStride, srcHStride);

    unsigned int dstWStride = dst.mWidth ;
    unsigned int dstHStride = dst.mHeight ;
    char name_yuv[100];
    char retriever_yuv_propty[100];
    char name_rgb[100];
    char retriever_propty_rgb[100];

    if (mSrcFormat == OMX_COLOR_FormatYUV420Planar) {
    char* planar[3];
    unsigned int length[3];
    planar[0] = (char*)src.mBits;
    length[0] = srcWStride*srcHStride;
    planar[1] = planar[0] + length[0];
    length[1] = srcWStride*srcHStride/4;
    planar[2] = planar[1] + length[1];
    length[2] = length[1];
    ALOGD("Yaddr(%p), Uaddr(%p), Vaddr(%p) YUV420P", planar[0], planar[1], planar[2]);
    ALOGD("Ylen(%d), Ulen(%d), Vlen(%d)", length[0], length[1], length[2]);

    blitStream->setSrcBuffer((void**)planar, (unsigned int*)length, 3);
    blitStream->setSrcConfig(srcWStride, srcHStride, eYUV_420_3P, eInterlace_None, &srcRoi);
    }
    else if (mSrcFormat == OMX_MTK_COLOR_FormatYV12) {
        char* planar[3];
        unsigned int length[3];
        planar[0] = (char*)src.mBits;
        length[0] = srcWStride*srcHStride;
        planar[1] = planar[0] + length[0];
        length[1] = (((srcWStride>>1)+0xf) & (~0xf))*srcHStride/2;
        planar[2] = planar[1] + length[1];
        length[2] = length[1];
        ALOGD("Yaddr(%p), Uaddr(%p), Vaddr(%p) YV12", planar[0], planar[1], planar[2]);
        ALOGD("Ylen(%d), Ulen(%d), Vlen(%d)", length[0], length[1], length[2]);

        blitStream->setSrcBuffer((void**)planar, (unsigned int*)length, 3);
        //blitStream->setSrcConfig(srcWStride, srcHStride, eYV12, eInterlace_None, &srcRoi);
        blitStream->setSrcConfig(srcWStride, srcHStride, srcWStride, (((srcWStride>>1)+0xf) & (~0xf)), eYV12, DP_PROFILE_BT601, eInterlace_None, &srcRoi);
    }
    else if (mSrcFormat == OMX_COLOR_Format32bitARGB8888) {
        char* planar[1];
        unsigned int length[1];
        planar[0] = (char*)src.mBits;
        length[0] = srcWStride*srcHStride*4;
        blitStream->setSrcBuffer((void**)planar, (unsigned int*)length, 1);
        blitStream->setSrcConfig(srcWStride, srcHStride, eRGBA8888, eInterlace_None, &srcRoi);
    }
    else if (mSrcFormat == OMX_COLOR_FormatVendorMTKYUV) {
        char* planar[2];
        unsigned int length[2];
        planar[0] = (char*)src.mBits;
        length[0] = srcWStride*srcHStride;
        planar[1] = planar[0] + length[0];
        length[1] = srcWStride*srcHStride/2;
        ALOGD("Yaddr(%p), Caddr(%p)", planar[0], planar[1]);
        ALOGD("Ylen(%d), Clen(%d)", length[0], length[1]);

        blitStream->setSrcBuffer((void**)planar, (unsigned int*)length, 2);
        //blitStream->setSrcConfig(srcWStride, srcHStride, eNV12_BLK, eInterlace_None, &srcRoi);
        blitStream->setSrcConfig(srcWStride, srcHStride, srcWStride * 32, srcWStride * 16, eNV12_BLK, DP_PROFILE_BT601, eInterlace_None, &srcRoi);
    }
    else if (mSrcFormat == OMX_COLOR_FormatVendorMTKYUV_FCM) {
        char* planar[2];
        unsigned int length[2];
        planar[0] = (char*)src.mBits;
        length[0] = srcWStride*srcHStride;
        planar[1] = planar[0] + length[0];
        length[1] = srcWStride*srcHStride/2;
        ALOGD("Yaddr(%p), Caddr(%p)", planar[0], planar[1]);
        ALOGD("Ylen(%d), Clen(%d)", length[0], length[1]);

        blitStream->setSrcBuffer((void**)planar, (unsigned int*)length, 2);
        //blitStream->setSrcConfig(srcWStride, srcHStride, eNV12_BLK_FCM, eInterlace_None, &srcRoi);
        blitStream->setSrcConfig(srcWStride, srcHStride, srcWStride * 32, srcWStride * 16, eNV12_BLK_FCM, DP_PROFILE_BT601, eInterlace_None, &srcRoi);
    }
    ALOGD("dst addr(%p), w(%d), h(%d)", dst.mBits, dstWStride, dstHStride);
    if (mDstFormat == OMX_COLOR_Format16bitRGB565) {
        blitStream->setDstBuffer(dst.mBits, dst.mWidth * dst.mHeight * 2);
        blitStream->setDstConfig(dst.mWidth, dst.mHeight, eRGB565);
    }
    else if (mDstFormat == OMX_COLOR_Format32bitARGB8888) {
        blitStream->setDstBuffer(dst.mBits, dst.mWidth * dst.mHeight * 4);
        //blitStream->setDstConfig(dst.mWidth, dst.mHeight, eARGB8888);
        blitStream->setDstConfig(dst.mWidth, dst.mHeight, eRGBA8888);
    }

    sprintf(name_yuv, "/sdcard/retriever_%" PRId64 "_%zu_%zu.yuv",systemTime(),src.mWidth,src.mHeight);
    sprintf(retriever_yuv_propty, "retriever.dump.yuv");
    dumpColorConverterData(name_yuv,src.mBits,(src.mWidth*src.mHeight)*2,retriever_yuv_propty);

    //Add Sharpness in Video Thumbnail
    blitStream->setTdshp(1);
    bool bRet = blitStream->invalidate();
    ALOGI("blitStream return %d.", bRet);

    sprintf(name_rgb, "/sdcard/retriever_%" PRId64 "_%zu_%zu.rgb",systemTime(),dst.mWidth,dst.mHeight);
    sprintf(retriever_propty_rgb, "retriever.dump.rgb");
    if (mDstFormat == OMX_COLOR_Format16bitRGB565){
        dumpColorConverterData(name_rgb,dst.mBits, dst.mWidth*dst.mHeight*2, retriever_propty_rgb);
    }else if(mDstFormat == OMX_COLOR_Format32bitARGB8888){
        dumpColorConverterData(name_rgb,dst.mBits, dst.mWidth*dst.mHeight*4, retriever_propty_rgb);
    }

    if(!bRet)
        return OK;
    else
        return UNKNOWN_ERROR;

    return OK;
}

void ColorConverter::dumpColorConverterData(const char * filepath, const void * buffer, size_t size,const char * propty) {

    char value[PROPERTY_VALUE_MAX];
    property_get(propty, value, "0");
    int bflag=atoi(value);

    if (bflag) {
       FILE * fp= fopen (filepath, "w");
       if (fp!=NULL) {
            fwrite(buffer,size,1,fp);
            fclose(fp);
       } else {
            ALOGV("dump %s fail",propty);
       }
    }
}
#endif

}  // namespace android
