-- Set default value to 'google-oauth2' because this is the main
-- supported identity provider from Auth0. OSS Cloud users with OSS Auth (hydra/kratos)
-- will have to manually rerun this command with 'kratos' if they deployed
-- before this migration existed.
UPDATE users SET identity_provider = 'google-oauth2';
