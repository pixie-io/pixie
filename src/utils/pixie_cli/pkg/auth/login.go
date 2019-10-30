package auth

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"html/template"
	"net/http"
	"net/url"
	"os"
	"os/user"
	"path/filepath"
	"strings"
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/skratchdot/open-golang/open"
)

const pixieAuthPath = ".pixie"
const pixieAuthFile = "auth.json"

var errUserChallengeTimeout = errors.New("timeout waiting for user")
var errBrowserFailed = errors.New("browser failed to open")
var localServerRedirectURL = "http://localhost:8085/auth_complete"
var localServerPort = int32(8085)

const authSuccessPage = `
<!DOCTYPE HTML>
<html lang="en-US">
  <head>
    <meta charset="UTF-8">
    <script type="text/javascript">
      window.location.href = "https://{{ .CloudAddr }}/auth_success"
    </script>
    <title>Authentication Successful - Pixie</title>
  </head>
  <body>
    <p><font face=roboto>
      You may close this window.
    </font></p>
  </body>
</html>
`

// EnsureDefaultAuthFilePath returns and creates the file path is missing.
func EnsureDefaultAuthFilePath() (string, error) {
	u, err := user.Current()
	if err != nil {
		return "", err
	}

	pixieDirPath := filepath.Join(u.HomeDir, pixieAuthPath)
	if _, err := os.Stat(pixieDirPath); os.IsNotExist(err) {
		os.Mkdir(pixieDirPath, 0744)
	}

	pixieAuthFilePath := filepath.Join(pixieDirPath, pixieAuthFile)
	return pixieAuthFilePath, nil
}

// SaveRefreshToken saves the refresh token in default spot.
func SaveRefreshToken(token *RefreshToken) error {
	pixieAuthFilePath, err := EnsureDefaultAuthFilePath()
	if err != nil {
		return err
	}

	f, err := os.OpenFile(pixieAuthFilePath, os.O_RDWR|os.O_CREATE, 0600)
	if err != nil {
		return err
	}
	defer f.Close()

	return json.NewEncoder(f).Encode(token)
}

// LoadDefaultCredentials loads the default credentials for the user.
func LoadDefaultCredentials() (*RefreshToken, error) {
	pixieAuthFilePath, err := EnsureDefaultAuthFilePath()
	if err != nil {
		return nil, err
	}
	f, err := os.Open(pixieAuthFilePath)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	token := &RefreshToken{}
	if err := json.NewDecoder(f).Decode(token); err != nil {
		return nil, err
	}
	// TODO(zasgar): Exchange refresh token for new token type.
	return token, nil
}

// PixieCloudLogin performs login on the pixie cloud.
type PixieCloudLogin struct {
	Site       string
	ManualMode bool
	CloudAddr  string
}

// Run either launches the browser or prints out the URL for auth.
func (p *PixieCloudLogin) Run() (*RefreshToken, error) {
	// There are two ways to do the auth. The first one is where we automatically open up the browser
	// and wait for the challenge to complete and call a HTTP server that we started.
	// The second one is to perform a manual auth.
	// Unless manual mode is specified we will try perform the browser based auth and fallback to manual auth.
	var accessToken string
	var err error
	if !p.ManualMode {
		if accessToken, err = p.tryBrowserAuth(); err != nil {
			// Handle errors.
			switch err {
			case errUserChallengeTimeout:
				log.Fatal("Timeout waiting for response from browser. Perhaps try --manual mode.")
			case errBrowserFailed:
				fallthrough
			default:
				log.Info("Failed to perform browser based auth. Will try manual auth")
			}
		}
	}

	if accessToken == "" {
		// Try to request using manual mode
		if accessToken, err = p.getAuthStringManually(); err != nil {
			return nil, err
		}
		log.Info("Fetching refresh token")
	}

	return p.getRefreshToken(accessToken)
}

