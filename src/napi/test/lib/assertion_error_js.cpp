// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "modules.h"

namespace napitest {
namespace module {

const char *assertion_error_js = R"JavaScript(

'use strict';

class AssertionError extends Error {
  constructor(options) {
    const {
      message,
      operator,
      stackStartFn,
      details,
    } = options;
    let {
      actual,
      expected
    } = options;

    // const limit = Error.stackTraceLimit;
    // Error.stackTraceLimit = 0;

    // if (message != null) {
    //   super(String(message));
    // } else {
    //   // Prevent the error stack from being visible by duplicating the error
    //   // in a very close way to the original in case both sides are actually
    //   // instances of Error.
    //   if (typeof actual === 'object' && actual !== null &&
    //       typeof expected === 'object' && expected !== null &&
    //       'stack' in actual && actual instanceof Error &&
    //       'stack' in expected && expected instanceof Error) {
    //     actual = copyError(actual);
    //     expected = copyError(expected);
    //   }

    //   if (operator === 'deepStrictEqual' || operator === 'strictEqual') {
    //     super(createErrDiff(actual, expected, operator));
    //   } else if (operator === 'notDeepStrictEqual' ||
    //     operator === 'notStrictEqual') {
    //     // In case the objects are equal but the operator requires unequal, show
    //     // the first object and say A equals B
    //     let base = kReadableOperator[operator];
    //     const res = StringPrototypeSplit(inspectValue(actual), '\n');

    //     // In case "actual" is an object or a function, it should not be
    //     // reference equal.
    //     if (operator === 'notStrictEqual' &&
    //         ((typeof actual === 'object' && actual !== null) ||
    //          typeof actual === 'function')) {
    //       base = kReadableOperator.notStrictEqualObject;
    //     }

    //     // Only remove lines in case it makes sense to collapse those.
    //     // TODO: Accept env to always show the full error.
    //     if (res.length > 50) {
    //       res[46] = `${blue}...${white}`;
    //       while (res.length > 47) {
    //         ArrayPrototypePop(res);
    //       }
    //     }

    //     // Only print a single input.
    //     if (res.length === 1) {
    //       super(`${base}${res[0].length > 5 ? '\n\n' : ' '}${res[0]}`);
    //     } else {
    //       super(`${base}\n\n${ArrayPrototypeJoin(res, '\n')}\n`);
    //     }
    //   } else {
    //     let res = inspectValue(actual);
    //     let other = inspectValue(expected);
    //     const knownOperator = kReadableOperator[operator];
    //     if (operator === 'notDeepEqual' && res === other) {
    //       res = `${knownOperator}\n\n${res}`;
    //       if (res.length > 1024) {
    //         res = `${StringPrototypeSlice(res, 0, 1021)}...`;
    //       }
    //       super(res);
    //     } else {
    //       if (res.length > 512) {
    //         res = `${StringPrototypeSlice(res, 0, 509)}...`;
    //       }
    //       if (other.length > 512) {
    //         other = `${StringPrototypeSlice(other, 0, 509)}...`;
    //       }
    //       if (operator === 'deepEqual') {
    //         res = `${knownOperator}\n\n${res}\n\nshould loosely deep-equal\n\n`;
    //       } else {
    //         const newOp = kReadableOperator[`${operator}Unequal`];
    //         if (newOp) {
    //           res = `${newOp}\n\n${res}\n\nshould not loosely deep-equal\n\n`;
    //         } else {
    //           other = ` ${operator} ${other}`;
    //         }
    //       }
    //       super(`${res}${other}`);
    //     }
    //   }
    // }

    // Error.stackTraceLimit = limit;

    // this.generatedMessage = !message;
    // Object.defineProperty(this, 'name', {
    //   value: 'AssertionError [ERR_ASSERTION]',
    //   enumerable: false,
    //   writable: true,
    //   configurable: true
    // });
    // this.code = 'ERR_ASSERTION';
    // if (details) {
    //   this.actual = undefined;
    //   this.expected = undefined;
    //   this.operator = undefined;
    //   for (let i = 0; i < details.length; i++) {
    //     this['message ' + i] = details[i].message;
    //     this['actual ' + i] = details[i].actual;
    //     this['expected ' + i] = details[i].expected;
    //     this['operator ' + i] = details[i].operator;
    //     this['stack trace ' + i] = details[i].stack;
    //   }
    // } else {
    //   this.actual = actual;
    //   this.expected = expected;
    //   this.operator = operator;
    // }
    // Error.captureStackTrace(this, stackStartFn || stackStartFunction);
    // // Create error message including the error code in the name.
    // this.stack; // eslint-disable-line no-unused-expressions

    if (message != null) {
      super(String(message));
    } else {
      super("Error");
    }

    const stackArray = this.stack.split('\n');
    const reducedStackArray = [];
    const fnNamePattern = `.${stackStartFn.name} (`;
    let found = false;
    for (const stackFrame of stackArray) {
      if (found) {
        reducedStackArray.push(stackFrame);
      } else {
        found = (stackFrame.indexOf(fnNamePattern) >= 0);
      }
    }
    this.stack = reducedStackArray.join('\n');

    // Reset the name.
    this.name = 'AssertionError';

    this.generatedMessage = !message;

    this.actual = String(actual);
    this.expected = String(expected);
    this.operator = operator;
  }

  toString() {
    return `${this.name} [${this.code}]: ${this.message}`;
  }
}

module.exports = AssertionError;

)JavaScript";

} // namespace module
} // namespace napitest
