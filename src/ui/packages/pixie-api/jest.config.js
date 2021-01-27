module.exports = {
  globals: {
    window: true,
  },
  moduleFileExtensions: ['ts', 'js'],
  moduleDirectories: ['node_modules', '<rootDir>/src'],
  resolver: null,
  transform: {
    '^.+\\.ts$': 'ts-jest',
  },
  testRegex: '.*\\.test\\.(ts|js)$',
  reporters: ['default', 'jest-junit'],
  collectCoverageFrom: [
    'src/**/*.ts',
    'src/**/*.js',
  ],
};
