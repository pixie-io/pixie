-- These cols will replace "previous_vizier_status" and "previous_vizier_status_time"
ALTER TABLE vizier_cluster_info
  ADD COLUMN prev_status vizier_status;

ALTER TABLE vizier_cluster_info
  ADD COLUMN prev_status_time TIMESTAMP;

ALTER TABLE vizier_cluster_info
  ADD COLUMN prev_status_message TEXT;

CREATE OR REPLACE FUNCTION update_prev_state()
  RETURNS TRIGGER AS $$
  BEGIN
    IF NEW.status <> OLD.status THEN
       NEW.prev_status = OLD.status;
       NEW.prev_status_time = OLD.last_heartbeat;
       NEW.prev_status_message = OLD.status_message;
    END IF;

    RETURN NEW;
  END;
  $$ language 'plpgsql';

CREATE TRIGGER update_vizier_cluster_info_prev_state
  BEFORE UPDATE ON vizier_cluster_info
  FOR EACH ROW EXECUTE PROCEDURE update_prev_state();
