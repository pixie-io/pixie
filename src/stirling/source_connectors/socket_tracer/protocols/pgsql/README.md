# PostgresSQL wire protocol parser

## Wire protocol version

The code here is based on Postgres wire protocol version 3. The spec can be found
[here](https://www.postgresql.org/docs/9.5/protocol.html). The linked spec is from Postgres version 9.5,
but the wire protocol was not changed since 2003 when it was launched into production with Postgres
version 7.4). So there should not be any difference between them.

[Postgres on the wire](https://www.pgcon.org/2014/schedule/attachments/330_postgres-for-the-wire.pdf)
is a slide deck with more succinct summary.

According to a [survey](https://www.postgresql.org/community/survey/51-what-version-of-postgresql-are-you-running-on-most-of-your-production-postgresql-servers/) conducted in 2008, 7.4 and newer accounts >98% of users in production.
There has not been such survey posted since then. However, given the time span, it is safe to assume
that everyone is using the wire protocol 3.0.

## Postgres stats

Postgres records its own stats and exports them into predefined tables. For example, you can use the
query below to access one of the stats database.

```
postgres=# select * from pg_stat_database;
```

DataDog agent can [read metrics from this table](https://www.datadoghq.com/blog/postgresql-monitoring-tools/).

## Notes
Cockroachdb also [implements Postgres wire protocol](
https://www.cockroachlabs.com/docs/stable/architecture/sql-layer.html#postgresql-wire-protocol).
