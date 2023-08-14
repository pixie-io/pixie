preprocessed AS (
  SELECT * FROM
  (
    SELECT
      r1.experiment_id,
      r1.timestamp,
      r1.tags,
      COALESCE(
        r1.cpu_usage,
        IF(r1.cpu_seconds_counter is NULL, NULL,
          (min(r2.cpu_seconds_counter) - r1.cpu_seconds_counter) / TIMESTAMP_DIFF(min(r2.timestamp), r1.timestamp, SECOND))
      ) AS cpu_usage
    FROM pivoted_results AS r1
    LEFT JOIN pivoted_results AS r2
      ON r1.experiment_id = r2.experiment_id AND r1.tags = r2.tags AND r2.timestamp > r1.timestamp
    GROUP BY r1.experiment_id, r1.timestamp, r1.tags, r1.cpu_usage, r1.cpu_seconds_counter
  )
  WHERE cpu_usage is not NULL
)
