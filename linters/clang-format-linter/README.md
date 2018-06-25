# clang-format-linter

Tired of spending half time of code review on marking code style
issues?

Here we go - this allows to use `clang-format` as a linter for
[arcanist](https://phacility.com/phabricator/arcanist) to enforce style over
your C/C++/Objective-C codebase.

It's not invasive (yet) and at the moment just suggests to autofix
code.

## Installation

What you want to get is to make this module to be available for
`arcanist`. There are a couple of ways to achieve it depending on your
requirements.

### Prerequisites

Right now `clang-format` should be installed beforehand. On OS X you
can do it through [homebrew](https://brew.sh) `brew install
clang-format`.

You also have to configure your style in `.clang-format`
([documentation](http://clang.llvm.org/docs/ClangFormatStyleOptions.html))

Best way is to start from a predefined style by dumping an existing
style `clang-format -style=LLVM -dump-config > .clang-format` and tune
parameters.

There is also a wonderful
[interactive builder](http://clangformat.com/) available.

### Project-specific installation

You can add this repository as a git submodule and in this case
`.arcconfig` should look like:

```json
{
  "load": [
    "path/to/submodule"
  ]
  // ...
}
```

### Global installation
`arcanist` can load modules from an absolute path. But there is one
more trick - it also searches for modules in a directory up one level
from itself.

It means that you can clone this repository to the same directory
where `arcanist` and `libphutil` are located. In the end it should
look like this:

```sh
> ls
arcanist
clang-format-linter
libphutil
```

In this case you `.arcconfig` should look like

```json
{
  "load": [
    "clang-format-linter"
  ]
  // ...
}
```

Another approach is to clone `clang-format-linter` to a fixed location
and use absolute path like:

```sh
cd ~/.dev-tools
git clone https://github.com/vhbit/clang-format-linter
```

```json
{
  "load": [
    "~/.dev-tools/clang-format-linter"
  ]
  // ...
}
```

Both ways of global installation are actually almost equally as in
most cases you'd like to have a bootstrapping script for all tools.

## Setup

Once installed, linter can be used and configured just as any other
`arcanist linter`

Here is a simplest `.arclint`:

```json
{
    "linters": {
        "clang-format": {
            "type": "clang-format",
            "include": "(^Source/.*\\.(m|h|mm)$)"
        },
        // ...
    }
}
```
