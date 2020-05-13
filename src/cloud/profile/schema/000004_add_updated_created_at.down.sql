ALTER TABLE users
DROP COLUMN created_at;
ALTER TABLE users
DROP COLUMN updated_at;

DROP TRIGGER IF EXISTS update_users_updated_at ON users;
