// |reftest| skip -- dynamic-import is not supported
'use strict';
// This file was procedurally generated from the following sources:
// - src/dynamic-import/ns-set-same-values-strict.case
// - src/dynamic-import/namespace/promise.template
/*---
description: The [[Set]] internal method consistently returns `false` even setting the same value - Strict Mode (value from promise then)
esid: sec-finishdynamicimport
features: [Symbol, Symbol.toStringTag, dynamic-import]
flags: [generated, onlyStrict, async]
info: |
    Runtime Semantics: FinishDynamicImport ( referencingScriptOrModule, specifier, promiseCapability, completion )

        1. If completion is an abrupt completion, ...
        2. Otherwise,
            ...
            d. Let namespace be GetModuleNamespace(moduleRecord).
            e. If namespace is an abrupt completion, perform ! Call(promiseCapability.[[Reject]], undefined, « namespace.[[Value]] »).
            f. Otherwise, perform ! Call(promiseCapability.[[Resolve]], undefined, « namespace.[[Value]] »).

    Runtime Semantics: GetModuleNamespace ( module )

        ...
        3. Let namespace be module.[[Namespace]].
        4. If namespace is undefined, then
            a. Let exportedNames be ? module.GetExportedNames(« »).
            b. Let unambiguousNames be a new empty List.
            c. For each name that is an element of exportedNames, do
                i. Let resolution be ? module.ResolveExport(name, « »).
                ii. If resolution is a ResolvedBinding Record, append name to unambiguousNames.
            d. Set namespace to ModuleNamespaceCreate(module, unambiguousNames).
        5. Return namespace.

    ModuleNamespaceCreate ( module, exports )

        ...
        4. Let M be a newly created object.
        5. Set M's essential internal methods to the definitions specified in 9.4.6.
        7. Let sortedExports be a new List containing the same values as the list exports where the
        values are ordered as if an Array of the same values had been sorted using Array.prototype.sort
        using undefined as comparefn.
        8. Set M.[[Exports]] to sortedExports.
        9. Create own properties of M corresponding to the definitions in 26.3.
        10. Set module.[[Namespace]] to M.
        11. Return M.

    26.3 Module Namespace Objects

        A Module Namespace Object is a module namespace exotic object that provides runtime
        property-based access to a module's exported bindings. There is no constructor function for
        Module Namespace Objects. Instead, such an object is created for each module that is imported
        by an ImportDeclaration that includes a NameSpaceImport.

        In addition to the properties specified in 9.4.6 each Module Namespace Object has the
        following own property:

    26.3.1 @@toStringTag

        The initial value of the @@toStringTag property is the String value "Module".

        This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.

    Module Namespace Exotic Objects

        A module namespace object is an exotic object that exposes the bindings exported from an
        ECMAScript Module (See 15.2.3). There is a one-to-one correspondence between the String-keyed
        own properties of a module namespace exotic object and the binding names exported by the
        Module. The exported bindings include any bindings that are indirectly exported using export *
        export items. Each String-valued own property key is the StringValue of the corresponding
        exported binding name. These are the only String-keyed properties of a module namespace exotic
        object. Each such property has the attributes { [[Writable]]: true, [[Enumerable]]: true,
        [[Configurable]]: false }. Module namespace objects are not extensible.


    1. Return false.

---*/

import('./module-code_FIXTURE.js').then(ns => {

    assert.sameValue(Reflect.set(ns, 'local1', 'Test262'), false, 'Reflect.set: local1');
    assert.throws(TypeError, function() {
      ns.local1 = 'Test262';
    }, 'AssignmentExpression: local1');

    assert.sameValue(Reflect.set(ns, 'renamed', 'TC39'), false, 'Reflect.set: renamed');
    assert.throws(TypeError, function() {
      ns.renamed = 'TC39';
    }, 'AssignmentExpression: renamed');

    assert.sameValue(Reflect.set(ns, 'indirect', 'Test262'), false, 'Reflect.set: indirect');
    assert.throws(TypeError, function() {
      ns.indirect = 'Test262';
    }, 'AssignmentExpression: indirect');

    assert.sameValue(Reflect.set(ns, 'default', 42), false, 'Reflect.set: default');
    assert.throws(TypeError, function() {
      ns.default = 42;
    }, 'AssignmentExpression: default');

    assert.sameValue(
      Reflect.set(ns, Symbol.toStringTag, ns[Symbol.toStringTag]),
      false,
      'Reflect.set: Symbol.toStringTag'
    );
    assert.throws(TypeError, function() {
      ns[Symbol.toStringTag] = ns[Symbol.toStringTag];
    }, 'AssignmentExpression: Symbol.toStringTag');

}).then($DONE, $DONE).catch($DONE);
