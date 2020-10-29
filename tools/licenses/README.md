# Licenses
This directory contains the logic to extract and compile our license notice release document.

## All licenses
To get all of the licenses, run the following commands
```
bash tools/licenses/get_go_licenses.sh
bazel build //tools/licenses:all_licenses
```
And copy the file output by the bazel build command into wherever you need it.

## Go licenses

Go licenses must be ran manually as the program must be run on the actual source of the
directory - something that would slow down the overall build. To run it, call
```
bash tools/licenses/get_go_licenses.sh
```
Note: This tool is strongly dependent on having the Pixie repo checked out under `$GOPATH/src/pixielabs.ai/pixielabs`


### Github API Key
The current Github api key is [pixie-labs-buildbot](https://github.com/pixie-labs-buildbot)'s personal access token. If you need to regenerate it, get the credentials for pixie-labs-buildbot and follow these instructions:
https://help.github.com/en/articles/creating-a-personal-access-token-for-the-command-line

and store the updated key in `credentials/api_keys/prod/github.json`
Note that you don't need to provide any extra permissions, this is used to verify that a repo exists at a public
github URL and avoid [Github's rate limiting](https://developer.github.com/v3/rate_limit/)




