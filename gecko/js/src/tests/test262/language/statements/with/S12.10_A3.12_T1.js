// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    No matter how control leaves the embedded 'Statement',
    the scope chain is always restored to its former state
es5id: 12.10_A3.12_T1
description: >
    Calling a function without "with" statement declared within the
    statement, leading to normal completion
flags: [noStrict]
---*/

this.p1 = 1;
var result = "result";
var myObj = {p1: 'a', 
             value: 'myObj_value',
             valueOf : function(){return 'obj_valueOf';}
}

with(myObj){
  var f = function(){
    p1 = 'x1';
  }
}

f();

if(!(p1 === 1)){
  throw new Test262Error('#1: p1 === 1. Actual:  p1 ==='+ p1  );
}

if(!(myObj.p1 === "x1")){
  throw new Test262Error('#2: myObj.p1 === "x1". Actual:  myObj.p1 ==='+ myObj.p1  );
}

reportCompare(0, 0);
