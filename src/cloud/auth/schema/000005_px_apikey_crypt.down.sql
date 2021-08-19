DROP INDEX idx_api_keys_hashed_key;

ALTER TABLE api_keys
  DROP COLUMN hashed_key;

ALTER TABLE api_keys
  DROP COLUMN encrypted_key;
