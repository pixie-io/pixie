/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import {
  getQueryFuncs, validateVis, TABLE_DISPLAY_TYPE, Vis,
} from './vis';

const testVisNoVars: Vis = {
  variables: [],
  globalFuncs: [],
  widgets: [{
    func: {
      name: 'get_latency',
      args: [{
        name: 'foo',
        value: 'abc',
      }],
    },
    displaySpec: {
      '@type': TABLE_DISPLAY_TYPE,
    },
  },
  {
    name: 'a_widget',
    func: {
      name: 'get_error_rate',
      args: [{
        name: 'bar',
        value: 'def',
      }],
    },
    displaySpec: {
      '@type': TABLE_DISPLAY_TYPE,
    },
  }],
};

const testVisWithVars: Vis = {
  variables: [{
    name: 'myvar1',
    type: 'PX_STRING',
    defaultValue: 'abc',
  }, {
    name: 'myvar2',
    type: 'PX_STRING',
    defaultValue: 'def',
  }],
  globalFuncs: [],
  widgets: [{
    name: 'latency',
    func: {
      name: 'get_latency',
      args: [{
        name: 'foo',
        variable: 'myvar1',
      }, {
        name: 'bar',
        variable: 'myvar2',
      }],
    },
    displaySpec: {
      '@type': TABLE_DISPLAY_TYPE,
    },
  }],
};

const testVisWithGlobalFuncs: Vis = {
  variables: [{
    name: 'myvar1',
    type: 'PX_STRING',
    defaultValue: 'abc',
  }, {
    name: 'myvar2',
    type: 'PX_STRING',
    defaultValue: 'def',
  }],
  globalFuncs: [{
    outputName: 'LET',
    func: {
      name: 'get_latency',
      args: [{
        name: 'foo',
        variable: 'myvar1',
      }, {
        name: 'bar',
        variable: 'myvar2',
      }],
    },
  }],
  widgets: [{
    name: 'latency',
    globalFuncOutputName: 'LET',
    displaySpec: {
      '@type': TABLE_DISPLAY_TYPE,
    },
  }],
};

describe('getQueryFuncs', () => {
  it('should fill in values from constants', () => {
    expect(getQueryFuncs(testVisNoVars, {}, null)).toStrictEqual([
      {
        name: 'get_latency',
        outputTablePrefix: 'widget_0',
        args: [{ name: 'foo', value: 'abc' }],
      },
      {
        name: 'get_error_rate',
        outputTablePrefix: 'a_widget',
        args: [{ name: 'bar', value: 'def' }],
      },
    ]);
  });

  it('should fill in values from variables with defaults', () => {
    expect(getQueryFuncs(testVisWithVars, {}, null)).toStrictEqual([
      {
        name: 'get_latency',
        outputTablePrefix: 'latency',
        args: [
          { name: 'foo', value: 'abc' },
          { name: 'bar', value: 'def' },
        ],
      },
    ]);
  });

  it('should fill in values from variable values overriding defaults', () => {
    expect(getQueryFuncs(testVisWithVars, {
      myvar2: 'xyz',
    }, null)).toStrictEqual([
      {
        name: 'get_latency',
        outputTablePrefix: 'latency',
        args: [
          { name: 'foo', value: 'abc' },
          { name: 'bar', value: 'xyz' },
        ],
      },
    ]);
  });

  it('should yield only one result from global functions', () => {
    expect(getQueryFuncs(testVisWithGlobalFuncs, {
      myvar2: 'xyz',
    }, null)).toStrictEqual([
      {
        name: 'get_latency',
        outputTablePrefix: 'LET',
        args: [
          { name: 'foo', value: 'abc' },
          { name: 'bar', value: 'xyz' },
        ],
      },
    ]);
  });

  it('should support widget selection with a global func', () => {
    expect(getQueryFuncs(testVisWithGlobalFuncs, {
      myvar2: 'xyz',
    }, 'latency')).toStrictEqual([
      {
        name: 'get_latency',
        outputTablePrefix: 'LET',
        args: [
          { name: 'foo', value: 'abc' },
          { name: 'bar', value: 'xyz' },
        ],
      },
    ]);
  });

  it('should support widget selection with a non-global func', () => {
    expect(getQueryFuncs(testVisNoVars, {
      myvar2: 'xyz',
    }, 'a_widget')).toStrictEqual([
      {
        name: 'get_error_rate',
        outputTablePrefix: 'a_widget',
        args: [{ name: 'bar', value: 'def' }],
      },
    ]);
  });
});

