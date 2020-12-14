CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- This table contains keys that can be used to access the Pixie API.
CREATE TABLE api_keys (
  -- The ID to use for this key.
  id UUID UNIQUE DEFAULT uuid_generate_v4(),
  -- org_id is the ID belonging to the org in which the API key is visible.
  org_id UUID NOT NULL,
  -- user_id that is the owner of this key.
  user_id UUID NOT NULL,
  -- Timestamp when this key was created.
  created_at TIMESTAMP DEFAULT NOW(),
  -- Description of the key. Can be empty.
  description varchar(1000),
  -- The actual key salted.
  key varchar(1000),

  UNIQUE(key),
  PRIMARY KEY(id)
);
