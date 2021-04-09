import { act } from '@testing-library/react';

export const waitNoAct = async (t = 0): Promise<void> => new Promise((resolve) => {
  setTimeout(resolve, t);
});

export const wait = async (t = 0): Promise<undefined> => act(async () => waitNoAct(t));
