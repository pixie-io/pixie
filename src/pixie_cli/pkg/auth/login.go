/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package auth

import (
	"bufio"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"strings"
	"syscall"
	"time"

	"github.com/lestrrat-go/jwx/jwt"
	"github.com/segmentio/analytics-go/v3"
	log "github.com/sirupsen/logrus"
	"github.com/skratchdot/open-golang/open"
	"golang.org/x/term"
	"google.golang.org/grpc/metadata"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/pixie_cli/pkg/pxanalytics"
	"px.dev/pixie/src/pixie_cli/pkg/pxconfig"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
	srvutils "px.dev/pixie/src/shared/services/utils"
	apiutils "px.dev/pixie/src/utils"
)

var errUserChallengeTimeout = errors.New("timeout waiting for user")
var errBrowserFailed = errors.New("browser failed to open")
var errServerListenerFailed = errors.New("failed to start up local server")
var errUserNotRegistered = errors.New("user is not registered. Please sign up")
var localServerRedirectURL = "http://localhost:8085/auth_complete"
var localServerPort = int32(8085)
var sentSegmentAlias = false

// SaveRefreshToken saves the refresh token in default spot.
func SaveRefreshToken(token *RefreshToken) error {
	pixieAuthFilePath, err := utils.EnsureDefaultAuthFilePath()
	if err != nil {
		return err
	}

	f, err := os.OpenFile(pixieAuthFilePath, os.O_RDWR|os.O_CREATE|os.O_TRUNC, 0600)
	if err != nil {
		return err
	}
	defer f.Close()

	return json.NewEncoder(f).Encode(token)
}

// LoadDefaultCredentials loads the default credentials for the user.
func LoadDefaultCredentials() (*RefreshToken, error) {
	pixieAuthFilePath, err := utils.EnsureDefaultAuthFilePath()
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

	if parsed, _ := jwt.Parse([]byte(token.Token)); parsed != nil {
		userID := srvutils.GetUserID(parsed)
		if userID != "" && !sentSegmentAlias {
			// Associate UserID with AnalyticsID.
			_ = pxanalytics.Client().Enqueue(&analytics.Alias{
				UserId:     pxconfig.Cfg().UniqueClientID,
				PreviousId: userID,
			})
			sentSegmentAlias = true
		}
	}

	// TODO(zasgar): Exchange refresh token for new token type.
	return token, nil
}

// IsAuthenticated returns whether the user is currently authenticated. This includes whether they have
// existing credentials and whether those are actually valid.
func IsAuthenticated(cloudAddr string) bool {
	creds := MustLoadDefaultCredentials()
	client := http.Client{}
	req, err := http.NewRequest("GET", fmt.Sprintf("https://%s/api/authorized", cloudAddr), nil)
	if err != nil {
		return false
	}

	req.Header.Add("Authorization", fmt.Sprintf("Bearer %s", creds.Token))

	resp, err := client.Do(req)
	if err != nil {
		return false
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return false
	}

	return string(body) == "OK"
}

// MustLoadDefaultCredentials loads the default credentials for the user.
// An error will print to console and call os.Exit.
func MustLoadDefaultCredentials() *RefreshToken {
	token, err := LoadDefaultCredentials()

	if err != nil && os.IsNotExist(err) {
		utils.Error("You must be logged in to perform this operation. Please run `px auth login`.")
	} else if err != nil {
		utils.Errorf("Failed to get auth credentials: %s", err.Error())
	}

	if err != nil {
		os.Exit(1)
	}

	return token
}

// CtxWithCreds returns a context with default credentials for the user.
// Since this uses MustLoadDefaultCredentials, a lack of credentials will
// cause an os.Exit
func CtxWithCreds(ctx context.Context) context.Context {
	creds := MustLoadDefaultCredentials()
	ctxWithCreds := metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", creds.Token))
	return ctxWithCreds
}

// PixieCloudLogin performs login on the pixie cloud.
type PixieCloudLogin struct {
	ManualMode bool
	CloudAddr  string
	// OrgID: Selection is only valid for "pixie.support", will be removed when RBAC is supported.
	OrgID string
	// UseAPIKey, if true then prompt user for API key to use for login.
	UseAPIKey bool
	// APIKey to use if specified. Otherwise, prompt for the key if UseAPIKey is true.
	APIKey string
}

