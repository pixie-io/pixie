CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

CREATE TYPE artifact_type AS ENUM('UNKNOWN',
                                 'LINUX_AMD64',
                                 'DARWIN_AMD64',
                                 'CONTAINER_SET_LINUX_AMD64');

CREATE TABLE artifacts (
  id UUID UNIQUE DEFAULT uuid_generate_v4(),
  -- artifact_name is actually duplicated, but does not seem necessary to pull it out
  -- into another table.
  artifact_name VARCHAR(50),
  create_time TIMESTAMP,
  commit_hash char(40),
  version_str varchar(50),
  available_artifacts artifact_type[],

  PRIMARY KEY(id, artifact_name, version_str),
  UNIQUE(artifact_name, version_str)
);

CREATE TABLE artifact_changelogs (
   artifacts_id UUID,
   changelog TEXT,

   PRIMARY KEY(artifacts_id),
   FOREIGN KEY(artifacts_id) REFERENCES Artifacts(id)
);
