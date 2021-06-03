-- Initially set no default, so that existing rows have a NULL created_at column
ALTER TABLE orgs ADD COLUMN created_at TIMESTAMP;
-- Set the default to NOW() for future rows
ALTER TABLE orgs ALTER COLUMN created_at SET DEFAULT now();
-- Initially set no default, so that existing rows have a NULL updated_at column
ALTER TABLE orgs ADD COLUMN updated_at TIMESTAMP;
-- Set the default to NOW() for future rows
ALTER TABLE orgs ALTER COLUMN updated_at SET DEFAULT now();

CREATE OR REPLACE FUNCTION update_updated_at()
  RETURNS TRIGGER AS $$
  BEGIN
      NEW.updated_at = now();
      RETURN NEW;
  END;
  $$ language 'plpgsql';

CREATE TRIGGER update_orgs_updated_at
BEFORE UPDATE ON orgs
FOR EACH ROW EXECUTE PROCEDURE update_updated_at();
