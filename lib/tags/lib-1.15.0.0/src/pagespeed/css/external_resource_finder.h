// Copyright 2011 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_CSS_EXTERNAL_RESOURCE_FINDER_H_
#define PAGESPEED_CSS_EXTERNAL_RESOURCE_FINDER_H_

#include <set>
#include <string>

#include "base/basictypes.h"

namespace pagespeed {

class Resource;

namespace css {

// Get all external resource URLs contained in the body of the given CSS
// resource.  The given resource object must have type CSS.
void FindExternalResourcesInCssResource(
    const Resource& resource,
    std::set<std::string>* external_resource_urls);

// Get all external resource URLs contained in the given CSS text.  This can
// either be the body of an external CSS resource, or the contents of an inline
// CSS block in an HTML resource.
void FindExternalResourcesInCssBlock(
    const std::string& resource_url, const std::string& css_body,
    std::set<std::string>* external_resource_urls);

// These function is exposed only for unit testing.  It should not be called by
// non-test code.
void RemoveCssComments(const std::string& in, std::string* out);


// Simple CSS tokenizer.  Generates a stream of tokens along with the
// token type.  Exposed in the header only for testing.
class CssTokenizer {
 public:
  enum CssTokenType {
    URL,
    IDENT,
    STRING,
    SEPARATOR,
    INVALID,
  };

  CssTokenizer(const std::string& css_body);

  // Generates the next token in the token stream as well as its
  // type. Returns true if a valid token was generated, false
  // otherwise (due to i.e. EOF).
  bool GetNextToken(std::string* out_token, CssTokenType* out_type);

 private:
  bool TakeUrl(std::string* out_token);
  bool TakeString(std::string* out_token);
  bool TakeIdent(std::string* out_token);
  size_t ConsumeEscape(size_t next_token, std::string* out_token);

  bool TakeString(std::string* out_token, size_t *inout_index);

  const std::string css_body_;
  size_t index_;

  DISALLOW_COPY_AND_ASSIGN(CssTokenizer);
};

}  // namespace css

}  // namespace pagespeed

#endif  // PAGESPEED_CSS_EXTERNAL_RESOURCE_FINDER_H_
