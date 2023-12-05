# Contributing Guidelines

Pixie welcomes contributions from the community. This document outlines the conventions that should be followed when making a contribution.

## Contact Us

Whether you are a user or contributor, official support channels include:

- [Issues](https://github.com/pixie-io/pixie/issues)
- [Pixie Slack](https://slackin.px.dev)

Before opening a new issue or submitting a new pull request, it's helpful to search the project -
it's likely that another user has already reported the issue you're facing, or it's a known issue
that we're already aware of. It is also worth asking on the Slack channels.

## Where to start?

You can contribute to Pixie in many ways. This includes, but is not limited to:

- Bug and feature reports
- Documentation
- Development of features and bug fixes

If you are interested in helping us shape our community, you can also [apply here](https://px.dev/community/) to be a Pixienaut.

## Contribution Process

### Reporting Bugs and Creating Issues

Reporting bugs is one of the most helpful ways to contribute to Pixie. Bugs may be reported by filing a Github issue in the appropriate repository. For bugs regarding Pixie, file an issue in the `pixie` repo. For reporting inaccurate documentation, file an issue in the `docs.px.dev` repo, etc. Please follow the template when filing an issue and provide as much information as possible.

Before reporting a bug, we encourage you to search the existing Github issues to ensure that the bug has not already been filed.

### Code Contributions

The project is still in its early stages, and we are a small team actively working on delivering our roadmap. For all changes, regardless of size, please create a Github issue that details the bug or feature being addressed before submitting a pull request. In the Github issue, contributors may discuss the viability of the solution, alternatives, and considerations.

#### Contribution Flow

1. Steps to making a code contribution to Pixie will generally look like the following:
2. Fork the repository on Github.
3. Create a new branch.
4. Make your changes in organized commits.
5. Push your branch to your fork.
6. Submit a pull request to the original repository.
7. Make any changes as requested by the maintainers.
8. Once accepted by a maintainer, it will be merged into the original repository by a maintainer.

#### Contribution Checklist

When making a contribution to the repository, please ensure that the following is addressed.

1. Code follows Pixie’s coding style guide.
2. All existing tests must pass, and new tests must be added for the bug/feature in question.
3. Commits are signed (see notes below).

#### Coding Style

Please refer to the style guide directory for more details.

#### Commit Messages

Commit messages should provide enough information about what has changed and why. Please follow the templates for how this information should be detailed.

#### Sign your commits

The sign-off is a simple line at the end of the explanation for a commit. All commits needs to be
signed and verified. Your signature certifies that you wrote the patch or otherwise have the right to contribute
the material. The rules are pretty simple, if you can certify the below (from
[developercertificate.org](https://developercertificate.org/)):

```
Developer Certificate of Origin
Version 1.1

Copyright (C) 2004, 2006 The Linux Foundation and its contributors.
1 Letterman Drive
Suite D4700
San Francisco, CA, 94129

Everyone is permitted to copy and distribute verbatim copies of this
license document, but changing it is not allowed.

Developer's Certificate of Origin 1.1

By making a contribution to this project, I certify that:

(a) The contribution was created in whole or in part by me and I
    have the right to submit it under the open source license
    indicated in the file; or

(b) The contribution is based upon previous work that, to the best
    of my knowledge, is covered under an appropriate open source
    license and I have the right under that license to submit that
    work with modifications, whether created in whole or in part
    by me, under the same open source license (unless I am
    permitted to submit under a different license), as indicated
    in the file; or

(c) The contribution was provided directly to me by some other
    person who certified (a), (b) or (c) and I have not modified
    it.

(d) I understand and agree that this project and the contribution
    are public and that a record of the contribution (including all
    personal information I submit with it, including my sign-off) is
    maintained indefinitely and may be redistributed consistent with
    this project or the open source license(s) involved.
```

Then you just add a line to every git commit message:

    Signed-off-by: Joe Smith <joe.smith@example.com>

Use your real name (sorry, no pseudonyms or anonymous contributions.)

##### Configuring Commit Signing in Git
1. If you set your `user.name` and `user.email` git configs, you can sign your commit with `git commit -s`.

    Note: If your git config information is set properly then viewing the `git log` information for your commit will look something like this:

    ```
    Author: Joe Smith <joe.smith@example.com>
    Date:   Thu Feb 2 11:41:15 2018 -0800

        Update README

        Signed-off-by: Joe Smith <joe.smith@example.com>
    ```

    Notice the `Author` and `Signed-off-by` lines match. If they don't your PR will be rejected by the automated DCO check.

##### Commit Signature Verification

All commit signatures must be verified to ensure that commits are coming from a trusted source. To do so, please follow Github's [Signing commits guide](https://docs.github.com/en/authentication/managing-commit-signature-verification/signing-commits).

##### Setup pinentry-tty to handle headless environments

When signing commits, gpg will need your passphrase to unlock the keyring. It usually uses a GUI pinentry program to do so. However on headless environments, this gets messy, so we highly recommend switching gpg to use a tty based pinentry to ease these problems.

```
echo "pinentry-program $(which pinentry-tty)" >> ~/.gnupg/gpg-agent.conf
echo "export GPG_TTY=\$(tty)" >> ~/.zshrc
echo "export GPG_TTY=\$(tty)" >> ~/.bashrc
export GPG_TTY=$(tty)
gpg-connect-agent reloadagent /bye
```

Here we tell `gpg` to use `pinentry-tty` when prompting for a passphrase, and export the current TTY to tell `gpg` which TTY to prompt on.
