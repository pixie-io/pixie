import { addDecorator } from '@storybook/react';
import { themes } from '@storybook/theming';
import withTheme from './mui-theme-decorator';

// Provide a material-ui theme.
addDecorator(withTheme);

export const parameters = {
  actions: { argTypesRegex: '^on[A-Z].*' },
  docs: {
    theme: themes.dark,
  },
};
