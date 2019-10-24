import { pluralize } from './pluralize';

describe('pluralize test', () => {
  it('should return singular if the count is 1', () => {
    expect(pluralize('word', 1)).toEqual('word');
  });

  it('should return plural if the count is 0', () => {
    expect(pluralize('word', 0)).toEqual('words');
  });

  it('should return plural if the count is greater than 1', () => {
    expect(pluralize('word', 10)).toEqual('words');
  });

  it('should return the given plural if the count is greater than 1', () => {
    expect(pluralize('person', 10, 'people')).toEqual('people');
  });
});
