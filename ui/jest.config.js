module.exports = {
  globals: {
    window: true,
  },
  setupFiles: [
    '<rootDir>/enzyme-setup.js',
  ],
  moduleFileExtensions: [
    'ts',
    'tsx',
    'js',
    'jsx',
  ],
  moduleDirectories: [
    'node_modules',
    '<rootDir>/src',
  ],
  moduleNameMapper: {
    '^.+\\.(jpg|jpeg|png|gif|svg)$': '<rootDir>/config/jest/file-mock.js',
    '(\\.css|scss$)|(normalize.css/normalize)|(^exports-loader)': 'identity-obj-proxy',
  },
  transform: {
    '^.+\\.tsx?$': 'ts-jest',
    '^.+\\.jsx?$': 'babel-jest',
  },
  testRegex: '.*-test\\.(ts|tsx|js|jsx)$',
  reporters: [
    'default',
    'jest-junit',
  ],
};
