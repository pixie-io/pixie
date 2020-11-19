CREATE TABLE user_settings (
  user_id UUID,
  key varchar(1024),
  value varchar(1024),

  PRIMARY KEY(user_id, key),
  UNIQUE (user_id, key),
  FOREIGN KEY (user_id) REFERENCES users(id)
);
