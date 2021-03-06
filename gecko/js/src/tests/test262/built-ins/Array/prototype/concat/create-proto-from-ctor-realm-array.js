// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.concat
description: Prefer Array constructor of current realm record
info: |
    1. Let O be ? ToObject(this value).
    2. Let A be ? ArraySpeciesCreate(O, 0).
    [...]

    9.4.2.3 ArraySpeciesCreate

    [...]
    5. Let C be ? Get(originalArray, "constructor").
    6. If IsConstructor(C) is true, then
       a. Let thisRealm be the current Realm Record.
       b. Let realmC be ? GetFunctionRealm(C).
       c. If thisRealm and realmC are not the same Realm Record, then
          i. If SameValue(C, realmC.[[Intrinsics]].[[%Array%]]) is true, let C
             be undefined.
    [...]
features: [cross-realm, Symbol.species]
---*/

var array = [];
var callCount = 0;
var OArray = $262.createRealm().global.Array;
var speciesDesc = {
  get: function() {
    callCount += 1;
  }
};
var result;
array.constructor = OArray;

Object.defineProperty(Array, Symbol.species, speciesDesc);
Object.defineProperty(OArray, Symbol.species, speciesDesc);

result = array.concat();

assert.sameValue(
  Object.getPrototypeOf(result),
  Array.prototype,
  'Object.getPrototypeOf(array.concat()) returns Array.prototype'
);
assert.sameValue(callCount, 0, 'The value of callCount is expected to be 0');

reportCompare(0, 0);
