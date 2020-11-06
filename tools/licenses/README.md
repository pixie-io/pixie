# Licenses
This directory contains the logic to extract and compile our license notice release document.

## All licenses
To get all of the licenses, run the following command:
```
bazel build //tools/licenses:all_licenses
```
And copy the file output by the bazel build command into wherever you need it.

### Github API Key
The current Github api key is [pixie-labs-buildbot](https://github.com/pixie-labs-buildbot)'s personal access token.
If you need to regenerate it, get the credentials for pixie-labs-buildbot and follow these instructions:
https://help.github.com/en/articles/creating-a-personal-access-token-for-the-command-line
and store the updated key in `credentials/dev_infra/github_token.txt`.

Note that you don't need to provide any extra permissions, this is used to verify that a repo exists at a public
github URL and avoid [Github's rate limiting](https://developer.github.com/v3/rate_limit/).
