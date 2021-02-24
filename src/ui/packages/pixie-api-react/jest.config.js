module.exports = {
  rootDir: '.',
  globals: {
    window: true,
  },
  setupFilesAfterEnv: [
    '<rootDir>/src/testing/jest-test-setup.js',
  ],
  moduleFileExtensions: ['ts', 'tsx', 'js'],
  moduleDirectories: ['node_modules', 'src'],
  resolver: null,
  transform: {
    '^.+\\.tsx?$': 'ts-jest',
  },
  testRegex: '.*\\.test\\.(ts|tsx|js)$',
  reporters: ['default', 'jest-junit'],
  collectCoverageFrom: [
    'src/**/*.ts',
    'src/**/*.tsx',
    'src/**/*.js',
  ],
};
