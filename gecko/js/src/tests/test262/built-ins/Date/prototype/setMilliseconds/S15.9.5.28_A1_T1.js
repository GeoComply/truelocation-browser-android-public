// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setMilliseconds" has { DontEnum } attributes
esid: sec-date.prototype.setmilliseconds
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setMilliseconds;
if (x === 1) {
  Date.prototype.setMilliseconds = 2;
} else {
  Date.prototype.setMilliseconds = 1;
}

assert.notSameValue(
  Date.prototype.setMilliseconds,
  x,
  'The value of Date.prototype.setMilliseconds is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.

reportCompare(0, 0);
