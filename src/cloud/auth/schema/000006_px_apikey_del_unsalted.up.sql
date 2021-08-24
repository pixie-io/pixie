ALTER TABLE api_keys
  DROP COLUMN unsalted_key;

ALTER TABLE api_keys
  DROP COLUMN key;
