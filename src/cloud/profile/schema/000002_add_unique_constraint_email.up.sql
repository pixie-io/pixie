ALTER TABLE users
ADD CONSTRAINT email_unique UNIQUE (email);
