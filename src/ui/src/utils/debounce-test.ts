import { debounce } from './debounce';

describe('debounce test', () => {
  beforeEach(() => {
    jest.useFakeTimers();
  });

  it('doesn\'t call method before debounce time', () => {
    const func = jest.fn();
    const call = debounce(func, 20);
    call();
    jest.advanceTimersByTime(10);
    expect(func.mock.calls).toHaveLength(0);
    jest.clearAllTimers();
  });

  it('calls the method after debounce time', () => {
    const func = jest.fn();
    const call = debounce(func, 20);
    call();
    jest.advanceTimersByTime(20);
    expect(func).toHaveBeenCalled();
  });

  it('debounces the calls', () => {
    const func = jest.fn();
    const call = debounce(func, 20);
    for (let i = 0; i < 10; i++) {
      call();
    }
    jest.advanceTimersByTime(20);
    expect(func).toHaveBeenCalledTimes(1);
  });

  it('resets the timer on later calls', () => {
    const func = jest.fn();
    const call = debounce(func, 20);
    call();
    jest.advanceTimersByTime(10);
    expect(func).not.toHaveBeenCalled();
    call();
    jest.advanceTimersByTime(10);
    expect(func).not.toHaveBeenCalled();

    jest.advanceTimersByTime(10);
    expect(func).toHaveBeenCalledTimes(1);
  });

  it('calls the function with the right arguments', () => {
    const func = jest.fn();
    const call = debounce(func, 20);
    call('abc');
    jest.advanceTimersByTime(10);
    expect(func).not.toHaveBeenCalled();
    call('def');
    jest.advanceTimersByTime(10);
    expect(func).not.toHaveBeenCalled();

    jest.advanceTimersByTime(10);
    expect(func).toHaveBeenCalledTimes(1);
    expect(func).toHaveBeenLastCalledWith('def');
  });
});
