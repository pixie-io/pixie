// If the pxl script contains any of the following strings, it contains a mutation.
const pxlMutations = [
  'from pxtrace',
  'import pxtrace',
  'import pxconfig',
];

export const containsMutation = (pxl: string): boolean => (
  pxlMutations.some((mutationStr) => {
    const re = new RegExp(`^${mutationStr}`, 'gm');
    return pxl.match(re);
  })
);

export const isStreaming = (pxl: string): boolean => (
  pxl.indexOf('.stream()') !== -1
);
