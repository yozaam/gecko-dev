// |reftest| skip error:SyntaxError -- class-fields-private,class-methods-private,class-fields-public is not supported
'use strict';
// This file was procedurally generated from the following sources:
// - src/class-elements/err-delete-call-expression-private-method-async-gen.case
// - src/class-elements/delete-error/cls-expr-field-delete.template
/*---
description: It's a SyntaxError if delete operator is applied to MemberExpression.PrivateName (in field)
esid: sec-class-definitions-static-semantics-early-errors
features: [class, class-fields-private, class-methods-private, class-fields-public]
flags: [generated, onlyStrict]
negative:
  phase: parse
  type: SyntaxError
info: |
    Static Semantics: Early Errors

      UnaryExpression : delete UnaryExpression

      It is a Syntax Error if the UnaryExpression is contained in strict mode
      code and the derived UnaryExpression is
      PrimaryExpression : IdentifierReference ,
      MemberExpression : MemberExpression.PrivateName , or
      CallExpression : CallExpression.PrivateName .

---*/


throw "Test262: This statement should not be evaluated.";

var C = class {
  #x;
  g = this.f;
  x = delete g().#m;

  f() {
  return this;
  }
}