// Run either launches the browser or prints out the URL for auth.
func (p *PixieCloudLogin) Run() (*RefreshToken, error) {
	// Preferentially use API key for auth.
	if p.UseAPIKey && len(p.APIKey) == 0 {
		return p.doAPIKeyAuth()
	}
	if len(p.APIKey) > 0 {
		return p.getRefreshToken("", p.APIKey)
	}
	// There are two ways to do the auth. The first one is where we automatically open up the browser
	// and wait for the challenge to complete and call a HTTP server that we started.
	// The second one is to perform a manual auth.
	// Unless manual mode is specified we will try perform the browser based auth and fallback to manual auth.
	if !p.ManualMode {
		refreshToken, err := p.tryBrowserAuth()
		// Handle errors.
		switch err {
		case nil:
			return refreshToken, nil
		case errUserNotRegistered:
			utils.Error("Failed to authenticate. Please refer to UI for further instructions.")
			os.Exit(1)
		case errUserChallengeTimeout:
			utils.Error("Timeout waiting for response from browser. Perhaps try --manual mode.")
			os.Exit(1)
		case errBrowserFailed:
			fallthrough
		default:
			utils.WithError(err).Info("Failed to perform browser based auth. Will try manual auth")
		}
	}
	_ = pxanalytics.Client().Enqueue(&analytics.Track{
		UserId: pxconfig.Cfg().UniqueClientID,
		Event:  "Manual Auth",
	})
	// Try to request using manual mode
	accessToken, err := p.getAuthStringManually()
	if err != nil {
		return nil, err
	}
	utils.Info("Fetching refresh token")

	return p.getRefreshToken(accessToken, "")
}

func addCORSHeaders(res http.ResponseWriter) {
	headers := res.Header()
	headers.Add("Access-Control-Allow-Origin", "*")
	headers.Add("Vary", "Origin")
	headers.Add("Vary", "Access-Control-Request-Method")
	headers.Add("Vary", "Access-Control-Request-Headers")
	headers.Add("Access-Control-Allow-Headers", "Content-Type, Origin, Accept, token")
	headers.Add("Access-Control-Allow-Methods", "GET, POST,OPTIONS")
}

func sendError(w http.ResponseWriter, err error) {
	if errors.Is(err, errUserNotRegistered) {
		w.WriteHeader(http.StatusNotFound)
	} else {
		w.WriteHeader(http.StatusInternalServerError)
	}

	fmt.Fprint(w, err.Error())
}

func (p *PixieCloudLogin) doAPIKeyAuth() (*RefreshToken, error) {
	fmt.Print("\nEnter API Key (won't echo): ")
	apiKey, err := term.ReadPassword(syscall.Stdin)
	fmt.Print("\n")
	if err != nil {
		return nil, err
	}

	return p.getRefreshToken("", string(apiKey))
}

func (p *PixieCloudLogin) tryBrowserAuth() (*RefreshToken, error) {
	// Browser auth starts up a server on localhost to do the user challenge
	// and get the authentication token.
	_ = pxanalytics.Client().Enqueue(&analytics.Track{
		UserId: pxconfig.Cfg().UniqueClientID,
		Event:  "Browser Auth",
	})
	authURL := p.getAuthURL()
	q := authURL.Query()
	q.Set("redirect_uri", localServerRedirectURL)
	authURL.RawQuery = q.Encode()

	type result struct {
		Token *RefreshToken
		err   error
	}

	// The token/error is returned on this channel.
	results := make(chan result, 1)

	mux := http.DefaultServeMux
	// Start up HTTP server to intercept the browser data.
	mux.HandleFunc("/auth_complete", func(w http.ResponseWriter, r *http.Request) {
		addCORSHeaders(w)
		if r.Method == http.MethodOptions {
			w.WriteHeader(http.StatusOK)
			return
		} else if r.Method != http.MethodGet {
			err := errors.New("wrong method on HTTP request, assuming auth failed")
			sendError(w, err)
			results <- result{nil, err}
			return
		}
		accessToken := r.Header.Get("token")
		if accessToken == "" {
			err := errors.New("missing code, assuming auth failed")
			sendError(w, err)
			results <- result{nil, err}
			return
		}

		refreshToken, err := p.getRefreshToken(accessToken, "")

		if err != nil {
			sendError(w, err)
			results <- result{nil, err}
			return
		}

		fmt.Fprintf(w, "OK")

		// Successful auth.
		results <- result{refreshToken, nil}
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
			results <- result{nil, errServerListenerFailed}
		}
	}()

	go func() {
		utils.Info("Starting browser... (if browser-based login fails, try running `px auth login --manual` for headless login)")
		err := open.Run(authURL.String())
		if err != nil {
			_ = pxanalytics.Client().Enqueue(&analytics.Track{
				UserId: pxconfig.Cfg().UniqueClientID,
				Event:  "Browser Open Failed",
			})
			results <- result{nil, errBrowserFailed}
		}
	}()

	ctx, cancel := context.WithTimeout(context.Background(), time.Minute*2)
	defer cancel()
	defer func() {
		_ = h.Shutdown(ctx)
	}()

	for {
		select {
		case <-ctx.Done():
			return nil, errUserChallengeTimeout
		case res, ok := <-results:
			if !ok {
				_ = pxanalytics.Client().Enqueue(&analytics.Track{
					UserId: pxconfig.Cfg().UniqueClientID,
					Event:  "Auth Failure",
				})
				return nil, errUserChallengeTimeout
			}
			_ = pxanalytics.Client().Enqueue(&analytics.Track{
				UserId: pxconfig.Cfg().UniqueClientID,
				Event:  "Auth Success",
			})
			// TODO(zasgar): This is a hack, figure out why this function takes so long to exit.
			utils.Info("Fetching refresh token ...")
			return res.Token, res.err
		}
	}
}

