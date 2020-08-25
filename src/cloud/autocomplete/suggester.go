package autocomplete

import (
	"context"
	"encoding/json"
	"fmt"
	"strings"

	"github.com/olivere/elastic/v7"
	"github.com/sahilm/fuzzy"
	uuid "github.com/satori/go.uuid"
	"github.com/spf13/viper"
	"google.golang.org/grpc/metadata"

	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/cloud/indexer/md"
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	"pixielabs.ai/pixielabs/src/shared/services/utils"
	pbutils "pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/script"
)

// ElasticSuggester provides suggestions based on the given index in Elastic.
type ElasticSuggester struct {
	client          *elastic.Client
	mdIndexName     string
	scriptIndexName string
	// This is temporary, and will be removed once we start indexing scripts.
	br         *script.BundleManager
	orgMapping map[uuid.UUID]string
}

var protoToElasticLabelMap = map[cloudapipb.AutocompleteEntityKind]string{
	cloudapipb.AEK_SVC:       "service",
	cloudapipb.AEK_POD:       "pod",
	cloudapipb.AEK_SCRIPT:    "script",
	cloudapipb.AEK_NAMESPACE: "script",
}

var elasticLabelToProtoMap = map[string]cloudapipb.AutocompleteEntityKind{
	"service":   cloudapipb.AEK_SVC,
	"pod":       cloudapipb.AEK_POD,
	"script":    cloudapipb.AEK_SCRIPT,
	"namespace": cloudapipb.AEK_NAMESPACE,
}

func getServiceCredentials(signingKey string) (string, error) {
	claims := utils.GenerateJWTForService("API Service")
	return utils.SignJWTClaims(claims, signingKey)
}

// NewElasticSuggester creates a suggester based on an elastic index.
func NewElasticSuggester(client *elastic.Client, mdIndex string, scriptIndex string, br *script.BundleManager, pc profilepb.ProfileServiceClient) (*ElasticSuggester, error) {
	orgMapping := make(map[uuid.UUID]string)
	if br != nil {
		// Generate mapping from orgID -> orgName. This is temporary until we have a script indexer.
		serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
		if err != nil {
			return nil, err
		}
		ctx := metadata.AppendToOutgoingContext(context.Background(), "authorization",
			fmt.Sprintf("bearer %s", serviceAuthToken))
		resp, err := pc.GetOrgs(ctx, &profilepb.GetOrgsRequest{})
		if err != nil {
			return nil, err
		}

		for _, org := range resp.Orgs {
			orgMapping[pbutils.UUIDFromProtoOrNil(org.ID)] = org.OrgName
		}
	}

	return &ElasticSuggester{client, mdIndex, scriptIndex, br, orgMapping}, nil
}

// SuggestionRequest is a request for autocomplete suggestions.
type SuggestionRequest struct {
	OrgID        uuid.UUID
	ClusterUID   string
	Input        string
	AllowedKinds []cloudapipb.AutocompleteEntityKind
	AllowedArgs  []cloudapipb.AutocompleteEntityKind
}

// SuggestionResult contains results for an autocomplete request.
type SuggestionResult struct {
	Suggestions []*Suggestion
	ExactMatch  bool
}

func parseHighlightIndexes(highlightStr string, offset int) []int64 {
	inHighlight := false
	j := int64(offset)
	k := 0
	matchedIndexes := make([]int64, 0)
	for k < len(highlightStr) {
		if string(highlightStr[k]) == "<" {
			if inHighlight { // Ending highlight.
				inHighlight = false
				k += len("</em>")
			} else { // Starting highlight section.
				inHighlight = true
				k += len("<em>")
			}
			continue
		}
		if inHighlight {
			matchedIndexes = append(matchedIndexes, j)
		}
		k++
		j++
	}
	return matchedIndexes
}

