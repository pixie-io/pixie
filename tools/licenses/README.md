# Licenses
This directory contains the logic to extract and compile our license notice release document.

## All licenses
To get all of the licenses, run the following command:
```
bazel build //tools/licenses:all_licenses
```
And copy the file output by the bazel build command into wherever you need it.

### Github API Key
To ensure that you don't get rate limited by Github, license builds need a Github API Key.
This is read from the GH_API_KEY env var which is set through bazel action_env.

Jenkins is setup with credentials and a bazelrc file to manage this automatically, but if you need
to run a prod build locally that doesn't fail stamping, add `build --action_env=GH_API_KEY=<gh_token>`
with your personal Github access token to your `user.bazelrc` file.

Insturctions to create a token are here:
https://help.github.com/en/articles/creating-a-personal-access-token-for-the-command-line

Note that you don't need to provide any extra permissions, this is used to verify that a repo exists at a public
github URL and avoid [Github's rate limiting](https://developer.github.com/v3/rate_limit/).
