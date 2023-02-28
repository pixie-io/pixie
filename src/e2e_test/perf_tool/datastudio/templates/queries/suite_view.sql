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
    ("application_overhead" NOT IN UNNEST(tag_array))
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
run_begin_end AS (
  SELECT
    a1.experiment_id,
    min(a1.action_ts) AS run_begin_ts,
    min(a2.action_ts) AS run_end_ts,
  FROM all_actions AS a1
  JOIN all_actions AS a2 USING (experiment_id)
  WHERE
    a1.action_type = 'run' AND
    a1.action_begin AND
    a2.action_type = 'run' AND
    NOT a2.action_begin
  GROUP BY a1.experiment_id
  UNION ALL
  SELECT
    experiment_id,
    timestamp AS run_begin_ts,
    -- for legacy experiments that used `workloads_deployed`, set the end time as 1 year after the start.
    -- this is only used to limit results, so it doesn't matter as long as no experiments go for longer than a year.
    TIMESTAMP_ADD(timestamp, INTERVAL 365 DAY) AS run_end_ts
  FROM `{{.Project}}.{{.Dataset}}.results`
  WHERE name = 'workloads_deployed'
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
        r'^pl[/]((?:[a-z]+\-)+)(?:\-?(?:[a-z]+[0-9]|[0-9]+[a-z])[a-z0-9]*)*?'
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
with_begin_end AS (
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
    begin_end.run_begin_ts,
    begin_end.run_end_ts,
  FROM joined_results AS r
  LEFT JOIN run_begin_end AS begin_end USING (experiment_id)
),
min_time_agg AS (
  SELECT
    experiment_id,
    min(ts) as min_ts
  FROM with_begin_end
  GROUP BY experiment_id
),
with_min_time AS (
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
    r.run_begin_ts,
    r.run_end_ts,
    agg.min_ts
  FROM with_begin_end AS r
  LEFT JOIN min_time_agg AS agg USING (experiment_id)
),
ignore_burnin AS (
  SELECT *
  FROM with_min_time
  WHERE TIMESTAMP_DIFF(ts, run_begin_ts, SECOND) >= 0 AND
    TIMESTAMP_DIFF(run_end_ts, ts, SECOND) >= 0
),
{{- if ne .TimeAgg "" -}}
time_agg AS (
  SELECT
    experiment_id,
    suite,
    workload,
    parameters,
    commit_sha,
    commit_topo_order,
    pod,
    {{.TimeAgg}} as {{.MetricName}}
  FROM ignore_burnin
  GROUP BY experiment_id, suite, workload, parameters, commit_sha, commit_topo_order, pod
),
{{- end -}}
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
    approx_quantiles({{.MetricName}}, 100) as quantiles
  FROM {{if ne .TimeAgg ""}}time_agg{{else}}ignore_burnin{{end}}
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
  quantiles[OFFSET(1)] AS {{.MetricName}}_p01,
  quantiles[OFFSET(25)] AS {{.MetricName}}_p25,
  quantiles[OFFSET(50)] AS {{.MetricName}}_p50,
  quantiles[OFFSET(75)] AS {{.MetricName}}_p75,
  quantiles[OFFSET(99)] AS {{.MetricName}}_p99
FROM agged
