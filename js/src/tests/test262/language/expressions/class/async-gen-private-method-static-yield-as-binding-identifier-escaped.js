// |reftest| skip error:SyntaxError -- class-static-methods-private is not supported
// This file was procedurally generated from the following sources:
// - src/async-generators/yield-as-binding-identifier-escaped.case
// - src/async-generators/syntax/async-class-expr-static-private-method.template
/*---
description: yield is a reserved keyword within generator function bodies and may not be used as a binding identifier. (Static async generator private method as a ClassExpression element)
esid: prod-AsyncGeneratorMethod
features: [async-iteration, class-static-methods-private]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    ClassElement :
      static PrivateMethodDefinition

    MethodDefinition :
      AsyncGeneratorMethod

    Async Generator Function Definitions

    AsyncGeneratorMethod :
      async [no LineTerminator here] * # PropertyName ( UniqueFormalParameters ) { AsyncGeneratorBody }


    BindingIdentifier : Identifier

    It is a Syntax Error if this production has a [Yield] parameter and
    StringValue of Identifier is "yield".

---*/
throw "Test262: This statement should not be evaluated.";


var C = class { static async *#gen() {
    var yi\u0065ld;
}};
