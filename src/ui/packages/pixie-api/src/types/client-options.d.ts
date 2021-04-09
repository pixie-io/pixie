/**
 * Options object to pass as the `options` argument to @link{PixieAPIClient#create}.
 * Default behavior is to connect to Pixie Cloud.
 */
export interface PixieAPIClientOptions {
  /**
   * Access token. Required for everyone except Pixie Cloud's Live UI code (which uses withCredentials and passes '').
   */
  apiKey: string;
  /**
   * Where the Pixie API is hosted.
   * Includes the protocol, host, port (if needed), and path.
   * Defaults to window.location.origin, which on Pixie Cloud means https://withpixie.ai.
   */
  uri?: string;
  /**
   * A method to invoke when an API request is denied due to a lack of authorization.
   * @default noop
   */
  onUnauthorized?: (errorMessage?: string) => void;
}
