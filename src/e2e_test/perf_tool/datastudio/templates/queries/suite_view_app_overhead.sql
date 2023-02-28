WITH json_unnest AS (
  SELECT * FROM (
    SELECT
      experiment_id,
      commit_topo_order,
      spec,
      json_value(spec, '$.commitSha') as commit_sha,
      cast(json_value(spec, '$.clusterSpec.numNodes') as int64) as num_nodes,
      json_value_array(spec, '$.tags') as tag_array
    FROM `{{.Project}}.{{.Dataset}}.specs`
  )
  WHERE
    num_nodes = 1 AND
    ("main" IN UNNEST(tag_array)) AND
    ("application_overhead" IN UNNEST(tag_array))
),
specs_filtered AS (
  SELECT * FROM (
    SELECT
      s1.experiment_id,
      s1.commit_topo_order,
      s1.commit_sha,
      json_value_array(s1.spec, '$.tags') as tag_array,
      count(distinct s2.commit_sha) as commits_ahead
    FROM json_unnest as s1
    LEFT JOIN json_unnest as s2
      ON s2.commit_topo_order > s1.commit_topo_order
    GROUP BY s1.experiment_id, s1.commit_topo_order, s1.commit_sha, s1.spec
  )
  WHERE
    commits_ahead < (@num_commits * @show_every_n_commits)
    AND mod(commits_ahead, @show_every_n_commits) = 0
),
suite_workload_and_parameters AS (
  SELECT
    experiment_id,
    commit_topo_order,
    commit_sha,
    suite,
    workload,
    array_to_string(array_agg(parameter), ",") AS parameters,
  FROM (
    SELECT
      experiment_id,
      commit_topo_order,
      commit_sha,
      regexp_extract(tag1, r'^parameter[/](.+)') as parameter,
      regexp_extract(tag2, r'^workload[/]([^/]+)') as workload,
      regexp_extract(tag3, r'^suite[/]([^/]+)') as suite
    FROM specs_filtered
    CROSS JOIN UNNEST(specs_filtered.tag_array) tag1
    CROSS JOIN UNNEST(specs_filtered.tag_array) tag2
    CROSS JOIN UNNEST(specs_filtered.tag_array) tag3
  )
  WHERE
    parameter is not null AND
    workload is not null AND
    suite is not null
  GROUP BY experiment_id, commit_sha, workload, suite, commit_topo_order
),
all_actions AS (
  SELECT
    r.experiment_id,
    r.timestamp as action_ts,
    REGEXP_EXTRACT(r.name, '[^:]+:(.*)') as action_name,
    REGEXP_EXTRACT(r.name, '[^:_]+_([^:]+):.*') as action_type,
    REGEXP_EXTRACT(r.name, '([^:_]+)_[^:]+:.*') = 'begin' as action_begin,
  FROM `{{.Project}}.{{.Dataset}}.results` AS r
  JOIN suite_workload_and_parameters AS s USING (experiment_id)
  WHERE r.name LIKE 'begin_%:%' OR r.name LIKE 'end_%:%'
),
running_timestamps AS (
  SELECT
    a1.experiment_id,
    min(a1.action_ts) AS no_vizier_begin_ts,
    min(a2.action_ts) AS no_vizier_end_ts,
    min(a3.action_ts) AS with_vizier_begin_ts,
    min(a4.action_ts) AS with_vizier_end_ts
  FROM all_actions AS a1
  JOIN all_actions AS a2 USING (experiment_id)
  JOIN all_actions AS a3 USING (experiment_id)
  JOIN all_actions AS a4 USING (experiment_id)
  WHERE
    a1.action_type = 'run' AND
    a1.action_name = 'no_vizier' AND
    a1.action_begin AND
    a2.action_type = 'run' AND
    a2.action_name = 'no_vizier' AND
    NOT a2.action_begin AND
    a3.action_type = 'run' AND
    a3.action_name = 'with_vizier' AND
    a3.action_begin AND
    a4.action_type = 'run' AND
    a4.action_name = 'with_vizier' AND
    NOT a4.action_begin
  GROUP BY a1.experiment_id
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
  WHERE {{$n := len .MetricsUsed -}}
    {{- range $index, $element := .MetricsUsed}}
      {{$element}} is not null
      {{if add1 $index | ne $n -}}
        OR
      {{- end -}}
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
joined_results AS (
  SELECT
    r.experiment_id,
    r.timestamp as ts,
    {{.MetricSelectExpr}} AS {{.MetricName}},
    rtrim(
      regexp_extract(
        json_value(r.tags, '$.pod'),
        r'^(?:[a-z\-]+[/])?((?:[a-z]+\-)+)(?:\-?(?:[a-z]+[0-9]|[0-9]+[a-z])[a-z0-9]*)*?'
      ),
      "-"
    ) as pod,
    s.commit_sha,
    s.commit_topo_order,
    s.parameters,
    s.workload,
    s.suite
  FROM preprocessed AS r
  JOIN suite_workload_and_parameters AS s USING (experiment_id)
),
with_running_timestamps AS (
  SELECT
    r.experiment_id,
    r.ts,
    r.{{.MetricName}},
    r.pod,
    r.commit_sha,
    r.commit_topo_order,
    r.parameters,
    r.workload,
    r.suite,
    (
      TIMESTAMP_DIFF(r.ts, t.no_vizier_begin_ts, SECOND) >=0 AND
      TIMESTAMP_DIFF(t.no_vizier_end_ts, r.ts, SECOND) >= 0
    ) AS no_vizier,
    (
      TIMESTAMP_DIFF(r.ts, t.with_vizier_begin_ts, SECOND) >=0 AND
      TIMESTAMP_DIFF(t.with_vizier_end_ts, r.ts, SECOND) >= 0
    ) AS with_vizier,
    t.with_vizier_begin_ts,
    t.with_vizier_end_ts
  FROM joined_results AS r
  LEFT JOIN running_timestamps AS t USING (experiment_id)
),
min_time_agg AS (
  SELECT
    experiment_id,
    min(ts) as min_ts
  FROM with_running_timestamps
  GROUP BY experiment_id
),
with_min_time AS (
  SELECT
    r.*,
    agg.min_ts
  FROM with_running_timestamps AS r
  LEFT JOIN min_time_agg AS agg USING (experiment_id)
),
time_agg AS (
  SELECT
    experiment_id,
    suite,
    workload,
    parameters,
    commit_sha,
    commit_topo_order,
    pod,
    no_vizier,
    with_vizier,
    {{.TimeAgg}} as {{.MetricName}}
  FROM with_min_time
  GROUP BY experiment_id, suite, workload, parameters, commit_sha, commit_topo_order, pod, no_vizier, with_vizier
),
no_vizier_results AS (
  SELECT *
  FROM time_agg
  WHERE no_vizier
),
with_vizier_results AS (
  SELECT *
  FROM time_agg
  WHERE with_vizier
),
overhead AS (
  SELECT
    no_viz.experiment_id,
    no_viz.suite,
    no_viz.workload,
    no_viz.parameters,
    no_viz.commit_sha,
    no_viz.commit_topo_order,
    no_viz.pod,
    (with_viz.{{.MetricName}} - no_viz.{{.MetricName}}) / no_viz.{{.MetricName}} AS overhead_percent
  FROM no_vizier_results as no_viz
  JOIN with_vizier_results as with_viz USING (experiment_id, suite, workload, parameters, commit_sha, commit_topo_order, pod)
),
agged AS (
  SELECT
    COUNT(DISTINCT experiment_id) as experiment_count,
    array_agg(DISTINCT experiment_id) as experiment_ids,
    suite,
    workload,
    parameters,
    commit_sha,
    commit_topo_order,
    pod,
    approx_quantiles(overhead_percent, 100) as quantiles
  FROM overhead
  GROUP BY suite, workload, parameters, commit_sha, commit_topo_order, pod
)
SELECT
  experiment_count,
  experiment_ids,
  (
    -- the params have to be URL encoded, so this is a bit ugly.
    'https://datastudio.google.com/reporting/{{.DSReportID}}/page/{{.DSExperimentPageID}}?params=%7B%22experiment_ids%22%3A%22'
    || array_to_string(experiment_ids, "%2C")
    ||'%22%7D'
  ) AS experiment_link,
  suite,
  workload,
  parameters,
  commit_sha,
  commit_topo_order,
  pod,
  quantiles[OFFSET(1)] AS overhead_percent_p01,
  quantiles[OFFSET(25)] AS overhead_percent_p25,
  quantiles[OFFSET(50)] AS overhead_percent_p50,
  quantiles[OFFSET(75)] AS overhead_percent_p75,
  quantiles[OFFSET(99)] AS overhead_percent_p99
FROM agged
