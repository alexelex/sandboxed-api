#include "sandboxed_api/sandbox2/util/fileops.h"
#include "sandboxed_api/sandbox2/util/path.h"
#include "sandboxed_api/vars.h"

#define PNG_FORMAT_FLAG_ALPHA 0x01U    /* format with an alpha channel */
#define PNG_FORMAT_FLAG_COLOR 0x02U    /* color format: otherwise grayscale */
#define PNG_FORMAT_FLAG_LINEAR 0x04U   /* 2-byte channels else 1-byte */
#define PNG_FORMAT_FLAG_COLORMAP 0x08U /* image data is color-mapped */

#ifdef PNG_FORMAT_BGR_SUPPORTED
#define PNG_FORMAT_FLAG_BGR 0x10U /* BGR colors, else order is RGB */
#endif

#ifdef PNG_FORMAT_AFIRST_SUPPORTED
#define PNG_FORMAT_FLAG_AFIRST 0x20U /* alpha channel comes first */
#endif

#define PNG_FORMAT_FLAG_ASSOCIATED_ALPHA 0x40U /* alpha channel is associated \
                                                */

#define PNG_FORMAT_GRAY 0
#define PNG_FORMAT_GA PNG_FORMAT_FLAG_ALPHA
#define PNG_FORMAT_AG (PNG_FORMAT_GA | PNG_FORMAT_FLAG_AFIRST)
#define PNG_FORMAT_RGB PNG_FORMAT_FLAG_COLOR
#define PNG_FORMAT_BGR (PNG_FORMAT_FLAG_COLOR | PNG_FORMAT_FLAG_BGR)
#define PNG_FORMAT_RGBA (PNG_FORMAT_RGB | PNG_FORMAT_FLAG_ALPHA)
#define PNG_FORMAT_ARGB (PNG_FORMAT_RGBA | PNG_FORMAT_FLAG_AFIRST)
#define PNG_FORMAT_BGRA (PNG_FORMAT_BGR | PNG_FORMAT_FLAG_ALPHA)
#define PNG_FORMAT_ABGR (PNG_FORMAT_BGRA | PNG_FORMAT_FLAG_AFIRST)

#define PNG_IMAGE_VERSION 1

#define PNG_IMAGE_PIXEL_(test, fmt) \
  (((fmt)&PNG_FORMAT_FLAG_COLORMAP) ? 1 : test(fmt))

#define PNG_IMAGE_SAMPLE_CHANNELS(fmt) \
  (((fmt) & (PNG_FORMAT_FLAG_COLOR | PNG_FORMAT_FLAG_ALPHA)) + 1)

#define PNG_IMAGE_PIXEL_CHANNELS(fmt) \
  PNG_IMAGE_PIXEL_(PNG_IMAGE_SAMPLE_CHANNELS, fmt)

#define PNG_IMAGE_ROW_STRIDE(image) \
  (PNG_IMAGE_PIXEL_CHANNELS((image).format) * (image).width)

#define PNG_IMAGE_SAMPLE_COMPONENT_SIZE(fmt) \
  ((((fmt)&PNG_FORMAT_FLAG_LINEAR) >> 2) + 1)

#define PNG_IMAGE_PIXEL_COMPONENT_SIZE(fmt) \
  PNG_IMAGE_PIXEL_(PNG_IMAGE_SAMPLE_COMPONENT_SIZE, fmt)

#define PNG_IMAGE_BUFFER_SIZE(image, row_stride)                     \
  (PNG_IMAGE_PIXEL_COMPONENT_SIZE((image).format) * (image).height * \
   (row_stride))

#define PNG_IMAGE_SIZE(image) \
  PNG_IMAGE_BUFFER_SIZE(image, PNG_IMAGE_ROW_STRIDE(image))

typedef uint8_t *png_bytep;

#if UINT_MAX == 65535
typedef unsigned int png_uint_16;
#elif USHRT_MAX == 65535
typedef unsigned short png_uint_16;
#else
#error "libpng requires an unsigned 16-bit type"
#endif

#define PNG_LIBPNG_VER_STRING "1.6.38.git"

#define PNG_COLOR_MASK_COLOR 2
#define PNG_COLOR_MASK_ALPHA 4

#define PNG_COLOR_TYPE_RGB (PNG_COLOR_MASK_COLOR)
#define PNG_COLOR_TYPE_RGB_ALPHA (PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_ALPHA)
#define PNG_COLOR_TYPE_RGBA PNG_COLOR_TYPE_RGB_ALPHA

#define PNG_FILTER_TYPE_BASE 0
#define PNG_COMPRESSION_TYPE_BASE 0
#define PNG_INTERLACE_NONE 0
