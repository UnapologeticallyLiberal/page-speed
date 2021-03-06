// Copyright 2010 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "pagespeed/image_compression/image_attributes_factory.h"

#include <stdio.h>

extern "C" {
#ifdef USE_SYSTEM_LIBJPEG
#include "jpeglib.h"
#else
#include "third_party/libjpeg/jpeglib.h"
#endif
#ifdef USE_SYSTEM_LIBPNG
#include "png.h"
#else
#include "third_party/libpng/png.h"
#endif
}  // extern "C"

#if PNG_LIBPNG_VER >= 10400
// Including setjmp in older versions of libpng triggers an error
// (since libpng already includes setjmp in those versions). However
// newer versions of libpng no longer include setjmp directly, so we
// must include it conditionally based on the libpng version.
#include <setjmp.h>
#endif

#include "pagespeed/core/resource.h"

#include "pagespeed/image_compression/gif_reader.h"
#include "pagespeed/image_compression/jpeg_reader.h"
#include "pagespeed/image_compression/png_optimizer.h"

namespace {

bool GetJpegWidthAndHeight(const pagespeed::Resource* resource,
                           int* out_width,
                           int* out_height) {
  pagespeed::image_compression::JpegReader reader;
  jpeg_decompress_struct* jpeg_decompress = reader.decompress_struct();

  // Configure error handlers.
  jmp_buf env;
  if (setjmp(env)) {
    jpeg_abort_decompress(jpeg_decompress);
    return false;
  }

  jpeg_decompress->client_data = static_cast<void*>(&env);
  const std::string& image_string = resource->GetResponseBody();
  reader.PrepareForRead(image_string.data(), image_string.size());
  jpeg_read_header(jpeg_decompress, TRUE);
  *out_width = jpeg_decompress->image_width;
  *out_height = jpeg_decompress->image_height;

  // We read the JPEG header but not the image data, so we want to
  // abort the decompression process (as opposed to calling
  // jpeg_finish_decompress).
  jpeg_abort_decompress(jpeg_decompress);
  return true;
}

bool GetWidthAndHeightFromPngReader(
    const pagespeed::Resource* resource,
    pagespeed::image_compression::PngReaderInterface* reader,
    int* out_width,
    int* out_height) {
  int bit_depth, color_type;
  if (!reader->GetAttributes(resource->GetResponseBody(),
                             out_width,
                             out_height,
                             &bit_depth,
                             &color_type)) {
    return false;
  }

  return true;
}

bool GetPngWidthAndHeight(const pagespeed::Resource* resource,
                          int* out_width,
                          int* out_height) {
  pagespeed::image_compression::PngReader reader;
  return GetWidthAndHeightFromPngReader(resource,
                                        &reader,
                                        out_width,
                                        out_height);
}

bool GetGifWidthAndHeight(const pagespeed::Resource* resource,
                          int* out_width,
                          int* out_height) {
  pagespeed::image_compression::GifReader reader;
  return GetWidthAndHeightFromPngReader(resource,
                                        &reader,
                                        out_width,
                                        out_height);
}

}  // namespace

namespace pagespeed {
namespace image_compression {

ImageAttributesFactory::ImageAttributesFactory() {}
ImageAttributesFactory::~ImageAttributesFactory() {}

ImageAttributes * ImageAttributesFactory::NewImageAttributes(
    const pagespeed::Resource* resource) const {
  if (resource->GetResourceType() != pagespeed::IMAGE) {
    return NULL;
  }

  int width = 0;
  int height = 0;
  switch (resource->GetImageType()) {
    case pagespeed::JPEG:
      if (!GetJpegWidthAndHeight(resource, &width, &height)) {
        return NULL;
      }
      break;
    case pagespeed::PNG:
      if (!GetPngWidthAndHeight(resource, &width, &height)) {
        return NULL;
      }
      break;
    case pagespeed::GIF:
      if (!GetGifWidthAndHeight(resource, &width, &height)) {
        return NULL;
      }
      break;
    default:
      return NULL;
  }

  return new pagespeed::ConcreteImageAttributes(width, height);
}

}  // namespace image_compression
}  // namespace pagespeed
