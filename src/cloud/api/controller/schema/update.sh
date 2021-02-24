#!/bin/bash -ex

# Grab bin data.
go get -u github.com/go-bindata/go-bindata/...

# Update the schema assets.
go generate

# The following assumes a few things:
# - yarn appears in $PATH (likely from running `npm i -g yarn` after getting NodeJS set up)
# - The package `graphql-schema-typescript` still has carriage returns in its bin script, breaking it on not-Windows.
# - `yarn install` was already run in //src/ui
# - The environment running this script can run Bash (if the top of this file is any indication, it can).
schemaRoot="$(cd "$(dirname "$0")" && pwd)" # dirname $0 can come back as just `.`, resolve it to a real path.
uiRoot="${schemaRoot}/../../../../ui"
pushd "${uiRoot}" && yarn workspace @pixie/api regenerate_graphql_schema
popd && cp "${uiRoot}/packages/pixie-api/src/types/schema.d.ts" "${schemaRoot}/schema.d.ts"
