// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype has the property "setHours"
esid: sec-properties-of-the-date-prototype-object
description: The Date.prototype has the property "setHours"
---*/
assert.sameValue(
  Date.prototype.hasOwnProperty("setHours"),
  true,
  'Date.prototype.hasOwnProperty("setHours") must return true'
);

reportCompare(0, 0);
