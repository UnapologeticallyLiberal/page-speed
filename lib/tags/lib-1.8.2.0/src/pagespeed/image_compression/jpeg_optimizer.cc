/**
 * Copyright 2009 Google Inc.
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

// Author: Bryan McQuade, Matthew Steele

#include "pagespeed/image_compression/jpeg_optimizer.h"

#include <setjmp.h>  // for setjmp/longjmp
#include <string.h>  // for memset

#include "base/basictypes.h"

extern "C" {
#ifdef USE_SYSTEM_LIBJPEG
#include "jpeglib.h"
#include "jerror.h"
#else
#include "third_party/chromium/src/third_party/libjpeg/jpeglib.h"
#include "third_party/chromium/src/third_party/libjpeg/jerror.h"
#endif
}

namespace {

// Unfortunately, libjpeg normally only supports reading images from C FILE
// pointers, wheras we want to read from a C++ string.  Fortunately, libjpeg
// also provides an extension mechanism.  Below, we define a new kind of
// jpeg_source_mgr for reading from strings.

// The below code was adapted from the JPEGMemoryReader class that can be found
// in src/o3d/core/cross/bitmap_jpg.cc in the Chromium source tree (r29423).
// That code is Copyright 2009, Google Inc.

METHODDEF(void) InitSource(j_decompress_ptr cinfo) {};

METHODDEF(boolean) FillInputBuffer(j_decompress_ptr cinfo) {
  // Should not be called because we already have all the data
  ERREXIT(cinfo, JERR_INPUT_EOF);
  return TRUE;
}

METHODDEF(void) SkipInputData(j_decompress_ptr cinfo, long num_bytes) {
  jpeg_source_mgr &mgr = *(cinfo->src);
  const int bytes_remaining = mgr.bytes_in_buffer - num_bytes;
  mgr.bytes_in_buffer = bytes_remaining < 0 ? 0 : bytes_remaining;
  mgr.next_input_byte += num_bytes;
}

METHODDEF(void) TermSource(j_decompress_ptr cinfo) {};

// Call this function on a j_decompress_ptr to install a reader that will read
// from the given string.
void JpegStringReader(j_decompress_ptr cinfo, const std::string &data_src) {
  if (cinfo->src == NULL) {
    cinfo->src = (struct jpeg_source_mgr*)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  sizeof(jpeg_source_mgr));
  }
  struct jpeg_source_mgr &src = *(cinfo->src);

  src.init_source = InitSource;
  src.fill_input_buffer = FillInputBuffer;
  src.skip_input_data = SkipInputData;
  src.resync_to_restart = jpeg_resync_to_restart; // default method
  src.term_source = TermSource;

  src.bytes_in_buffer = data_src.size();
  src.next_input_byte =
      reinterpret_cast<JOCTET*>(const_cast<char*>(data_src.data()));
}

// Similarly to the above, libjpeg normally only supports writing to C FILE
// pointers, so next, we define a custom jpeg_destination_mgr for writing to a
// C++ string.

#define DESTINATION_MANAGER_BUFFER_SIZE 4096
struct DestinationManager : public jpeg_destination_mgr {
  JOCTET buffer[DESTINATION_MANAGER_BUFFER_SIZE];
  std::string *str;
};

METHODDEF(void) InitDestination(j_compress_ptr cinfo) {
  DestinationManager &dest =
      *reinterpret_cast<DestinationManager*>(cinfo->dest);

  dest.next_output_byte = dest.buffer;
  dest.free_in_buffer = DESTINATION_MANAGER_BUFFER_SIZE;
};

METHODDEF(boolean) EmptyOutputBuffer(j_compress_ptr cinfo) {
  DestinationManager &dest =
      *reinterpret_cast<DestinationManager*>(cinfo->dest);

  dest.str->append(reinterpret_cast<char*>(dest.buffer),
                   DESTINATION_MANAGER_BUFFER_SIZE);

  dest.free_in_buffer = DESTINATION_MANAGER_BUFFER_SIZE;
  dest.next_output_byte = dest.buffer;

  return TRUE;
};

METHODDEF(void) TermDestination(j_compress_ptr cinfo) {
  DestinationManager &dest =
      *reinterpret_cast<DestinationManager*>(cinfo->dest);

  const size_t datacount =
      DESTINATION_MANAGER_BUFFER_SIZE - dest.free_in_buffer;
  if (datacount > 0) {
    dest.str->append(reinterpret_cast<char*>(dest.buffer), datacount);
  }
};

// Call this function on a j_compress_ptr to install a writer that will write
// to the given string.
void JpegStringWriter(j_compress_ptr cinfo, std::string *data_dest) {
  if (cinfo->dest == NULL) {
    cinfo->dest = (struct jpeg_destination_mgr*)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  sizeof(DestinationManager));
  }
  DestinationManager &dest =
      *reinterpret_cast<DestinationManager*>(cinfo->dest);

  dest.str = data_dest;

  dest.init_destination = InitDestination;
  dest.empty_output_buffer = EmptyOutputBuffer;
  dest.term_destination = TermDestination;
}

// ErrorExit() is installed as a callback, called on errors
// encountered within libjpeg.  The longjmp jumps back
// to the setjmp in JpegOptimizer::CreateOptimizedJpeg().
void ErrorExit(j_common_ptr jpeg_state_struct) {
  jmp_buf *env = static_cast<jmp_buf *>(jpeg_state_struct->client_data);
  (*jpeg_state_struct->err->output_message)(jpeg_state_struct);
  if (env)
    longjmp(*env, 1);
}

// OutputMessageFromReader is called by libjpeg code on an error when reading.
// Without this function, a default function would print to standard error.
void OutputMessage(j_common_ptr jpeg_decompress) {
  // The following code is handy for debugging.
  /*
  char buf[JMSG_LENGTH_MAX];
  (*jpeg_decompress->err->format_message)(jpeg_decompress, buf);
  cerr << "JPEG Reader Error: " << buf << endl;
  */
}

