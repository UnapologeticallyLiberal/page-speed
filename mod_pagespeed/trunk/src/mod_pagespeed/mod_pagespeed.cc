// Copyright 2010 Google Inc.
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

#include <string>

#include "third_party/apache_httpd/include/httpd.h"
#include "third_party/apache_httpd/include/http_config.h"
#include "third_party/apache_httpd/include/http_log.h"
#include "third_party/apache_httpd/include/http_protocol.h"

#include "base/string_util.h"
#include "mod_pagespeed/apache/log_message_handler.h"
#include "mod_pagespeed/apache/pool_util.h"
#include "pagespeed/cssmin/cssmin.h"
#include "pagespeed/html/html_compactor.h"
#include "pagespeed/image_compression/gif_reader.h"
#include "pagespeed/image_compression/jpeg_optimizer.h"
#include "pagespeed/image_compression/png_optimizer.h"
#include "third_party/jsmin/cpp/jsmin.h"

using pagespeed::html::HtmlCompactor;
using pagespeed::cssmin::MinifyCss;
using pagespeed::image_compression::GifReader;
using pagespeed::image_compression::OptimizeJpeg;
using pagespeed::image_compression::PngReader;
using pagespeed::image_compression::PngOptimizer;
using jsmin::MinifyJs;

namespace {

static const char pagespeed_filter_name[] = "PAGESPEED";

enum ResourceType {HTML, JAVASCRIPT, CSS, GIF, PNG, JPEG};

// We use the following structure to keep the pagespeed module context. We
// accumulate buckets into the input string. When we receive the EOS bucket, we
// optimize the content to the string output, and re-bucket it.
class PagespeedContext {
 public:
  std::string input;   // original content
  std::string output;  // content after pagespeed optimization
};

// Check if pagespeed optimization rules applicable.
bool check_pagespeed_applicable(ap_filter_t* filter,
                                apr_bucket_brigade* bb,
                                ResourceType* type) {
  request_rec* request = filter->r;
  // We can't operate on Content-Ranges.
  if (apr_table_get(request->headers_out, "Content-Range") != NULL) {
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, request,
                  "Content-Range is not available");
    return false;
  }

  // Only support text/html, javascript, css, gif, png and jpeg.
  if (StartsWithASCII(request->content_type, "text/html", false)) {
    *type = HTML;
  } else if (StartsWithASCII(request->content_type, "text/javascript", false) ||
             StartsWithASCII(request->content_type, "application/x-javascript",
                             false) ||
             StartsWithASCII(request->content_type, "application/javascript",
                             false)) {
    *type = JAVASCRIPT;
  } else if (StartsWithASCII(request->content_type, "text/css", false)) {
    *type = CSS;
  } else if (StartsWithASCII(request->content_type, "image/gif", false)) {
    *type = GIF;
  } else if (StartsWithASCII(request->content_type, "image/png", false)) {
    *type = PNG;
  } else if (StartsWithASCII(request->content_type, "image/jpg", false) ||
             StartsWithASCII(request->content_type, "image/jpeg", false)) {
    *type = JPEG;
  } else {
    ap_log_rerror(APLOG_MARK, APLOG_INFO, APR_SUCCESS, request,
                  "Content-Type=%s URI=%s%s",
                  request->content_type, request->hostname,
                  request->unparsed_uri);
    return false;
  }

  return true;
}

// Optimize the resource.
bool do_pagespeed(const ResourceType resource_type,
                  const std::string& input, std::string* output) {
  bool success = false;
  switch (resource_type) {
    case HTML:
      success = HtmlCompactor::CompactHtml(input, output);
      break;
    case JAVASCRIPT:
      success = MinifyJs(input, output);
      break;
    case CSS:
      success = MinifyCss(input, output);
      break;
    case GIF:
    {
      GifReader reader;
      success = PngOptimizer::OptimizePng(reader, input, output);
      break;
    }
    case PNG:
    {
      PngReader reader;
      success = PngOptimizer::OptimizePng(reader, input, output);
      break;
    }
    case JPEG:
      success = OptimizeJpeg(input, output);
      break;
    default:
      // should never be here.
      DCHECK(false);
      break;
  }
  return success;
}

