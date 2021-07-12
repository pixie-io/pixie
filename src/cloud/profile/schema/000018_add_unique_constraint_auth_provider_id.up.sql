ALTER TABLE users
ADD CONSTRAINT auth_provider_id_unique UNIQUE (auth_provider_id);
