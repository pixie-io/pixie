// If the pxl script contains any of the following strings, it contains a mutation.
const pxlMutations = [
  'from pxtrace',
  'import pxtrace',
];

export const ContainsMutation = (pxl): boolean => (
  pxlMutations.some((mutationStr) => {
    const re = new RegExp(`^${mutationStr}`, 'gm');
    return pxl.match(re);
  })
);

export const IsStreaming = (pxl): boolean => (
  pxl.indexOf('.stream()') !== -1
);
