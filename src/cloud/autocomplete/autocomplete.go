package autocomplete

import (
	"errors"
	"fmt"
	"strings"

	"pixielabs.ai/pixielabs/src/cloud/autocomplete/ebnf"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
)

// CursorMarker is the string that is used to denote the position of the cursor in the formatted output string.
var CursorMarker = "$0"

// Suggester is responsible for providing suggestions.
type Suggester interface {
	// GetSuggestions does a fuzzy match on the given input.
	// allowedKinds: The entity types we should be matching with.
	// allowedArgs: If matching a script entity, these are the arg types the script should be able to take.
	GetSuggestions(input string, allowedKinds []cloudapipb.AutocompleteEntityKind, allowedArgs []cloudapipb.AutocompleteEntityKind) ([]*Suggestion, bool, error)
}

// Suggestion is a suggestion for a token.
type Suggestion struct {
	Name  string
	Desc  string
	Score float64 // A score representing how closely the suggestion matches the given input.
	Kind  cloudapipb.AutocompleteEntityKind
	Args  []cloudapipb.AutocompleteEntityKind // If the suggestion is a script, the args that the script takes.
}

// TabStop represents a tab stop in a command.
type TabStop struct {
	Value          string
	Kind           cloudapipb.AutocompleteEntityKind
	Valid          bool
	Suggestions    []*Suggestion
	ContainsCursor bool
}

// Command represents an executable command.
type Command struct {
	TabStops   []*TabStop
	Executable bool
}

var kindLabelToProtoMap = map[string]cloudapipb.AutocompleteEntityKind{
	"svc":    cloudapipb.AEK_SVC,
	"pod":    cloudapipb.AEK_POD,
	"script": cloudapipb.AEK_SCRIPT,
	"ns":     cloudapipb.AEK_NAMESPACE,
}

var protoToKindLabelMap = map[cloudapipb.AutocompleteEntityKind]string{
	cloudapipb.AEK_SVC:       "svc",
	cloudapipb.AEK_POD:       "pod",
	cloudapipb.AEK_SCRIPT:    "script",
	cloudapipb.AEK_NAMESPACE: "ns",
}

// Autocomplete returns a formatted string and suggestions for the given input.
func Autocomplete(input string, cursorPos int, action cloudapipb.AutocompleteActionType, s Suggester) (string, bool, []*cloudapipb.TabSuggestion, error) {
	inputWithCursor := input[:cursorPos] + "$0" + input[cursorPos:]
	cmd, err := ParseIntoCommand(inputWithCursor, s)
	if err != nil {
		return "", false, nil, err
	}

	fmtOutput, suggestions := cmd.ToFormatString(action)

	return fmtOutput, cmd.Executable, suggestions, nil
}

// ParseIntoCommand takes user input and attempts to parse it into a valid command with suggestions.
func ParseIntoCommand(input string, s Suggester) (*Command, error) {
	parsedCmd, err := ebnf.ParseInput(input)
	if err != nil {
		return nil, err
	}

	cmd := &Command{}
	cmd.TabStops = make([]*TabStop, 0)

	action := "run"
	if parsedCmd.Action != nil {
		action = *parsedCmd.Action
	}
	// First tabstop should always be action.
	cmd.TabStops = append(cmd.TabStops, &TabStop{
		Value: action,
		Kind:  cloudapipb.AEK_UNKNOWN,
		Valid: true,
	})

	if action == "go" {
		err = parseGoCommand(parsedCmd, cmd, s)
	} else {
		err = parseRunCommand(parsedCmd, cmd, s)
	}

	if err != nil {
		return nil, err
	}

	return cmd, nil
}

func parseGoCommand(parsedCmd *ebnf.ParsedCmd, cmd *Command, s Suggester) error {
	return errors.New("Not yet implemented")
}

func parseRunScript(parsedCmd *ebnf.ParsedCmd, cmd *Command, s Suggester) (int, map[cloudapipb.AutocompleteEntityKind]int, error) {
	// The TabStop after the action should be the script. Check if there are any scripts defined.
	scriptArgs := make(map[cloudapipb.AutocompleteEntityKind]int)
	scriptTabIndex := -1
	for i, a := range parsedCmd.Args {
		if a.Type != nil && *a.Type == "script" {
			// Determine if this is a valid script, and if so, what arguments it takes.
			input := ""
			if a.Name != nil {
				input = *a.Name
			}
			containsCursor := strings.Contains(input, CursorMarker)
			searchTerm := input
			if containsCursor {
				searchTerm = strings.Replace(searchTerm, CursorMarker, "", 1)
			}

			suggestions, exactMatch, err := s.GetSuggestions(searchTerm, []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SCRIPT}, []cloudapipb.AutocompleteEntityKind{})
			if err != nil {
				return -1, nil, err
			}

			if exactMatch {
				// Create a mapping from argKind to count.
				for _, scriptArg := range suggestions[0].Args {
					if _, ok := scriptArgs[scriptArg]; ok {
						scriptArgs[scriptArg]++
					} else {
						scriptArgs[scriptArg] = 1
					}
				}
			}

			cmd.TabStops = append(cmd.TabStops, &TabStop{
				Value:          input,
				Kind:           cloudapipb.AEK_SCRIPT,
				Valid:          exactMatch,
				ContainsCursor: containsCursor,
			})

			scriptTabIndex = i
			break
		}
	}

	return scriptTabIndex, scriptArgs, nil
}