func (p *PixieCloudLogin) tryBrowserAuth() (string, error) {
	// Browser auth starts up a server on localhost to do the user challenge
	// and get the authentication token.
	authURL := getAuthURL(p.CloudAddr, p.Site)
	q := authURL.Query()
	q.Set("redirect_uri", localServerRedirectURL)
	authURL.RawQuery = q.Encode()

	type result struct {
		Token string
		err   error
	}

	// The token/ error is returned on this channel. A closed channel also implies error.
	results := make(chan result, 1)

	// Template of the page to render when auth succeeds.
	okPageTmpl := template.Must(template.New("okPage").Parse(authSuccessPage))

	mux := http.DefaultServeMux
	// Start up HTTP server to intercept the browser data.
	mux.HandleFunc("/auth_complete", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			results <- result{"", errors.New("wrong method on HTTP request, assuming auth failed")}
			close(results)
			return
		}

		if err := r.ParseForm(); err != nil {
			close(results)
			return
		}

		accessToken := r.Form.Get("access_token")
		if accessToken == "" {
			results <- result{"", errors.New("missing code, assuming auth failed")}
			close(results)
			return
		}

		// Fill out the template with the correct data.
		templateParams := struct {
			CloudAddr string
		}{p.CloudAddr}
		// Write out the page to the handler.
		okPageTmpl.Execute(w, templateParams)

		// Sucessful auth.
		results <- result{accessToken, nil}
		close(results)
	})

	h := http.Server{
		Addr:    fmt.Sprintf(":%d", localServerPort),
		Handler: mux,
	}

	// Start up the server in the background. Wait for either a timeout
	// or completion of the challenge auth.
	go func() {
		if err := h.ListenAndServe(); err != nil {
			if err == http.ErrServerClosed {
				return
			}
			log.WithError(err).Fatal("failed to listen")
		}
	}()

	go func() {
		log.Info("Starting browser")
		err := open.Run(authURL.String())
		if err != nil {
			results <- result{"", errBrowserFailed}
			close(results)
		}
	}()

	ctx, cancel := context.WithTimeout(context.Background(), time.Minute*2)
	defer cancel()
	defer h.Shutdown(ctx)
	for {
		select {
		case <-ctx.Done():
			return "", errUserChallengeTimeout
		case res, ok := <-results:
			if !ok {
				return "", errUserChallengeTimeout
			}
			// TODO(zasgar): This is a hack, figure out why this function takes so long to exit.
			log.Info("Fetching refresh token ...")
			return res.Token, res.err
		}
	}
}

func (p *PixieCloudLogin) getAuthStringManually() (string, error) {
	authURL := getAuthURL(p.CloudAddr, p.Site)
	fmt.Printf("\nPlease Visit: \n \t %s\n\n", authURL.String())
	f := bufio.NewWriter(os.Stdout)
	f.WriteString("Copy and paste token here: ")
	f.Flush()

	r := bufio.NewReader(os.Stdin)
	return r.ReadString('\n')
}

func (p *PixieCloudLogin) getRefreshToken(accessToken string) (*RefreshToken, error) {
	params := struct {
		AccessToken string `json:"accessToken"`
		SiteName    string `json:"siteName"`
	}{
		AccessToken: strings.Trim(accessToken, "\n"),
		SiteName:    p.Site,
	}
	b, err := json.Marshal(params)
	if err != nil {
		return nil, err
	}
	authURL := getAuthAPIURL(p.CloudAddr)
	req, err := http.NewRequest("POST", authURL, bytes.NewBuffer(b))
	req.Header.Set("content-type", "application/json")
	if err != nil {
		return nil, err
	}

	client := http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode == http.StatusUnauthorized {
		return nil, errors.New("invalid token")
	}
	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("Request for token failed with status %d", resp.StatusCode)
	}
	refreshToken := &RefreshToken{}
	if err := json.NewDecoder(resp.Body).Decode(refreshToken); err != nil {
		return nil, err
	}

	return refreshToken, nil
}

// RefreshToken is the format for the refresh token.
type RefreshToken struct {
	Token     string `json:"token"`
	ExpiresAt int64  `json:"expiresAt"`
}

func getAuthURL(cloudAddr, siteName string) *url.URL {
	authURL, err := url.Parse(fmt.Sprintf("https://id.%s", cloudAddr))
	if err != nil {
		log.WithError(err).Fatal("Failed to parse cloud addr.")
	}
	authURL.Path = "/login"
	params := url.Values{}
	params.Add("domain_name", siteName)
	params.Add("local_mode", "true")
	authURL.RawQuery = params.Encode()
	return authURL
}

func getAuthAPIURL(cloudAddr string) string {
	authURL, err := url.Parse(fmt.Sprintf("https://%s/api/auth/login", cloudAddr))
	if err != nil {
		log.WithError(err).Fatal("Failed to parse cloud addr.")
	}
	return authURL.String()
}
