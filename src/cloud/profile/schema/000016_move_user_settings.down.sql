INSERT INTO user_settings (user_id, key, value)
SELECT user_id, 'tour_seen', tour_seen::varchar(50) FROM user_attributes;

DELETE FROM user_attributes;
