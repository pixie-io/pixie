Environment setup:
```
export PX_DIRECT_VIZIER_ADDR=localhost:12345
export PX_DISABLE_SSL=true
export PX_DIRECT_VIZIER_KEY=test
```

To run the standalone pem:
```
sudo bazel-bin/src/experimental/standalone_pem/standalone_pem
```

Using the `px` cli to query the standalone pem:
```
px run -f pxl/http.pxl
```
If you want data in other formats you can add `-o json`, etc to change the formats.

If you want to export data to NR using OTEL you can use `otel.pxl` to do this.
```
px run -f pxl/otel.pxl
```

To access a sample of custom BPF code, you can run the included tp.pxl. This will snoop all process executions.
```
px run -f pxl/tp.pxl
```

To fully static link (experimental), invoke as follows:
```
bazel build //src/experimental/standalone_pem:standalone_pem --//src/experimental/standalone_pem:full_static=True
```
