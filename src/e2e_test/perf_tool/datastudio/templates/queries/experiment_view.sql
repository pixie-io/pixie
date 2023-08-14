WITH json_unnest AS (
  SELECT
    experiment_id,
    json_value(spec, '$.commitSha') as commit_sha,
    json_value_array(spec, '$.tags') as tag_array
  FROM `{{.Project}}.{{.Dataset}}.specs`
  WHERE experiment_id IN UNNEST(SPLIT(@experiment_ids, ","))
),
suite_workload_and_parameters AS (
  SELECT
    experiment_id,
    commit_sha,
    suite,
    workload,
    array_to_string(array_agg(parameter), ",") as parameters
  FROM (
    SELECT
      experiment_id,
      commit_sha,
      regexp_extract(tag1, r'^parameter[/](.+)') as parameter,
      regexp_extract(tag2, r'^workload[/]([^/]+)') as workload,
      regexp_extract(tag3, r'^suite[/]([^/]+)') as suite
    FROM json_unnest
    CROSS JOIN UNNEST(json_unnest.tag_array) tag1
    CROSS JOIN UNNEST(json_unnest.tag_array) tag2
    CROSS JOIN UNNEST(json_unnest.tag_array) tag3
  )
  WHERE
    parameter is not null AND
    workload is not null AND
    suite is not null
  GROUP BY experiment_id, commit_sha, suite, workload
),
actions AS (
  SELECT
    r.experiment_id,
    r.timestamp as action_ts,
    IFNULL(REGEXP_EXTRACT(r.name, '[^:]+:(.*)'), '') as action_name,
    IFNULL(REGEXP_EXTRACT(r.name, '[^:_]+_([^:]+):.*'), 'run') as action_type,
    IFNULL(REGEXP_EXTRACT(r.name, '([^:_]+)_[^:]+:.*'), 'begin') = 'begin' as action_begin,
    s.commit_sha,
    s.parameters,
    s.workload,
    s.suite
  FROM `{{.Project}}.{{.Dataset}}.results` AS r
  JOIN suite_workload_and_parameters AS s USING (experiment_id)
  WHERE r.name LIKE 'begin_%:%' OR r.name LIKE 'end_%:%' OR r.name = 'workloads_deployed'
),
pivoted_results AS (
  SELECT *
  FROM `{{.Project}}.{{.Dataset}}.results`
  PIVOT(
    any_value(value)
    FOR name
      IN (
        {{- $n := len .MetricsUsed -}}
        {{- range $index, $element := .MetricsUsed -}}
          '{{$element}}'
          {{- if add1 $index | ne $n -}}
            ,
          {{- end -}}
        {{- end -}}
      )
  )
  WHERE
    {{$n := len .MetricsUsed -}}
    {{- range $index, $element := .MetricsUsed}}
      {{$element}} is not null
      {{if add1 $index | ne $n -}}
        OR
      {{- end}}
    {{- end}}
),
{{- if ne .CustomPreprocessing "" -}}
{{.CustomPreprocessing}},
{{else}}
-- By default we require all of the MetricsUsed to be valid. So that eg. `heap_size_bytes - table_size` make sense.
preprocessed AS (
  SELECT *
  FROM pivoted_results
  WHERE
    {{$n := len .MetricsUsed -}}
    {{- range $index, $element := .MetricsUsed}}
      {{$element}} is not null
      {{if add1 $index | ne $n -}}
        AND
      {{- end}}
    {{- end}}
),
{{- end}}
all_results AS (
  SELECT
    r.experiment_id,
    r.timestamp as ts,
    {{range $idx, $name := .MetricNames -}}
      {{index $.MetricExprs $idx}} AS {{$name}},
    {{- end}}
    json_value(r.tags, '$.pod') as pod,
    s.commit_sha,
    s.parameters,
    s.workload,
    s.suite
  FROM preprocessed AS r
  JOIN suite_workload_and_parameters AS s USING (experiment_id)
),
with_actions AS (
  SELECT
    experiment_id,
    ts,
    {{range $name := .MetricNames -}}
      {{$name}},
    {{- end}}
    pod,
    commit_sha,
    parameters,
    workload,
    suite,
    NULL as action_ts,
    NULL as action_name,
    NULL as action_type,
    false as action_begin
  FROM all_results
  UNION ALL
  SELECT
    experiment_id,
    NULL as ts,
    {{range $name := .MetricNames -}}
      NULL as {{$name}},
    {{- end}}
    NULL as pod,
    commit_sha,
    parameters,
    workload,
    suite,
    action_ts,
    action_name,
    action_type,
    action_begin
  FROM actions
),
min_time_agg AS (
  SELECT
    experiment_id,
    least(min(ts), min(action_ts)) as min_ts
  FROM with_actions
  GROUP BY experiment_id
),
with_min_time AS (
  SELECT
    r.*,
    agg.min_ts
  FROM with_actions AS r
  LEFT JOIN min_time_agg AS agg USING (experiment_id)
),
relative_timestamps AS (
  SELECT
    experiment_id,
    TIMESTAMP_DIFF(ts, min_ts, SECOND) AS ts,
    {{range $name := .MetricNames -}}
      {{$name}},
    {{- end}}
    pod,
    commit_sha,
    parameters,
    workload,
    suite,
    TIMESTAMP_DIFF(action_ts, min_ts, SECOND) AS action_ts,
    action_name,
    action_type,
    action_begin
  FROM with_min_time
)
SELECT * FROM relative_timestamps
