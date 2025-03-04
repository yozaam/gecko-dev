// |reftest| skip error:SyntaxError -- class-methods-private,class-fields-private is not supported
// This file was procedurally generated from the following sources:
// - src/class-elements/grammar-privatemeth-duplicate-get-field.case
// - src/class-elements/syntax/invalid/cls-decl-elements-invalid-syntax.template
/*---
description: It's a SyntaxError if a class contains a private getter and a private field with the same name (class declaration)
esid: prod-ClassElement
features: [class-methods-private, class-fields-private, class]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Static Semantics: Early Errors

    ClassBody : ClassElementList
        It is a Syntax Error if PrivateBoundNames of ClassBody contains any duplicate entries, unless the name is used once for a getter and once for a setter and in no other entries.

---*/


throw "Test262: This statement should not be evaluated.";

class C {
  #m;
  get #m() {}
}
