// |reftest| skip error:SyntaxError -- regexp-unicode-property-escapes is not supported
// Copyright 2018 Mathias Bynens. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
author: Mathias Bynens
description: >
  Properties not explicitly listed in the Unicode property escapes spec must
  not be supported.
esid: sec-static-semantics-unicodematchproperty-p
negative:
  phase: parse
  type: SyntaxError
features: [regexp-unicode-property-escapes]
---*/

throw "Test262: This statement should not be evaluated.";

/\p{Block=Adlam}/u;
