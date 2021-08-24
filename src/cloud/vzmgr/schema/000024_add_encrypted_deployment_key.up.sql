ALTER TABLE vizier_deployment_keys
  ADD COLUMN encrypted_key bytea;

-- Hashed key stores a salted and hashed key that we can use for associative lookup.
ALTER TABLE vizier_deployment_keys
  ADD COLUMN hashed_key bytea;

CREATE INDEX idx_vizier_deployment_keys_hashed_key
  ON vizier_deployment_keys(hashed_key);