// GetSuggestions get suggestions for the given input using Elastic.
func (e *ElasticSuggester) GetSuggestions(reqs []*SuggestionRequest) ([]*SuggestionResult, error) {
	resps := make([]*SuggestionResult, len(reqs))

	if len(reqs) == 0 {
		return resps, nil
	}

	ms := e.client.MultiSearch()

	highlight := elastic.NewHighlight()
	highlight = highlight.Fields(elastic.NewHighlighterField("*"))

	for _, r := range reqs {
		ms.Add(elastic.NewSearchRequest().
			Highlight(highlight).
			Query(e.getQueryForRequest(r.OrgID, r.ClusterUID, r.Input, r.AllowedKinds, r.AllowedArgs)).
			Size(5).FetchSourceIncludeExclude([]string{"kind", "name", "ns"}, []string{}))
	}

	resp, err := ms.Do(context.Background())

	if err != nil {
		return nil, err
	}

	// Parse scripts to prepare for matching. This is temporary until we have script indexing.
	scripts := []string{}
	scriptArgMap := make(map[string][]cloudapipb.AutocompleteEntityKind)
	scriptArgNames := make(map[string][]string)
	if e.br != nil {
		for _, s := range e.br.GetScripts() {
			scripts = append(scripts, s.ScriptName)
			scriptArgMap[s.ScriptName] = make([]cloudapipb.AutocompleteEntityKind, 0)
			for _, a := range s.ComputedArgs() {
				aKind := cloudapipb.AEK_UNKNOWN
				// The args aren't typed yet, so we assume the type from the name.
				if strings.Index(a.Name, "pod") != -1 {
					aKind = cloudapipb.AEK_POD
				}
				if strings.Index(a.Name, "svc") != -1 || strings.Index(a.Name, "service") != -1 {
					aKind = cloudapipb.AEK_SVC
				}

				if aKind != cloudapipb.AEK_UNKNOWN {
					scriptArgMap[s.ScriptName] = append(scriptArgMap[s.ScriptName], aKind)
					scriptArgNames[s.ScriptName] = append(scriptArgNames[s.ScriptName], a.Name)
				}
			}
		}
	}

	for i, r := range resp.Responses {
		// This is temporary until we index scripts in Elastic.
		scriptResults := make([]*Suggestion, 0)
		if e.br != nil {
			for _, t := range reqs[i].AllowedKinds {
				if t == cloudapipb.AEK_SCRIPT { // Script is an allowed type for this tabstop, so we should find matching scripts.
					matches := fuzzy.Find(reqs[i].Input, scripts)

					if reqs[i].Input == "" { // The input is empty, so none of the scripts will match using the fuzzy search.
						matches = make([]fuzzy.Match, len(scripts))
						for i, s := range scripts {
							matches[i] = fuzzy.Match{
								Str:            s,
								MatchedIndexes: make([]int, 0),
							}
						}
					}
					for _, m := range matches {
						script := e.br.MustGetScript(m.Str)
						scriptArgs := scriptArgMap[m.Str]
						scriptNames := scriptArgNames[m.Str]
						valid := true
						if script.OrgName != "" && e.orgMapping[reqs[i].OrgID] != script.OrgName {
							valid = false
						}

						for _, r := range reqs[i].AllowedArgs { // Check that the script takes the allowed args.
							found := false
							for _, arg := range scriptArgs {
								if arg == r {
									found = true
									break
								}
							}
							if !found {
								valid = false
								break
							}
						}
						if valid {
							matchedIdxs := make([]int64, len(m.MatchedIndexes))
							for i, matched := range m.MatchedIndexes {
								matchedIdxs[i] = int64(matched)
							}
							scriptResults = append(scriptResults, &Suggestion{
								Name:           m.Str,
								Kind:           cloudapipb.AEK_SCRIPT,
								Desc:           script.LongDoc,
								ArgNames:       scriptNames,
								ArgKinds:       scriptArgs,
								MatchedIndexes: matchedIdxs,
							})
						}
					}
					break
				}
			}
		}
		exactMatch := len(scriptResults) > 0 && scriptResults[0].Name == reqs[i].Input

		// Convert elastic entity into a suggestion object.
		results := make([]*Suggestion, 0)
		for _, h := range r.Hits.Hits {
			res := &md.EsMDEntity{}
			err = json.Unmarshal(h.Source, res)
			if err != nil {
				return nil, err
			}

			matchedIndexes := make([]int64, 0)
			// Parse highlight string into indexes.
			if len(h.Highlight["ns"]) > 0 {
				matchedIndexes = append(matchedIndexes, parseHighlightIndexes(h.Highlight["ns"][0], 0)...)
			}
			if len(h.Highlight["name"]) > 0 {
				matchedIndexes = append(matchedIndexes, parseHighlightIndexes(h.Highlight["name"][0], len(res.NS)+1)...)
			}
			results = append(results, &Suggestion{
				Name:           res.NS + "/" + res.Name,
				Score:          float64(*h.Score),
				Kind:           elasticLabelToProtoMap[res.Kind],
				MatchedIndexes: matchedIndexes,
			})
		}

		exactMatch = exactMatch || len(results) > 0 && results[0].Name == reqs[i].Input

		results = append(scriptResults, results...)

		resps[i] = &SuggestionResult{
			Suggestions: results,
			ExactMatch:  exactMatch,
		}
	}
	return resps, nil
}

