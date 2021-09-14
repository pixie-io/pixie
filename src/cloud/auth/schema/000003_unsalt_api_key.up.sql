ALTER TABLE api_keys
ADD COLUMN unsalted_key varchar(1000);

CREATE INDEX idx_api_keys_unsalted
ON api_keys(unsalted_key);
