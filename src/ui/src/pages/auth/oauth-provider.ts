export type Token = string;
// OAuthProviderClient is the interface for OAuth providers such as Auth0 and ORY/Hydra.
export abstract class OAuthProviderClient {
  // loginRequest starts the login process for the OAuthProvider by redirecting the window.
  abstract loginRequest(): void;

  // SignupRequest starts the signup process for the OAuthProvider by redirecting the window.
  abstract signupRequest(): void;

  // handleToken will get the token wherever it's stored by the OAuthProvider and pass it to the callback.
  abstract handleToken(): Promise<Token>;
}
