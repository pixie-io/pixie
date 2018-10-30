# Locust
We use Locust, a Python library, to simulate network traffic patterns on our demo applications.
See the "Demo Applications" design doc for further details about Locust and the wrapper we've written around Locust.

# Creating a Load
If you want to create a new load for a demo application, follow the instructions below.
## Locust File Template
Locust requires a locustFile which specifies the types and behaviors of users accessing the
application in the load test. Since the ratios and min/max wait times for the user types may change in each phase,
our wrapper needs a different locustFile for each phase.
To do so, you must provide a template version of the locustFile, which includes the tasks and behaviors of users,
but not the actual user class definitions.
For an example of a locustFile template, see:
```
/demos/applications/hipster_shop/load_generation/locustfile.loadgen.tmpl
```
In general, for consistency, your locustfile template should be saved in
`demos/applications/<application_name>/load_generation/locustfile.loadgen.tmpl`.

## Load Configuration
Our Locust wrapper expects a configuration file to be provided in the form of a pbtxt file.
This file details the phase configurations for the load, and specifies the path to the locust file template.
See `demos/load_generation/proto/load_config.proto` for the protobuf definitions.

For each user type, you must have the corresponding user behavior defined in your locustFile template.
For example, for the user:
```
  user_types {
    name: "Weekend"
    ratio: 2
    min_wait_ms: 1000
    max_wait_ms: 1000
  }
```
The locustFile template should have a `class WeekendUserBehavior(TaskSet):` defined.

For an example config, see:
```
/demos/applications/hipster_shop/load_generation/load.pbtxt
```
In general, for consistency, your locustfile template should be saved in
`demos/applications/<application_name>/load_generation/loadpbtxt`.

# Running a Load Locally
You may want to run your load locally, perhaps for testing purposes. To do so, run the following command:
```
bazel run //demos/load_generation:load_generation --
--config_file ~/go/src/pixielabs.ai/pixielabs/demos/applications/<application_name>/load_generation/load.pbtxt
--host <demo-host>
```