func (e *ElasticSuggester) getQueryForRequest(orgID uuid.UUID, clusterUID string, input string, allowedKinds []cloudapipb.AutocompleteEntityKind, allowedArgs []cloudapipb.AutocompleteEntityKind) *elastic.BoolQuery {
	q := elastic.NewBoolQuery()

	q.Should(e.getMDEntityQuery(orgID, clusterUID, input, allowedKinds))

	// TODO(michelle): Add script query here once that is ready: q.Should(e.getScriptQuery(orgID, input, allowedArgs))
	return q
}

func (e *ElasticSuggester) getMDEntityQuery(orgID uuid.UUID, clusterUID string, input string, allowedKinds []cloudapipb.AutocompleteEntityKind) *elastic.BoolQuery {
	entityQuery := elastic.NewBoolQuery()
	entityQuery.Must(elastic.NewTermQuery("_index", e.mdIndexName))

	// Search by name + namespace.
	splitInput := strings.Split(input, "/") // If contains "/", then everything preceding "/" is a namespace.
	name := input
	if len(splitInput) > 1 {
		entityQuery.Must(elastic.NewMatchQuery("ns", splitInput[0]))
		name = splitInput[1]

		if name != "" {
			entityQuery.Must(elastic.NewMatchQuery("name", name))
		}
	} else if name != "" {
		nsOrNameQuery := elastic.NewMultiMatchQuery(name, "name", "ns")
		entityQuery.Must(nsOrNameQuery)
	}

	// Only search for entities in org.
	entityQuery.Must(elastic.NewTermQuery("orgID", orgID.String()))

	if clusterUID != "" {
		entityQuery.Must(elastic.NewTermQuery("clusterUID", clusterUID))
	}

	// Only search for allowed kinds.
	kindsQuery := elastic.NewBoolQuery()
	for _, k := range allowedKinds {
		kindsQuery.Should(elastic.NewTermQuery("kind", protoToElasticLabelMap[k]))
	}
	entityQuery.Must(kindsQuery)

	return entityQuery
}

func (e *ElasticSuggester) getScriptQuery(orgID uuid.UUID, clusterUID string, input string, allowedArgs []cloudapipb.AutocompleteEntityKind) *elastic.BoolQuery {
	// TODO(michelle): Handle scripts once we get a better idea of what the index looks like.
	scriptQuery := elastic.NewBoolQuery()
	scriptQuery.Must(elastic.NewTermQuery("_index", "scripts"))

	return scriptQuery
}
