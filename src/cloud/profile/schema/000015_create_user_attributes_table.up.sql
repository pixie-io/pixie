CREATE TABLE user_attributes (
  user_id UUID,
  tour_seen boolean DEFAULT false,

  PRIMARY KEY(user_id),
  FOREIGN KEY (user_id) REFERENCES users(id)
);
