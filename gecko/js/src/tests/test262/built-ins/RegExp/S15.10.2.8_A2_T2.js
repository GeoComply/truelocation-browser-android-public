// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The form (?! Disjunction ) specifies a zero-width negative lookahead.
    In order for it to succeed, the pattern inside Disjunction must fail to match at the current position.
    The current position is not advanced before matching the sequel
es5id: 15.10.2.8_A2_T2
description: >
    Execute /Java(?!Script)([A-Z]\w*)/.exec("using of JavaBeans
    technology") and check results
---*/

var __executed = /Java(?!Script)([A-Z]\w*)/.exec("using of JavaBeans technology");

var __expected = ["JavaBeans", "Beans"];
__expected.index = 9;
__expected.input = "using of JavaBeans technology";

assert.sameValue(
  __executed.length,
  __expected.length,
  'The value of __executed.length is expected to equal the value of __expected.length'
);

assert.sameValue(
  __executed.index,
  __expected.index,
  'The value of __executed.index is expected to equal the value of __expected.index'
);

assert.sameValue(
  __executed.input,
  __expected.input,
  'The value of __executed.input is expected to equal the value of __expected.input'
);

for(var index=0; index<__expected.length; index++) {
  assert.sameValue(
    __executed[index],
    __expected[index],
    'The value of __executed[index] is expected to equal the value of __expected[index]'
  );
}

reportCompare(0, 0);
