# Running Pixie UI Integration Tests

These tests use [Cypress][https://docs.cypress.io] to run integration tests in a browser.

To run them, you need two things:
1. The base URL of a running instance of Pixie's UI. By default, this assumes `dev.withpixie.dev` which comes from the [self-host instructions](https://docs.px.dev/installing-pixie/install-guides/self-hosted-pixie).
2. The Pixie session cookie for a Google-specific login to the Pixie UI you pointed to.
   To get this, load the UI, open the dev tools, and look for the `default-sessionSOME_NUMBER` cookie. That is what you'll use to set `CYPRESS_GOOGLE_SESSION_COOKIE`.
   The name of that cookie will set `CYPRESS_GOOGLE_SESSION_COOKIE_KEY`.

Then, run this command to access Cypress' UI:
`CYPRESS_BASE_URL='https://dev.withpixie.dev' CYPRESS_GOOGLE_SESSION_COOKIE_KEY='default-sessionSOME_NUMBER' CYPRESS_GOOGLE_SESSION_COOKIE='paste-your-session-cookie-value-here' yarn cypress open`

You can use `... yarn cypress:run:chrome` (or `... yarn cypress:run:firefox`) instead if you want to run the tests immediately and headlessly.

If you don't want to set these environment variables every time, you can override everything except the base URL in `cypress.env.json` (copy from `cypress.template.env.json`):
```json
{
  "GOOGLE_SESSION_COOKIE_KEY": "default-sessionSOME_NUMBER",
  "GOOGLE_SESSION_COOKIE": "paste it here"
}
```
