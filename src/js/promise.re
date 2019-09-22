/* This file is part of reason-promise, released under the MIT license. See
   LICENSE.md for details, or visit
   https://github.com/aantron/repromise/blob/master/LICENSE.md. */



type rejectable('a, 'e);
type never;

type promise('a) = rejectable('a, never);
type t('a) = promise('a);



let onUnhandledException = ref(exn => {
  prerr_endline("Unhandled exception in promise callback:");
  Js.Console.error(exn);
});



[%%bs.raw {|
function WrappedRepromise(p) {
    this.wrapped = p;
};

function unwrap(value) {
    if (value instanceof WrappedRepromise)
        return value.wrapped;
    else
        return value;
}

function wrap(value) {
    if (value != null && typeof value.then === 'function')
        return new WrappedRepromise(value);
    else
        return value;
}

function make(executor) {
    return new Promise(function (resolve, reject) {
        var wrappingResolve = function(value) {
            resolve(wrap(value));
        };
        executor(wrappingResolve, reject);
    });
};

function resolved(value) {
    return Promise.resolve(wrap(value));
};

function then(promise, callback) {
    return promise.then(function (value) {
        try {
            return callback(unwrap(value));
        }
        catch (exception) {
            onUnhandledException[0](exception);
        }
    });
};

function map(promise, callback) {
    return promise.then(function (value) {
        try {
            return resolved(callback(unwrap(value)));
        }
        catch (exception) {
            onUnhandledException[0](exception);
        }
    });
};

function catch_(callback, promise) {
    var safeCallback = function (error) {
        try {
            return callback(error);
        }
        catch (exception) {
            onUnhandledException[0](exception);
        }
    };

    return promise.catch(safeCallback);
};

function arrayToList(array) {
  var list = 0;
  for (var index = array.length - 1; index >= 0; --index) {
    list = [array[index], list];
  }
  return list;
};

function listToArray(list) {
  var array = [];
  while (list !== 0) {
    array.push(list[0]);
    list = list[1];
  }
  return array;
};

function mapArray(f, a) {
  return a.map(f);
};
|}];

[@bs.val]
external arrayToList: array('a) => list('a) = "arrayToList";

[@bs.val]
external listToArray: list('a) => array('a) = "listToArray";

