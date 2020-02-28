# PostgresSQL wire protocol parser

## Wire protocol version

Underlying parsing APIs for PostgresSQL traffic following the wire protocol version 3.
The spec: https://www.postgresql.org/docs/9.5/protocol.html. This doc is for postgres version 9.5,
but the wire protocol was not changed since 2003 when it was launched into production with Postgres
version 7.4. So there should not be any difference between them.

https://www.pgcon.org/2014/schedule/attachments/330_postgres-for-the-wire.pdf is a slide deck with
more succinct summary.

7.4 and newer accounts >98% of users in production according to a survey [1] posted in 2008.
There has not been such survey posted since then. However, given the time span, it is safe to assume
that everyone is using the wire protocol 3.0.

NOTE: An interesting thing is that postgressql is used by cockroachdb as well:
https://www.cockroachlabs.com/docs/stable/architecture/sql-layer.html#postgresql-wire-protocol
So we should save the need for duplicating the work.

## Postgres stats

Postgres records its own stats and exports them into predefined tables. For example, you can use the
query below to access one of the stats database.

```
postgres=# select * from pg_stat_database;
```

DataDog agent can access these tables: https://www.datadoghq.com/blog/postgresql-monitoring-tools/.


[1] [Postgres version survey](https://www.postgresql.org/community/survey/51-what-version-of-postgresql-are-you-running-on-most-of-your-production-postgresql-servers/)
