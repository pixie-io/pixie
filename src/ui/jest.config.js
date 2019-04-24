module.exports = {
  globals: {
    window: true,
  },
  setupFiles: [
    '<rootDir>/enzyme-setup.js',
  ],
  moduleFileExtensions: [
    'js',
    'json',
    'jsx',
    'mjs',
    'ts',
    'tsx',
  ],
  moduleDirectories: [
    '<rootDir>/node_modules',
    '<rootDir>/src',
  ],
  moduleNameMapper: {
    '^.+\.(jpg|jpeg|png|gif|svg)$': '<rootDir>/src/file-mock.js',
    '(\\.css|scss$)|(normalize.css/normalize)|(^exports-loader)': 'identity-obj-proxy',
  },
  resolver: null,
  transform: {
    '^.+\\.jsx?$': 'babel-jest',
    '^.+\\.tsx?$': 'ts-jest',
  },
  testRegex: '.*-test\\.(ts|tsx|js|jsx)$',
  reporters: [
    'default',
    'jest-junit',
  ],
};
