// |reftest| skip error:SyntaxError -- regexp-unicode-property-escapes is not supported
// Copyright 2018 Mathias Bynens. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
author: Mathias Bynens
description: >
  This tests violations of the grammar starting here:

  CharacterClassEscape[U]::
    [+U] p{ UnicodePropertyValueExpression }
    [+U] P{ UnicodePropertyValueExpression }
esid: prod-CharacterClassEscape
negative:
  phase: parse
  type: SyntaxError
features: [regexp-unicode-property-escapes]
---*/

throw "Test262: This statement should not be evaluated.";

/\p{=}/u;
