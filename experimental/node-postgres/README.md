# Instructions

## Nodejs

The official nodejs version has no debug information.

https://github.com/nodejs/node/blob/master/BUILDING.md#building-a-debug-build
Follow this to build a debug version of nodejs interpreter.

Then you can use the index.js and test.js as the input for debugging:
* index.js: It has a 20 seconds sleep before querying postgres server, which allows stirling_wrapper
  to initialize.
* test.js: It has a single query, which is meant for using with gdb.

## Postgres ssl

The official postgres SSL configuration is quite confusing, we used
https://hub.docker.com/r/nimbustech/postgres-ssl/ instead.

First generate the certs:
```
bazel build src/stirling/source_connectors/socket_tracer/testing/containers/ssl:certs
```
Then copy all of the generated file to `/tmp/ssl`; and follow the instructions in
https://hub.docker.com/r/nimbustech/postgres-ssl/ to change the file permissions accordingly.
(Postgres requires particular permissions for the SSL files, otherwise it wont start)

Then start postgres server:
```
docker run -it --rm --name pg -d -v /tmp/ssl:/var/ssl -e POSTGRES_PASSWORD='password' nimbustech/postgres-ssl:9.5
```

## Launch client test.js and index.js

As showing above, you can run test.js or index.js for your purpose.
Note that you might need to install modules with `npm`, the commands are:
```
npm install --save pg
```
This creates a node-modules directory at `PWD`.

Afterwards run them:
```
# The debug binary was copied to ~/bin as an example
$ gdb --args ~/bin/node.debug test.js
```
