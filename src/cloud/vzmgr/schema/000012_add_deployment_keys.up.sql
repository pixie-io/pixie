CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- This table contains keys that can be used to deploy a new vizier.
CREATE TABLE vizier_deployment_keys (
  -- The ID to use for this cluster.
  id UUID UNIQUE DEFAULT uuid_generate_v4(),
  -- org_id is the owner of this key.
  org_id UUID NOT NULL,
  -- user_id that is the owner of this key.
  user_id UUID NOT NULL,
  -- Timestamp when this key was created.
  created_at TIMESTAMP DEFAULT NOW(),

  -- The actual key salted.
  key varchar(1000),

  UNIQUE(key),
  PRIMARY KEY(id)
);
