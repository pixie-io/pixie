package autocomplete

import (
	"errors"

	"pixielabs.ai/pixielabs/src/cloud/autocomplete/ebnf"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
)

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
	Value       string
	Kind        cloudapipb.AutocompleteEntityKind
	Valid       bool
	Suggestions []*Suggestion
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

			suggestions, exactMatch, err := s.GetSuggestions(input, []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SCRIPT}, []cloudapipb.AutocompleteEntityKind{})
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
				Value: input,
				Kind:  cloudapipb.AEK_SCRIPT,
				Valid: exactMatch,
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
			Value: value,
			Kind:  entityType,
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
		suggestions, exactMatch, err := s.GetSuggestions(a.Value, ak, specifiedEntities)
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
func (c *Command) ToFormatString() (formattedInput string, suggestions []cloudapipb.TabSuggestion) {
	return "", nil
}
