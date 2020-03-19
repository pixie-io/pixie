UPDATE sites
SET domain_name = site_name;

ALTER TABLE sites
DROP column site_name;