class JpegOptimizer {
 public:
  JpegOptimizer();
  ~JpegOptimizer();

  // Take the given input file and losslessly compress it by removing
  // all unnecessary chunks.  If this function fails (returns false),
  // it can be called again.
  // @return true on success, false on failure.
  bool CreateOptimizedJpeg(const std::string &original,
                           std::string *compressed);

 private:
  bool DoCreateOptimizedJpeg(const std::string &original,
                             std::string *compressed);

  // Structures for jpeg decompression
  jpeg_decompress_struct jpeg_decompress_;
  jpeg_error_mgr decompress_error_;

  // Structures for jpeg compression.
  jpeg_compress_struct jpeg_compress_;
  jpeg_error_mgr compress_error_;

  DISALLOW_COPY_AND_ASSIGN(JpegOptimizer);
};

JpegOptimizer::JpegOptimizer() {
  memset(&jpeg_decompress_, 0, sizeof(jpeg_decompress_struct));
  memset(&jpeg_compress_, 0, sizeof(jpeg_compress_struct));
  memset(&decompress_error_, 0, sizeof(jpeg_error_mgr));
  memset(&compress_error_, 0, sizeof(jpeg_error_mgr));

  jpeg_decompress_.err = jpeg_std_error(&decompress_error_);
  decompress_error_.error_exit = &ErrorExit;
  decompress_error_.output_message = &OutputMessage;
  jpeg_create_decompress(&jpeg_decompress_);

  jpeg_compress_.err = jpeg_std_error(&compress_error_);
  compress_error_.error_exit = &ErrorExit;
  compress_error_.output_message = &OutputMessage;
  jpeg_create_compress(&jpeg_compress_);

  jpeg_compress_.optimize_coding = TRUE;
}

JpegOptimizer::~JpegOptimizer() {
  jpeg_destroy_compress(&jpeg_compress_);
  jpeg_destroy_decompress(&jpeg_decompress_);
}

// Helper for JpegOptimizer::CreateOptimizedJpeg().  This function does the
// work, and CreateOptimizedJpeg() does some cleanup.
bool JpegOptimizer::DoCreateOptimizedJpeg(const std::string &original,
                                          std::string *compressed) {
  // libjpeg's error handling mechanism requires that longjmp be used
  // to get control after an error.
  jmp_buf env;
  if (setjmp(env)) {
    // This code is run only when libjpeg hit an error, and called
    // longjmp(env).  Returning false will cause jpeg_abort_(de)compress to be
    // called on jpeg_(de)compress_, putting those structures back into a state
    // where they can be used again.
    return false;
  }

  // Need to install env so that it will be longjmp()ed to on error.
  jpeg_decompress_.client_data = static_cast<void *>(&env);
  jpeg_compress_.client_data = static_cast<void *>(&env);

  // Prepare to read from a string.
  JpegStringReader(&jpeg_decompress_, original);

  // Read jpeg data into the decompression struct.
  jpeg_read_header(&jpeg_decompress_, TRUE);
  jvirt_barray_ptr *coefficients = jpeg_read_coefficients(&jpeg_decompress_);

  // Copy data from the source to the dest.
  jpeg_copy_critical_parameters(&jpeg_decompress_, &jpeg_compress_);

  // Prepare to write to a string.
  JpegStringWriter(&jpeg_compress_, compressed);

  // Copy the coefficients into the compression struct.
  jpeg_write_coefficients(&jpeg_compress_, coefficients);

  // Finish the compression process.
  jpeg_finish_compress(&jpeg_compress_);
  jpeg_finish_decompress(&jpeg_decompress_);

  return true;
}

bool JpegOptimizer::CreateOptimizedJpeg(const std::string &original,
                                        std::string *compressed) {
  bool result = DoCreateOptimizedJpeg(original, compressed);

  jpeg_decompress_.client_data = NULL;
  jpeg_compress_.client_data = NULL;

  if (!result) {
    // Clean up the state of jpeglib structures.  It is okay to abort even if
    // no (de)compression is in progress.  This is crucial because we enter
    // this block even if no jpeg-related error happened.
    jpeg_abort_decompress(&jpeg_decompress_);
    jpeg_abort_compress(&jpeg_compress_);
  }

  return result;
}

}  // namespace

namespace pagespeed {

namespace image_compression {

bool OptimizeJpeg(const std::string &original,
                  std::string *compressed) {
  JpegOptimizer optimizer;
  return optimizer.CreateOptimizedJpeg(original, compressed);
}

}  // namespace image_compression

}  // namespace pagespeed
