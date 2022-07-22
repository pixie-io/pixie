_Privy_ is a command line tool for generating synthetic protocol traces similar to those that Pixie collects from pods in a cluster. It uses [OpenAPI descriptors](https://swagger.io/resources/open-api/) and fake data sources to produce full-body messages in a variety of formats:
- `JSON`
- `XML`
- `SQL` (in progress)
- `Protobuf` (in progress)

This data may be used for demo purposes, to train PII detection models, and to evaluate existing PII identification systems.

# Quickstart

1. Generate json data in bazel's sandboxed runtime directory, specifying a folder containing OpenAPI specs to generate data from. By default, privy downloads API specs from the [OpenAPI directory](https://github.com/APIs-guru/openapi-directory) to the path provided in `api_specs`. Privy checks if this folder already exists.
```bazel
bazel run //privy/run:privy_run -- --api_specs=/path/to/openapi/specs
```

2. [Optional] Specify an absolute path to store synthetic data in with `--out_folder`, the type of data to generate with `--generate`, and log priority level with `--logging`. For brevity, you can also use the first letter of each option.
```bazel
bazel run //privy/run:privy_run -- --out_folder=/path/to/output/directory --generate=json --logging=debug
```

## Get help
```
options:
  --generate {json,sql,proto,xml}, -g {json,sql,proto,xml}
                        Which dataset to generate. (default: json)
  --logging {debug,info,warning,error}, -l {debug,info,warning,error}
                        logging level: debug, info, warning, error (default: info)
  --out_folder OUT_FOLDER, -o OUT_FOLDER
                        Absolute path to output folder. By default, saves to bazel cache for this runtime.
  --api_specs API_SPECS, -a API_SPECS
                        Absolute path to folder download openapi specs into. Privy checks if this folder already exists. (default: None)
```
