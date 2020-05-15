const enzyme = require('enzyme');
const Adapter = require('enzyme-adapter-react-16');
const noop = require('utils/noop');

enzyme.configure({ adapter: new Adapter() });
// Jest uses jsdom, where document.createRange is not specified. This is used
// in some of our external dependencies. Mock this out so tests don't fail.
if (global.document) {
  document.createRange = () => ({
    setStart: noop,
    setEnd: noop,
    commonAncestorContainer: {
      nodeName: 'BODY',
      ownerDocument: document,
    },
  });
}
