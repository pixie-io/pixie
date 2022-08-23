CREATE TYPE experiment_state AS ENUM (
  'WAITING_TASKS',
  'RUNNING',
  'FINISHED',
  'ERROR',
  'RETRY_BACKOFF'
);

CREATE TABLE experiment_queue (
  -- id is the unique id of an experiment
  id UUID UNIQUE,
  -- serialized_spec is the protobuf serialized experiment spec.
  serialized_spec bytea,
  -- state of the experiment.
  state experiment_state,
  -- num_tasks_waiting is the number of tasks this experiment is waiting for before it can start.
  num_tasks_waiting int,
  -- num_retries is the number of times the experiment has been retried.
  num_retries int,

  PRIMARY KEY(id)
);
