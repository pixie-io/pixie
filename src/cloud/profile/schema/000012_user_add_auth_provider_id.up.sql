-- auth_provider_id is the user ID that an auth_provider uses for an ID. That way if a user signs in with a different
-- email but the same provider, we know to link that sign up with this particular user.
ALTER TABLE users
ADD COLUMN auth_provider_id varchar(50) DEFAULT '';
