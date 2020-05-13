-- Initially set no default, so that existing rows have a NULL updated_at column
ALTER TABLE vizier_cluster ADD COLUMN updated_at TIMESTAMP;
-- Set the default to NOW() for future rows
ALTER TABLE vizier_cluster ALTER COLUMN updated_at SET DEFAULT now();

CREATE OR REPLACE FUNCTION update_updated_at()
  RETURNS TRIGGER AS $$
  BEGIN
      NEW.updated_at = now();
      RETURN NEW;
  END;
  $$ language 'plpgsql';

CREATE TRIGGER update_vizier_cluster_updated_at
BEFORE UPDATE ON vizier_cluster
FOR EACH ROW EXECUTE PROCEDURE update_updated_at();
