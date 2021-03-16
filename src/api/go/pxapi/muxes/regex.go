package muxes

import (
	"context"
	"regexp"

	"pixielabs.ai/pixielabs/src/api/go/pxapi"
	"pixielabs.ai/pixielabs/src/api/go/pxapi/types"
)

// TableRecordHandlerFunc is called by the mux whenever a new table is streamed.
type TableRecordHandlerFunc func(metadata types.TableMetadata) (pxapi.TableRecordHandler, error)

type patternHandler struct {
	re          *regexp.Regexp
	handlerFunc TableRecordHandlerFunc
}

// RegexTableMux provides a regex based router for table based on table names.
type RegexTableMux struct {
	patterns []*patternHandler
}

// NewRegexTableMux creates a new default RegexTableMux.
func NewRegexTableMux() *RegexTableMux {
	return &RegexTableMux{}
}

// RegisterHandlerForPattern registers a handler function that is called based on the passed in regex pattern. The patterns are handled in the order specified.
func (r *RegexTableMux) RegisterHandlerForPattern(pattern string, handlerFunc TableRecordHandlerFunc) error {
	re, err := regexp.Compile(pattern)
	if err != nil {
		return err
	}
	r.patterns = append(r.patterns, &patternHandler{
		re:          re,
		handlerFunc: handlerFunc,
	})
	return nil
}

// AcceptTable implements the Muxer interface and is called when a new table is available.
func (r *RegexTableMux) AcceptTable(ctx context.Context, metadata types.TableMetadata) (pxapi.TableRecordHandler, error) {
	for _, pattern := range r.patterns {
		if pattern.re.Match([]byte(metadata.Name)) {
			return pattern.handlerFunc(metadata)
		}
	}
	return nil, nil
}
