ALTER TABLE sites
ADD COLUMN domain_name VARCHAR(50) UNIQUE;

UPDATE sites
SET domain_name = site_name;

ALTER TABLE sites
DROP column site_name;
