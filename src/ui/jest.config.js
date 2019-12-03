module.exports = {
  globals: {
    window: true,
  },
  setupFiles: [
    '<rootDir>/enzyme-setup.js',
  ],
  setupFilesAfterEnv: [
    '<rootDir>/src/jest-test-setup.js',
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
    'node_modules',
    '<rootDir>/src',
  ],
  moduleNameMapper: {
    '^.+\.(jpg|jpeg|png|gif|svg)$': '<rootDir>/src/file-mock.js',
    '(\\.css|\\.scss$)|(normalize.css/normalize)|(^exports-loader)': 'identity-obj-proxy',
  },
  resolver: null,
  transform: {
    '^.+\\.jsx?$': 'babel-jest',
    '^.+\\.tsx?$': 'ts-jest',
    '^.+\\.toml?$': 'jest-raw-loader',
  },
  testRegex: '.*-test\\.(ts|tsx|js|jsx)$',
  reporters: [
    'default',
    'jest-junit',
  ],
};
