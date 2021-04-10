// TODO(nick): Arcanist's lint config is using the root config instead of the local one, causing this to trip.
//  The .eslintrc.json that goes with this package is properly set up for this file and is the one we should be using.
//  For now, just silence the warning since it's a false positive.
// eslint-disable-next-line import/no-extraneous-dependencies
import { act } from '@testing-library/react';

export const waitNoAct = async (t = 0): Promise<void> => new Promise((resolve) => {
  setTimeout(resolve, t);
});

export const wait = async (t = 0): Promise<undefined> => act(async () => waitNoAct(t));
