module.exports = {
  globals: {
    window: true,
  },
  moduleFileExtensions: ['ts', 'js'],
  moduleDirectories: ['node_modules', '<rootDir>/src'],
  resolver: null,
  transform: {
    '^.+\\.jsx?$': 'babel-jest',
    '^.+\\.tsx?$': 'ts-jest',
  },
  testRegex: '.*\\.test\\.(ts|js)$',
  reporters: ['default', 'jest-junit'],
  collectCoverageFrom: [
    'src/**/*.ts',
    'src/**/*.js',
  ],
};
