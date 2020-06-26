// Determines if elem is fully visible inside parent. If not, this can be used with
// .scrollIntoView() to make elem visible.
// NOTE: this only checks the vertical position of the elements.
export function isInView(parent: HTMLElement, elem: HTMLElement): boolean {
  const pbbox = parent.getBoundingClientRect();
  const ebbox = elem.getBoundingClientRect();
  return pbbox.top <= ebbox.top
    && pbbox.bottom >= ebbox.bottom;
}