apr_status_t pagespeed_out_filter(ap_filter_t *filter, apr_bucket_brigade *bb) {
  // Do nothing if there is nothing.
  if (APR_BRIGADE_EMPTY(bb)) {
    return ap_pass_brigade(filter->next, bb);
  }

  // Check if pagespeed optimization applicable.
  ResourceType resource_type;
  if (!check_pagespeed_applicable(filter, bb, &resource_type)) {
    ap_remove_output_filter(filter);
    return ap_pass_brigade(filter->next, bb);
  }

  request_rec* request = filter->r;
  PagespeedContext* context = static_cast<PagespeedContext*>(filter->ctx);

  // Initialize pagespeed context structure.
  if (context == NULL) {
    filter->ctx = context = new PagespeedContext;
    mod_pagespeed::PoolRegisterDelete(request->pool, context);
    apr_table_unset(request->headers_out, "Content-Length");
    apr_table_unset(request->headers_out, "Content-MD5");
  }

  for (apr_bucket* bucket = APR_BRIGADE_FIRST(bb);
       bucket != APR_BRIGADE_SENTINEL(bb);
       bucket = APR_BUCKET_NEXT(bucket) ) {
    if (!APR_BUCKET_IS_METADATA(bucket)) {
      const char* buf = NULL;
      size_t bytes = 0;
      apr_status_t ret_code =
          apr_bucket_read(bucket, &buf, &bytes, APR_BLOCK_READ);
      if( ret_code == APR_SUCCESS ) {
        // save the content of the bucket
        context->input.append(buf, bytes);
      } else {
        // Read error, log the eror and return.
        ap_log_rerror(APLOG_MARK, APLOG_ERR, ret_code, request,
                      "Reading bucket failed (rcode=%d)", ret_code);
        return ret_code;
      }
      // Processed the bucket, now delete it.
      apr_bucket_delete(bucket);
    } else if (APR_BUCKET_IS_EOS(bucket)) {
      if (context->input.empty()) {
        break;  // Nothing to be optimized.
      }

      // Do pagespeed optimization.
      bool success = do_pagespeed(resource_type, context->input,
                                  &context->output);

      // Re-create the bucket.
      apr_bucket* new_bucket = NULL;
      if (!success || context->input.size() <= context->output.size()) {
        if (!success) {
          ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_SUCCESS, request,
                        "Minify %s failed. URI=%s%s",
                        request->content_type, request->hostname,
                        request->unparsed_uri);
        } else {
          ap_log_rerror(APLOG_MARK, APLOG_INFO, APR_SUCCESS, request,
                        "Minify %s(%s%s) original=%d, minified=%d",
                        request->content_type,
                        request->hostname, request->unparsed_uri,
                        context->input.size(), context->output.size());
        }
        // Use the original content.
        new_bucket = apr_bucket_transient_create(
            context->input.data(),
            context->input.size(),
            request->connection->bucket_alloc);
      } else {
        double saved_percent =
            100 - 100.0 * context->output.size() / context->input.size();
        ap_log_rerror(APLOG_MARK, APLOG_INFO, APR_SUCCESS, request,
                      "%5.2lf%% saved Minify %s(%s%s) original=%d, minified=%d",
                      saved_percent,  request->content_type,
                      request->hostname, request->unparsed_uri,
                      context->input.size(), context->output.size());
        if (resource_type == GIF) {
          // We minified the gif to png.
          ap_set_content_type(request, "image/png");
        }
        // Use the optimized content.
        new_bucket = apr_bucket_transient_create(
            context->output.data(),
            context->output.size(),
            request->connection->bucket_alloc);
      }
      APR_BUCKET_INSERT_BEFORE(bucket, new_bucket);
    } else {
      // Unknown meta data, do nothing.
      // If the meta data is FLUSH, we cannot do it because we need the
      // entire content before we can optimize it.
      //
      // TODO(lsong): To make it production ready, we'll need to ensure that we
      // pass unknown metadta buckets down the chain.
      ap_log_rerror(APLOG_MARK, APLOG_INFO, APR_SUCCESS, request,
                    "Unknown meta data");
    }
  }
  return ap_pass_brigade(filter->next, bb);
}


// This function is a callback and it declares what
// other functions should be called for request
// processing and configuration requests. This
// callback function declares the Handlers for
// other events.

void mod_pagespeed_register_hooks(apr_pool_t *p) {
  // Enable logging using pagespeed style
  mod_pagespeed::InstallLogMessageHandler();

  ap_register_output_filter(pagespeed_filter_name,
                            pagespeed_out_filter,
                            NULL,
                            AP_FTYPE_RESOURCE);
}

}  // namespace

extern "C" {
// Export our module so Apache is able to load us.
// See http://gcc.gnu.org/wiki/Visibility for more information.
#if defined(__linux)
#pragma GCC visibility push(default)
#endif

// Declare and populate the module's data structure.  The
// name of this structure ('pagespeed_module') is important - it
// must match the name of the module.  This structure is the
// only "glue" between the httpd core and the module.

module AP_MODULE_DECLARE_DATA pagespeed_module = {
  // Only one callback function is provided.  Real
  // modules will need to declare callback functions for
  // server/directory configuration, configuration merging
  // and other tasks.
  STANDARD20_MODULE_STUFF,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  mod_pagespeed_register_hooks,      // callback for registering hooks
};

#if defined(__linux)
#pragma GCC visibility pop
#endif
}  // extern "C"