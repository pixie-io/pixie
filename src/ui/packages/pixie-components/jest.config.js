module.exports = {
  globals: {
    window: true,
  },
  setupFiles: ['<rootDir>/src/testing/enzyme-setup.ts', 'jest-canvas-mock'],
  setupFilesAfterEnv: ['<rootDir>/src/testing/jest-test-setup.js'],
  moduleFileExtensions: ['ts', 'tsx', 'js'],
  moduleDirectories: ['node_modules', '<rootDir>/src'],
  resolver: null,
  transform: {
    '^.+\\.tsx?$': 'ts-jest',
  },
  testRegex: '.*\\.test\\.(ts|tsx|js|jsx)$',
  reporters: ['default', 'jest-junit'],
  collectCoverageFrom: [
    'src/**/*.ts',
    'src/**/*.tsx',
    'src/**/*.js',
    'src/**/*.jsx',
    'src/*.ts',
    'src/*.tsx',
    'src/*.js',
    'src/*.jsx',
  ],
};
