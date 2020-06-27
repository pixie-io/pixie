import * as AutocompleteParser from './autocomplete-parser';

describe('AutocompleteParser test', () => {
  it('should return correct tabstops for basic command', () => {
    // eslint-disable-next-line no-template-curly-in-string
    const ts = AutocompleteParser.ParseFormatStringToTabStops('${1:run} ${2:svc_name:$0pl/test} ${3}');
    expect(ts).toEqual([
      { Index: 1, Value: 'run', CursorPosition: -1 },
      {
        Index: 2, Label: 'svc_name', Value: 'pl/test', CursorPosition: 0,
      },
      { Index: 3, CursorPosition: -1 },
    ]);
  });

  it('should return correct tabstops with empty label', () => {
    // eslint-disable-next-line no-template-curly-in-string
    const ts = AutocompleteParser.ParseFormatStringToTabStops('${1:run$0} ${2:svc_name:} ${3}');
    expect(ts).toEqual([
      { Index: 1, Value: 'run', CursorPosition: 3 },
      { Index: 2, Label: 'svc_name', CursorPosition: -1 },
      { Index: 3, CursorPosition: -1 },
    ]);
  });
});
