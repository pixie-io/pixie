DROP INDEX idx_vizier_deployment_keys_hashed_key;

ALTER TABLE vizier_deployment_keys
  DROP COLUMN hashed_key;

ALTER TABLE vizier_deployment_keys
  DROP COLUMN encrypted_key;