[@bs.val]
external mapArray: ('a => 'b, array('a)) => array('b) = "mapArray";



module Rejectable = {
  type t('a, 'e) = rejectable('a, 'e);

  external relax: promise('a) => rejectable('a, _) = "%identity";

  [@bs.val]
  external jsNew:
    (('a => unit) => ('e => unit) => unit) => rejectable('a, 'e) = "make";

  let pending = () => {
    let resolve = ref(ignore);
    let reject = ref(ignore);
    let p =
      jsNew((resolve', reject') => {
        resolve := resolve';
        reject := reject';
      });
    (p, resolve^, reject^);
  };

  [@bs.val]
  external resolved: 'a => rejectable('a, _) = "resolved";

  [@bs.val]
  external flatMap:
    (rejectable('a, 'e), 'a => rejectable('b, 'e)) => rejectable('b, 'e) =
      "then";

  [@bs.val]
  external map:
    (rejectable('a, 'e), 'a => 'b) => rejectable('b, 'e) =
      "map";

  let on = (promise, callback) =>
    ignore(map(promise, callback));

  let tap = (promise, callback) => {
    on(promise, callback);
    promise;
  };

  [@bs.scope "Promise"]
  [@bs.val]
  external rejected: 'e => rejectable(_, 'e) = "reject";

  [@bs.val]
  external catch:
    ('e => rejectable('a, 'e2), rejectable('a, 'e)) => rejectable('a, 'e2) =
      "catch_";

  [@bs.val]
  external unwrap: 'a => 'a = "unwrap";

  [@bs.scope "Promise"]
  [@bs.val]
  external jsAll: 'a => 'b = "all";

  let arrayAll = promises =>
    map(jsAll(promises), mapArray(unwrap));

  let all = promises =>
    map(arrayAll(listToArray(promises)), results => arrayToList(results));

  let all2 = (p1, p2) =>
    jsAll((p1, p2));

  let all3 = (p1, p2, p3) =>
    jsAll((p1, p2, p3));

  let all4 = (p1, p2, p3, p4) =>
    jsAll((p1, p2, p3, p4));

  let all5 = (p1, p2, p3, p4, p5) =>
    jsAll((p1, p2, p3, p4, p5));

  let all6 = (p1, p2, p3, p4, p5, p6) =>
    jsAll((p1, p2, p3, p4, p5, p6));

  [@bs.scope "Promise"]
  [@bs.val]
  external jsRace: array(rejectable('a, 'e)) => rejectable('a, 'e) = "race";

  let race = promises =>
    if (promises == []) {
      raise(Invalid_argument("Repromise.race([]) would be pending forever"));
    }
    else {
      jsRace(listToArray(promises));
    };

  external fromJsPromise:
    Js.Promise.t('a) => rejectable('a, Js.Promise.error) = "%identity";

  external toJsPromise: rejectable('a, _) => Js.Promise.t('a) = "%identity";
};



let pending = () => {
  let (p, resolve, _) = Rejectable.pending();
  (p, resolve);
};

let exec = executor => {
  let (p, resolve) = pending();
  executor(resolve);
  p;
};

let resolved = Rejectable.resolved;
let flatMap = Rejectable.flatMap;
let map = Rejectable.map;
let on = Rejectable.on;
let tap = Rejectable.tap;
let all = Rejectable.all;
let all2 = Rejectable.all2;
let all3 = Rejectable.all3;
let all4 = Rejectable.all4;
let all5 = Rejectable.all5;
let all6 = Rejectable.all6;
let arrayAll = Rejectable.arrayAll;
let race = Rejectable.race;



/* Compatibility with BukleScript < 6. */
type result('a, 'e) = Belt.Result.t('a, 'e) = Ok('a) | Error('e);

let flatMapOk = (promise, _callback) =>
  flatMap(promise, result =>
    switch (result) {
    | Ok(_) => [%raw "_callback(result[0])"]
    | Error(_) as error => resolved(error)
    });

let flatMapError = (promise, _callback) =>
  flatMap(promise, result =>
    switch (result) {
    | Ok(_) as ok => resolved(ok)
    | Error(_) => [%raw "_callback(result[0])"]
    });

let mapOk = (promise, _callback) =>
  map(promise, result =>
    switch (result) {
    | Ok(_) => Ok([%raw "_callback(result[0])"])
    | Error(_) as error => error
    });

let mapError = (promise, _callback) =>
  map(promise, result =>
    switch (result) {
    | Ok(_) as ok => ok
    | Error(_) => Error([%raw "_callback(result[0])"])
    });

let onOk = (promise, _callback) =>
  on(promise, result =>
    switch (result) {
    | Ok(_) => [%raw "_callback(result[0])"]
    | Error(_) => ()
    });

let onError = (promise, _callback) =>
  on(promise, result =>
    switch (result) {
    | Ok(_) => ()
    | Error(_) => [%raw "_callback(result[0])"]
    });

let tapOk = (promise, callback) => {
  onOk(promise, callback);
  promise;
};

let tapError = (promise, callback) => {
  onError(promise, callback);
  promise;
};

module Operators = {
  let (>|=) = mapOk;
  let (>>=) = flatMapOk;
};



let flatMapSome = (promise, _callback) =>
  flatMap(promise, option =>
    switch (option) {
    | Some(_) => [%raw "_callback(Caml_option.valFromOption(option))"]
    | None => resolved(None)
    });

let mapSome = (promise, _callback) =>
  map(promise, option =>
    switch (option) {
    | Some(_) => Some([%raw "_callback(Caml_option.valFromOption(option))"])
    | None => None
    });

let onSome = (promise, _callback) =>
  on(promise, option =>
    switch (option) {
    | Some(_) => [%raw "_callback(Caml_option.valFromOption(option))"]
    | None => ()
    });

let tapSome = (promise, callback) => {
  onSome(promise, callback);
  promise;
};



module FastPipe = {
};