const testVisWithMissingGlobalFuncDef: Vis = {
  variables: [],
  globalFuncs: [],
  widgets: [{
    name: 'latency',
    globalFuncOutputName: 'LET',
    displaySpec: {
      '@type': TABLE_DISPLAY_TYPE,
    },
  }],
};

// Note this is an erroneous vis -> you can't define a func and ref
// a global func in the same widget.
const testVisWithGlobalFuncAndDefinedFunc: Vis = {
  variables: [],
  globalFuncs: [{
    outputName: 'LET',
    func: {
      name: 'get_latency',
      args: [],
    },
  }],
  widgets: [{
    name: 'latency',
    globalFuncOutputName: 'LET',
    func: {
      name: 'get_latency',
      args: [],
    },
    displaySpec: {
      '@type': TABLE_DISPLAY_TYPE,
    },
  }],
};

// Note this is an erroneous vis -> you can't reference a variable that doesn't exist.
const testMissingVarVis: Vis = {
  variables: [],
  globalFuncs: [{
    outputName: 'LET',
    func: {
      name: 'get_latency',
      args: [{
        name: 'foo',
        variable: 'nonexistant',
      }],
    },
  }],
  widgets: [{
    name: 'latency',
    globalFuncOutputName: 'LET',
    displaySpec: {
      '@type': TABLE_DISPLAY_TYPE,
    },
  }],
};

// Note that arg "foo" is missing a value or a variable.
const testMissingArgDef: Vis = {
  variables: [],
  globalFuncs: [{
    outputName: 'LET',
    func: {
      name: 'get_latency',
      args: [{
        name: 'foo',
      }],
    },
  }],
  widgets: [{
    name: 'latency',
    globalFuncOutputName: 'LET',
    displaySpec: {
      '@type': TABLE_DISPLAY_TYPE,
    },
  }],
};

describe('validateVis', () => {
  it('accepts valid vis', () => {
    expect(validateVis(testVisNoVars, {})).toEqual([]);
    expect(validateVis(testVisWithGlobalFuncs, {})).toEqual([]);
    expect(validateVis(testVisWithVars, {})).toEqual([]);
  });
  it('errors on missing global func', () => {
    // Do object unpacking to make unexpected behavior render more informatively.
    expect(validateVis(testVisWithMissingGlobalFuncDef, {})).toEqual([expect.objectContaining({
      errType: 'vis',
      details: expect.stringMatching(/globalFunc "LET" referenced by "latency" not found/),
    })]);
  });
  it('errors on including global func and func def', () => {
    // Do object unpacking to make unexpected behavior render more informatively.
    expect(validateVis(testVisWithGlobalFuncAndDefinedFunc, {})).toEqual([expect.objectContaining({
      errType: 'vis',
      details: expect.stringMatching(/"latency" may only have one of "func" and "globalFuncOutputName"/),
    })]);
  });

  it('errors on referencing a missing variable', () => {
    // Do object unpacking to make unexpected behavior render more informatively.
    expect(validateVis(testMissingVarVis, {})).toEqual([expect.objectContaining({
      errType: 'vis',
      details: [expect.stringMatching(/Arg "foo" of "get_latency\(\)" references undefined variable "nonexistant"/)],
    })]);
  });

  it('errors when a func arg does not have a value or variable reference', () => {
    // Do object unpacking to make unexpected behavior render more informatively.
    expect(validateVis(testMissingArgDef, {})).toEqual([expect.objectContaining({
      errType: 'vis',
      details: [expect.stringMatching(/Arg "foo" of "get_latency\(\)" needs either a value or a variable reference/)],
    })]);
  });
});
