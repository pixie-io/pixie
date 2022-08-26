CREATE TABLE build_artifacts (
  experiment_uuid UUID UNIQUE,
  serialized_artifacts bytea,
  last_access_time timestamp
);
