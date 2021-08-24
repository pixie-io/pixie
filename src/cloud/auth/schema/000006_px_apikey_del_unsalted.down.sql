ALTER TABLE api_keys
  ADD COLUMN unsalted_key varchar;

ALTER TABLE api_keys
  ADD COLUMN key varchar;
