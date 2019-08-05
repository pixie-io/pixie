#!/bin/bash -ex

# Grab bin data.
go get -u github.com/jteeuwen/go-bindata/...

# Update the schema assets.
go generate
