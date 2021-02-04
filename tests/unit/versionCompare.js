// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

// Test cases for version comparison

const JsUnit = imports.jsUnit;

const Environment = imports.ui.environment;
Environment.init();

const Util = imports.misc.util;

const tests = [
    { v1: '40',
      v2: '40',
      res: 0 },
    { v1: '40',
      v2: '42',
      res: -1 },
    { v1: '42',
      v2: '40',
      res: 1 },
    { v1: '3.38.0',
      v2: '40',
      res: -1 },
    { v1: '40',
      v2: '3.38.0',
      res: 1 },
    { v1: '40',
      v2: '3.38.0',
      res: 1 },
    { v1: '40.alpha.1.1',
      v2: '40',
      res: -1 },
    { v1: '40',
      v2: '40.alpha.1.1',
      res: 1 },
    { v1: '40.beta',
      v2: '40',
      res: -1 },
    { v1: '40.1',
      v2: '40',
      res: 1 },
    { v1: '',
      v2: '40.alpha',
      res: -1 },
];

for (let i = 0; i < tests.length; i++) {
    name = 'Test #' + i + ' v1: ' + tests[i].v1 + ', v2: ' + tests[i].v2;
    print(name);
    JsUnit.assertEquals(name, Util.GNOMEversionCompare (tests[i].v1, tests[i].v2), tests[i].res);
}