func parseRunArgs(parsedCmd *ebnf.ParsedCmd, cmd *Command, s Suggester, scriptArgs map[cloudapipb.AutocompleteEntityKind]int, scriptTabIndex int) ([]*TabStop, []cloudapipb.AutocompleteEntityKind) {
	// Look through other entities that are already defined (by svc:,pod:, etc. specifiers) to determine which script
	// args still need to be filled out.
	specifiedEntities := []cloudapipb.AutocompleteEntityKind{}
	args := make([]*TabStop, 0)
	for i, a := range parsedCmd.Args {
		if i == scriptTabIndex {
			continue // Don't re-parse the script field.
		}

		entityType := cloudapipb.AEK_UNKNOWN
		value := ""
		if a.Type != nil {
			if val, ok := kindLabelToProtoMap[*a.Type]; ok {
				entityType = val
			}
			value += *a.Type + ":"
		}

		// If a script was defined, and a type is defined for this arg, we should check that the arg is valid argument for the script.
		if entityType != cloudapipb.AEK_UNKNOWN {
			count, ok := scriptArgs[entityType]
			if (len(scriptArgs) == 0 && scriptTabIndex == -1) || (ok && count > 0) {
				if ok {
					scriptArgs[entityType]-- // Track this argument kind has already been used.
				}
				value = ""
				specifiedEntities = append(specifiedEntities, entityType)
			} else {
				entityType = cloudapipb.AEK_UNKNOWN // The specified type is not valid. This argument should be another type.
			}
		}

		if a.Name != nil {
			value += *a.Name
		}

		args = append(args, &TabStop{
			Value:          value,
			Kind:           entityType,
			ContainsCursor: strings.Contains(value, CursorMarker),
		})
	}
	return args, specifiedEntities
}

func validateCommand(scriptDefined bool, scriptArgs map[cloudapipb.AutocompleteEntityKind]int, cmd *Command) {
	// Determine if the command is executable.
	cmd.Executable = true
	if !scriptDefined {
		cmd.Executable = false
	}
	for _, v := range scriptArgs {
		// All script args should be specified.
		if v != 0 {
			cmd.Executable = false
			break
		}
	}
	for _, a := range cmd.TabStops {
		// All args should be valid.
		if a.Valid != true {
			cmd.Executable = false
			break
		}
	}
}

func parseRunCommand(parsedCmd *ebnf.ParsedCmd, cmd *Command, s Suggester) error {
	if parsedCmd.Args == nil {
		// TODO(michelle): Handle the case where there are no args.
		return nil
	}

	scriptTabIndex, scriptArgs, err := parseRunScript(parsedCmd, cmd, s)
	if err != nil {
		return err
	}

	args, specifiedEntities := parseRunArgs(parsedCmd, cmd, s, scriptArgs, scriptTabIndex)

	// Determine what entity kinds we should search for.
	allowedKinds := make([]cloudapipb.AutocompleteEntityKind, 0)
	if len(scriptArgs) == 0 { // No script was defined, so we can search for all possible types.
		allowedKinds = []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_POD, cloudapipb.AEK_SVC, cloudapipb.AEK_NAMESPACE}
	} else {
		for k, v := range scriptArgs {
			// Get the remaining allowed kinds based on the
			// arguments of the script and what kinds have already been specified.
			if v > 0 {
				allowedKinds = append(allowedKinds, k)
			}
		}
	}
	if scriptTabIndex == -1 { // No script is defined, so we can search for scripts as well.
		allowedKinds = append(allowedKinds, cloudapipb.AEK_SCRIPT)
	}

	// Get suggestions for each argument.
	for i, a := range args { // TODO(michelle): Run this in parallel.
		ak := allowedKinds
		if a.Kind != cloudapipb.AEK_UNKNOWN { // The kind is already specified in the input string.
			ak = []cloudapipb.AutocompleteEntityKind{a.Kind}
		}
		searchTerm := a.Value
		if a.ContainsCursor {
			searchTerm = strings.Replace(searchTerm, CursorMarker, "", 1)
		}
		suggestions, exactMatch, err := s.GetSuggestions(searchTerm, ak, specifiedEntities)
		if err != nil {
			return err
		}
		args[i].Suggestions = suggestions
		args[i].Valid = exactMatch && a.Kind != cloudapipb.AEK_UNKNOWN
	}

	cmd.TabStops = append(cmd.TabStops, args...)

	validateCommand(scriptTabIndex != -1, scriptArgs, cmd)

	return nil
}

