Linter for shellcheck

ShellCheck is a comprehensive linter for shell scripts to highlight
syntax issues, seantic problems and more subtle caveats, to avoid
scripts to behave counter-intuitively or stop to work in some odd
conditions.

For more information about ShellCheck, see http://www.shellcheck.net/

**REQUIREMENTS**

This library requires Arcanist and ShellCheck installed.

**INSTALLATION**

Clone this repository or deploy to the same folder where you've
arcanist and libphutil. For example if they live in /opt/phabricator
you should deploy this as /opt/phabricator/shellcheck-linter.

**USE**

1. Add to your .arcconfig the requirements to load the extra library:

```lang=json
  "load": [
    "shellcheck-linter"
  ]
```

Replace the absolute value by a relative path to the folder
if you aren't able to install globally as instructed above:

```lang=json
  "load": [
    "./vendor/shellcheck-linter"
  ]
```

2. Configure .arclint to lint shell:

```
{
    "linters": {
        "shell": {
            "type": "shellcheck",
            "include": [
                "(\\.sh$)"
            ]
        }
    }
}
```

**OPTIONS**

Please see `arc linters shellcheck` for the list of options supported by this
linter.

**TIPS**

As Arcanist currently doesn't detect file formats or doesn't parse
shebangs, you can find useful to enforce a convention scripts MUST
be appended by an extension like .py .sh in your repository.
This may be more convenient for other CI tasks too.

That doesn't mean you have to install them with the extension, both
cp and install commands are happy to accept an arbitray filename as
target, including a target without the extension.

**CREDITS**

  - Joshua Spence: linter development
  - Sébastien Santoro: library packaging, maintainer

**LICENSE**

This linter for Arcanist is released under the Apache 2.0 license.
