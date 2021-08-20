ALTER TABLE api_keys
  ADD COLUMN encrypted_key bytea;

-- Hashed key stores a salted and hashed key that we can use for associative lookup.
ALTER TABLE api_keys
  ADD COLUMN hashed_key bytea;

CREATE INDEX idx_api_keys_hashed_key
  ON api_keys(hashed_key);
