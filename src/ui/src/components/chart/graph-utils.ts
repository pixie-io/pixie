import * as d3 from 'd3';

export function overflows(parent: DOMRect, child: DOMRect): boolean {
  return child.height > parent.height || child.width > parent.width;
}

export function scaleToFit(parent: DOMRect, child: DOMRect): number {
  if (!overflows(parent, child)) {
    // Don't scale the child if it doesn't overflow.
    return 1;
  }
  return Math.min(parent.height / child.height, parent.width / child.width);
}

export function fitDirection(parent: DOMRect, child: DOMRect): 'x' | 'y' {
  return parent.width / child.width > parent.height / child.height ? 'y' : 'x';
}

export function centerFit(parent: DOMRect, child: DOMRect): d3.ZoomTransform {
  if (!overflows(parent, child)) {
    return d3.zoomIdentity.translate(parent.width / 2 - child.width / 2, parent.height / 2 - child.height / 2);
  }

  const scale = scaleToFit(parent, child);
  const direction = fitDirection(parent, child);
  const translate = {
    x: direction === 'x' ? 0 : parent.width / 2 - child.width * scale / 2,
    y: direction === 'y' ? 0 : parent.height / 2 - child.height * scale / 2,
  };
  console.log({ translate, parent, child });
  return d3.zoomIdentity.translate(translate.x, translate.y).scale(scale);
}
