ALTER TABLE orgs
DROP COLUMN created_at;
ALTER TABLE orgs
DROP COLUMN updated_at;

DROP TRIGGER IF EXISTS update_orgs_updated_at ON orgs;
