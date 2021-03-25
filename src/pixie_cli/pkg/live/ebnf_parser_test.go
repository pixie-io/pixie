package live_test

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/live"
)

type tabStop struct {
	Index    int
	Label    string
	Value    string
	HasLabel bool
}

func TestParseInput(t *testing.T) {
	tests := []struct {
		name             string
		input            string
		expectedTabStops []*tabStop
	}{
		{
			name:  "basic",
			input: "${1:run} ${2:svc_name:$0pl/test} ${3}",
			expectedTabStops: []*tabStop{
				{
					Index:    1,
					Label:    "run",
					Value:    "",
					HasLabel: false,
				},
				{
					Index:    2,
					Label:    "svc_name",
					Value:    "$0pl/test",
					HasLabel: true,
				},
				{
					Index:    3,
					Label:    "",
					Value:    "",
					HasLabel: false,
				},
			},
		},
		{
			name:  "empty label",
			input: "${1:run} ${2:svc_name:} ${3}",
			expectedTabStops: []*tabStop{
				{
					Index:    1,
					Label:    "run",
					Value:    "",
					HasLabel: false,
				},
				{
					Index:    2,
					Label:    "svc_name",
					Value:    "",
					HasLabel: true,
				},
				{
					Index:    3,
					Label:    "",
					Value:    "",
					HasLabel: false,
				},
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			cmd, err := live.ParseInput(test.input)
			require.NoError(t, err)
			assert.Equal(t, len(test.expectedTabStops), len(cmd.TabStops))

			for i, ts := range cmd.TabStops {
				assert.Equal(t, test.expectedTabStops[i].Index, *ts.Index)
				if test.expectedTabStops[i].Label == "" {
					assert.Nil(t, ts.Label)
				} else {
					assert.Equal(t, test.expectedTabStops[i].Label, *ts.Label)
				}
				if test.expectedTabStops[i].Value == "" {
					assert.Nil(t, ts.Value)
				} else {
					assert.Equal(t, test.expectedTabStops[i].Value, *ts.Value)
				}
				assert.Equal(t, test.expectedTabStops[i].HasLabel, ts.HasLabel)
			}
		})
	}
}
