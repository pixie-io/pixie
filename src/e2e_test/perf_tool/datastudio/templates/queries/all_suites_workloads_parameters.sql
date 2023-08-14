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
    ("main" IN UNNEST(tag_array))
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
    suite,
    workload,
    array_to_string(array_agg(parameter), ",") AS parameters,
  FROM (
    SELECT
      experiment_id,
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
  GROUP BY experiment_id, workload, suite
)
SELECT
  COUNT(*) as num_experiments,
  suite,
  workload,
  parameters
FROM suite_workload_and_parameters
GROUP BY suite, workload, parameters
