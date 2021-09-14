INSERT INTO user_attributes (user_id)
SELECT id FROM users;

UPDATE user_attributes SET tour_seen=s.value::boolean FROM user_settings as s
  WHERE user_attributes.user_id = s.user_id;

DELETE FROM user_settings;
