import { act } from '@testing-library/react';

export const waitNoAct = async (t = 0) => new Promise((resolve) => {
  setTimeout(resolve, t);
});

export const wait = async (t = 0) => act(async () => waitNoAct(t));
