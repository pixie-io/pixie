CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pgcrypto";

CREATE TABLE cron_scripts (
  -- The ID of the cron script.
  id UUID UNIQUE DEFAULT uuid_generate_v4(),
  -- org_id is the org who has this script enabled.
  org_id UUID NOT NULL,
  -- contents contains the actual PxL script.
  script varchar,
  -- cron_expression specifies is how often the script should run using a cron expression.
  cron_expression varchar,
  -- cluster_ids is the list of clusters which this script should run on. If empty, assumes it runs on all clusters in the org.
  cluster_ids UUID[],
  -- Environment variables that should be used to fill in the script, in a YAML format.
  configs bytea,
  -- Token for the user/org for which this script is run on behalf.
  token bytea,
  -- enabled is whether the cron script is enabled.
  enabled boolean,

  PRIMARY KEY (id)
);
