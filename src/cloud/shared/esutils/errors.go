package esutils

import (
	"regexp"

	"github.com/olivere/elastic/v7"
)

var mergeFailureReg *regexp.Regexp
var settingsNonDynReg *regexp.Regexp

func init() {
	mergeFailureReg = regexp.MustCompile(
		`mapper \[(.*)\] of different type, current_type \[(.*)\], merged_type \[(.*)\]`)
	settingsNonDynReg = regexp.MustCompile(
		`Can't update non dynamic settings \[\[(.*)\]\] for open indices \[\[(.*)\]\]`)
}

type elasticErrorWrapper struct {
	e *elastic.Error
}

func mustParseError(err error) *elasticErrorWrapper {
	e := err.(*elastic.Error)
	return &elasticErrorWrapper{e}
}

func (e *elasticErrorWrapper) isMappingMergeFailure() bool {
	details := e.e.Details
	if details.Type != "illegal_argument_exception" {
		return false
	}
	return mergeFailureReg.Match([]byte(details.Reason))
}

func (e *elasticErrorWrapper) isSettingsNonDynamicUpdateFailure() bool {
	details := e.e.Details
	if details.Type != "illegal_argument_exception" {
		return false
	}
	return settingsNonDynReg.Match([]byte(details.Reason))
}
