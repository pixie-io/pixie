#!/bin/bash -ex

# Grab bin data.
go get -u github.com/go-bindata/go-bindata/...

# Update the schema assets.
go generate