func (p *PixieCloudLogin) getAuthStringManually() (string, error) {
	authURL := p.getAuthURL()
	// fmt.Printf appears to escape % (as desired) so we use it here instead of the cli logger.
	fmt.Printf("\nPlease Visit: \n \t %s\n\n", authURL.String())
	f := bufio.NewWriter(os.Stdout)
	_, err := f.WriteString("Copy and paste token here: ")
	if err != nil {
		return "", err
	}
	f.Flush()

	r := bufio.NewReader(os.Stdin)
	str, err := r.ReadString('\n')
	if err != nil {
		return "", err
	}

	return strings.TrimSpace(str), nil
}

func (p *PixieCloudLogin) getRefreshToken(accessToken string, apiKey string) (*RefreshToken, error) {
	conn, err := utils.GetCloudClientConnection(p.CloudAddr)
	if err != nil {
		return nil, err
	}
	authClient := cloudpb.NewAuthServiceClient(conn)
	authRequest := &cloudpb.LoginRequest{
		AccessToken: accessToken,
	}
	ctx := context.Background()
	if len(apiKey) > 0 {
		ctx = metadata.AppendToOutgoingContext(ctx, "pixie-api-key", apiKey)
	}
	resp, err := authClient.Login(ctx, authRequest)
	if err != nil {
		return nil, err
	}

	// Get the org name from the cloud.
	var orgID string
	if token, _ := jwt.Parse([]byte(resp.Token)); token != nil {
		orgID = srvutils.GetOrgID(token)
	}

	orgClient := cloudpb.NewOrganizationServiceClient(conn)
	ctx = metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", resp.Token))
	ctx, cancel := context.WithTimeout(ctx, 2*time.Second)
	defer cancel()
	orgResp, err := orgClient.GetOrg(ctx, apiutils.ProtoFromUUIDStrOrNil(orgID))
	if err != nil {
		return nil, err
	}

	return &RefreshToken{
		Token:     resp.Token,
		ExpiresAt: resp.ExpiresAt,
		OrgID:     orgID,
		OrgName:   orgResp.OrgName,
	}, nil
}

// RefreshToken is the format for the refresh token.
type RefreshToken struct {
	Token     string `json:"token"`
	ExpiresAt int64  `json:"expiresAt"`
	OrgName   string `json:"orgName,omitempty"`
	OrgID     string `json:"orgID,omitempty"`
}

func (p *PixieCloudLogin) getAuthURL() *url.URL {
	authURL, err := url.Parse(fmt.Sprintf("https://work.%s", p.CloudAddr))
	if err != nil {
		// This is intentionally a log.Fatal, which will trigger a Sentry error.
		// In most other cases, we want to use the cli logger, which will not trigger
		// Sentry errors on user errors.
		log.WithError(err).Fatal("Failed to parse cloud addr.")
	}
	authURL.Path = "/login"
	params := url.Values{}
	params.Add("local_mode", "true")
	authURL.RawQuery = params.Encode()
	return authURL
}
