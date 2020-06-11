#!/bin/bash -ex

# Grab bin data.
go get -u github.com/go-bindata/go-bindata/...

# Update the schema assets.
go generate
graphql-schema-typescript generate-ts schema.graphql --output schema.d.ts
