CREATE TYPE build_state AS ENUM ('WAITING', 'IN_PROGRESS');

CREATE TABLE build_queue (
  -- internal id used to order the queue.
  id SERIAL,
  -- experiment_uuid of this build job
  experiment_uuid UUID,
  -- experiment_retry_idx together with experiment_uuid uniquely identifies this experiment attempt.
  experiment_retry_idx int,
  -- serialized_spec is the protobuf serialized experiment spec. (See experimentpb.ExperimentSpec)
  serialized_spec bytea,
  -- state is the state of the build
  state build_state,

  PRIMARY KEY(id)
);