// ToFormatString converts a command to a formatted string with tab indexes, such as: ${1:run} ${2: px/svc_info}
func (cmd *Command) ToFormatString(action cloudapipb.AutocompleteActionType) (formattedInput string, suggestions []*cloudapipb.TabSuggestion) {
	curTabStop, nextInvalidTabStop, invalidTabs := cmd.processTabStops()

	// Move the cursor according to the action that was taken.
	switch action {
	case cloudapipb.AAT_EDIT:
		// If the action was an edit, the cursor should stay in the same position.
		break
	case cloudapipb.AAT_SELECT:
		// If the action was a select, we should move the cursor to the next invalid tabstop.
		// Remove cursor from current tab stop.
		cmd.TabStops[curTabStop].Value = strings.Replace(cmd.TabStops[curTabStop].Value, CursorMarker, "", 1)
		cmd.TabStops[nextInvalidTabStop].Value = cmd.TabStops[nextInvalidTabStop].Value + CursorMarker
		curTabStop = nextInvalidTabStop
		break
	default:
		break
	}

	// Construct the formatted string and tab suggestions.
	fStr := ""
	suggestions = make([]*cloudapipb.TabSuggestion, len(cmd.TabStops))
	for i, t := range cmd.TabStops {
		// The tab index of this tabstop is ((idx - (curTabStop + 1)) % numTabStops) + 1.
		idx := (((i - (curTabStop + 1)) + len(cmd.TabStops)) % len(cmd.TabStops)) + 1

		invalid, ok := invalidTabs[i]
		executableAfterSelect := ok && invalid && len(invalidTabs) == 1

		// Populate suggestions for the tab index.
		acSugg := make([]*cloudapipb.AutocompleteSuggestion, len(t.Suggestions))
		for j, s := range t.Suggestions {
			acSugg[j] = &cloudapipb.AutocompleteSuggestion{
				Kind:        s.Kind,
				Name:        s.Name,
				Description: s.Desc,
			}
		}

		ts := &cloudapipb.TabSuggestion{
			TabIndex:              int64(idx),
			ExecutableAfterSelect: executableAfterSelect,
			Suggestions:           acSugg,
		}
		suggestions[i] = ts

		// Append to the formatted string.
		if t.Value == "" && t.Kind == cloudapipb.AEK_UNKNOWN {
			fStr += fmt.Sprintf("$%d", idx)
		} else {
			fStr += fmt.Sprintf("${%d:", idx)
			if t.Kind != cloudapipb.AEK_UNKNOWN {
				fStr += protoToKindLabelMap[t.Kind] + ":"
			}
			if t.Value != "" {
				fStr += t.Value
			}
			fStr += "}"
		}

		if i != len(cmd.TabStops)-1 {
			fStr += " "
		}
	}

	return fStr, suggestions
}

// processTabStops iterates through the tabs to determine which is the current tab the cursor is on, which is the next invalid
// tab stop that should be next if a selection is made, and finds all invalid tab stops.
func (cmd *Command) processTabStops() (int, int, map[int]bool) {
	// The tab stop contains the current cursor.
	curTabStop := -1
	// The next invalid tab stop in the command, that should be tabbed to if a selection is made.
	nextInvalidTabStop := -1
	// All tabs that are still invalid.
	invalidTabs := make(map[int]bool)
	for i, t := range cmd.TabStops {
		if !t.Valid {
			invalidTabs[i] = true

			if nextInvalidTabStop == -1 || (curTabStop != -1 && nextInvalidTabStop < curTabStop) {
				// If we haven't found an invalid tabstop, or if there is an invalid tabstop after the current tab stop, that should be the next tabstop.
				nextInvalidTabStop = i
			}
		}

		if t.ContainsCursor {
			curTabStop = i
		}
	}

	if curTabStop == -1 { // For some reason, if there is no cursor, we should put it at the very end.
		curTabStop = len(cmd.TabStops) - 1
		cmd.TabStops[curTabStop].Value = cmd.TabStops[curTabStop].Value + CursorMarker
	}
	if nextInvalidTabStop == -1 { // All tabstops are valid.
		nextInvalidTabStop = len(cmd.TabStops) - 1
	}
	return curTabStop, nextInvalidTabStop, invalidTabs
}
