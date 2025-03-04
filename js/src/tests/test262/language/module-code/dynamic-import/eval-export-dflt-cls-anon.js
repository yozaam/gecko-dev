// |reftest| skip module -- dynamic-import is not supported
// Copyright (C) 2018 Rick Waldron. All rights reserved.
// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
    Default "anonymous" class declaration is correctly initialized upon
    evaluation
esid: sec-moduleevaluation
info: |
    [...]
    16. Let result be the result of evaluating module.[[ECMAScriptCode]].
    [...]

    Runtime Semantics: Evaluation

    ExportDeclaration : export default ClassDeclaration

    [...]
    3. Let className be the sole element of BoundNames of ClassDeclaration.
    4. If className is "*default*", then
       a. Let hasNameProperty be ? HasOwnProperty(value, "name").
       b. If hasNameProperty is false, perform SetFunctionName(value,
          "default").
       c. Let env be the running execution context's LexicalEnvironment.
       d. Perform ? InitializeBoundName("*default*", value, env).
    5. Return NormalCompletion(empty).
flags: [async, module]
features: [dynamic-import]
---*/

export default class { valueOf() { return 45; } }
import('./eval-export-dflt-cls-anon.js').then(imported => {
  assert.sameValue(new imported.default().valueOf(), 45, 'binding initialized');
  assert.sameValue(imported.default.name, 'default', 'correct name is assigned');
}).then($DONE, $DONE).catch($DONE);

