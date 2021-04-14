# arc-tslint

Use [tslint](https://palantir.github.io/tslint/) to lint your Typescript source code with
[Phabricator](http://phabricator.org)'s `arc` command line tool.

## Features

tslint generates warning messages.

Example output:

    >>> Lint for src/index.ts:

     Warning  (quotemark) tslint violation
      ' should be "

                14  *  under the License.
                15  */
                16
      >>>       17 import $$observable from 'symbol-observable';
                18
                19 export default class IndefiniteObservable<T> implements Observable<T> {
                20   _creator: Creator;

     Warning  (semicolon) tslint violation
      Missing semicolon

                71 export type Unsubscribe = () => void;
                72 export type Subscription = {
                73   unsubscribe: Unsubscribe,
      >>>       74 }

## Installation

tclint is required.

    npm install tslint typescript -g

### Project-specific installation

You can add this repository as a git submodule. Add a path to the submodule in your `.arcconfig`
like so:

```json
{
  "load": ["path/to/arc-tslint"]
}
```

### Global installation

`arcanist` can load modules from an absolute path. But it also searches for modules in a directory
up one level from itself.

You can clone this repository to the same directory where `arcanist` and `libphutil` are located.
In the end it will look like this:

```sh
arcanist/
arc-tslint/
libphutil/
```

Your `.arcconfig` would look like

```json
{
  "load": ["arc-tslint"]
}
```

## Setup

To use the linter you must register it in your `.arclint` file.

```json
{
  "linters": {
    "ts": {
      "type": "tslint",
      "include": "(src/.*\\.(ts)$)"
    }
  }
}
```

## License

Licensed under the Apache 2.0 license. See LICENSE for details.
