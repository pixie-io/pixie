# PEM direct-query gRPC endpoint — contract

## Why

Today in-cluster services read PEM data by querying the **vizier-query-broker**
(the standard `ExecuteScript` path). That works and is the primary path. This
feature adds the *node-local* alternative: make the normal `vizier-pem` itself
serve `ExecuteScript` directly over gRPC, so an on-node client can query its
node-local PEM with no broker hop and no cloud dependency.

This capability was already proved in the experimental `standalone_pem`
(`src/experimental/standalone_pem/vizier_server.h` — `px::vizier::agent::VizierServer`
implementing `api::vizierpb::VizierService::ExecuteScript` against a local Carnot).
The two differences for the real PEM:

1. **Metadata-connected.** The normal PEM has the metadata service, so per-pod PxL
   filters (`df[df.ctx['pod'] == ...]`) resolve — the gap that made standalone_pem
   return empty per-pod results. Reuse the PEM's existing Carnot +
   table_store + metadata state; do **not** stand up a second Carnot.
2. **Authenticated.** standalone_pem was insecure (`WithDirectCredsInsecure`). The
   real PEM is in `pl` and must require a **valid cluster service JWT** (the same
   `jwt-signing-key` used by kelvin, query-broker, and metadata-server).

## The endpoint

- Service: `px.api.vizierpb.VizierService` (the generated gRPC service).
- Method implemented: **`ExecuteScript`** (server-streaming). Mutations/tracepoints
  are **out of scope** — return `UNIMPLEMENTED` for `req.mutation()==true`.
  (standalone_pem handles mutations; the read path never mutates.)
- Transport: gRPC over TLS using the in-cluster self-signed CA
  (`SSL::DefaultGRPCServerCreds()`). Insecure fallback only when `PL_DISABLE_SSL=1`
  is explicitly set for a dev/soak cluster.

## Config (flags / env) — gated OFF by default

| flag / env                          | default | meaning                                            |
|-------------------------------------|---------|----------------------------------------------------|
| `--direct_query_enabled` / `PL_PEM_DIRECT_QUERY_ENABLED` | `false` | master switch; when false the port is never opened |
| `--direct_query_port` / `PL_PEM_DIRECT_QUERY_PORT`       | `50305` | gRPC listen port for the direct-query service      |
| `--direct_query_jwt_signing_key` / `PL_JWT_SIGNING_KEY`  | `""`    | HMAC key the bearer JWT must verify against         |

Default-off so existing PEM deployments are byte-for-byte unchanged until opted in.
Opt out at runtime with `PL_PEM_DIRECT_QUERY_ENABLED=false`, or compile it out with
`--//src/vizier/services/agent/pem:direct_query=disabled`.

## Auth contract

Every `ExecuteScript` call MUST present `authorization: Bearer <jwt>` metadata.
The JWT is verified with `PL_JWT_SIGNING_KEY` and must:
- have a valid signature (HS256) against the signing key,
- be unexpired (`exp` in the future),
- carry a service/audience claim acceptable to vizier
  (minted via `GenerateJWTForService` in `src/shared/services/utils`).

Missing/invalid/expired token → `grpc::StatusCode::UNAUTHENTICATED`. No token must
ever fall through to query execution.

## Behavioral contract (the executable spec → `direct_query_server_test.cc`)

1. **flag-off → no listener.** With `direct_query_enabled=false`, nothing listens on
   the port; the PEM starts exactly as today.
2. **flag-on → serves ExecuteScript.** With it enabled + a signing key set, a gRPC
   client with a valid bearer JWT gets a streamed response (status OK) for a trivial
   PxL (e.g. `import px; px.display(px.DataFrame('http_events'))`).
3. **auth required.** Same call with (a) no token, (b) a token signed by the wrong
   key, (c) an expired token → each `UNAUTHENTICATED`, no rows.
4. **metadata-connected filter.** A PxL with a per-pod filter returns only that pod's
   rows (proves the metadata gap from standalone_pem is closed on the real PEM). May
   be an integration test tagged `requires_metadata` if a unit Carnot fixture can't
   supply pod context.
5. **mutation rejected.** `req.mutation()==true` → `UNIMPLEMENTED` (scope guard).
6. **no regression.** The existing PEM agent registration / Carnot / Stirling path is
   unchanged when the flag is off (assert via the existing PEM smoke/unit tests).

## Client integration (informational)

The existing in-cluster clients already have the pieces:
- **JWT mint**: `GenerateJWTForService` in `src/shared/services/utils/jwt.go`; mount
  the `pl-cluster-secrets/jwt-signing-key` via `secretKeyRef`.
- **gRPC metadata**: attach `authorization: Bearer <jwt>` to each call.
- **Address**: `<HOST_IP>:50305` (HOST_IP via downward API, or the pod's node IP).
- **TLS**: use `WithDisableTLSVerification` against the cluster's self-signed CA,
  matching the broker path.

## Done =

`direct_query_server_test.cc` green under `bazel test`, PEM image builds via the
vizier-release workflow, and a live cluster shows an on-node client ruling in an
e2e scenario off the node-local PEM with the same verdict it gets via the broker.
