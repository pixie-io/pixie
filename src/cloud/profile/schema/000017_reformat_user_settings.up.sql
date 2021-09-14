DROP TABLE user_settings;

CREATE TABLE user_settings (
  user_id UUID,
  analytics_optout boolean DEFAULT false,

  PRIMARY KEY(user_id),
  FOREIGN KEY (user_id) REFERENCES users(id)
);

INSERT INTO user_settings (user_id)
SELECT id FROM users;
