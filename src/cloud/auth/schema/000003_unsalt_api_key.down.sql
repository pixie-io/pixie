DROP INDEX idx_api_keys_unsalted;

ALTER TABLE api_keys
DROP COLUMN unsalted_key;
