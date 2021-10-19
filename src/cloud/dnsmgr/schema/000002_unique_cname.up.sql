ALTER TABLE ssl_certs
ADD CONSTRAINT unique_cname UNIQUE (cname);
