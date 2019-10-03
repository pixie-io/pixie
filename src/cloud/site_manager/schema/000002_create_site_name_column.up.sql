ALTER TABLE sites
ADD COLUMN  site_name VARCHAR(50) UNIQUE;

UPDATE sites
SET site_name = domain_name;
