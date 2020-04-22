// NOTE(james): this file is a modified version of https://github.com/vega/vega-tooltip

import './tooltip.scss';

import * as _ from 'lodash';
import * as numeral from 'numeral';
import {TooltipHandler, View} from 'vega-typings';
import {isArray, isNumber, isObject, isString} from 'vega-util';

const EL_ID = 'vg-tooltip-element';
const DEFAULT_OPTIONS = {
  /**
   * X offset.
   */
  offsetX: 10,

  /**
   * Y offset.
   */
  offsetY: 10,

  /**
   * ID of the tooltip element.
   */
  id: EL_ID,

  /**
   * The name of the theme. You can use the CSS class called [THEME]-theme to style the tooltips.
   *
   * There are two predefined themes: "light" (default) and "dark".
   */
  theme: 'dark',

  /**
   * HTML sanitizer function that removes dangerous HTML to prevent XSS.
   *
   * This should be a function from string to string. You may replace it with a formatter such as a markdown formatter.
   */
  sanitize: escapeHTML,

  /**
   * numeral.js format string to use for numeric vals.
   */
  numeralFmtString: '0[.]00',
};
type Options = Partial<typeof DEFAULT_OPTIONS>;

/**
 * Position the tooltip
 *
 * @param event The mouse event.
 * @param tooltipBox
 * @param offsetX Horizontal offset.
 * @param offsetY Vertical offset.
 */
function calculatePosition(
  event: MouseEvent,
  tooltipBox: {width: number; height: number},
  offsetX: number,
  offsetY: number,
) {
  let x = event.clientX + offsetX;
  if (x + tooltipBox.width > window.innerWidth) {
    x = +event.clientX - offsetX - tooltipBox.width;
  }

  let y = event.clientY + offsetY;
  if (y + tooltipBox.height > window.innerHeight) {
    y = +event.clientY - offsetY - tooltipBox.height;
  }

  return {x, y};
}

// The only major changes from vega-tooltip are in this function.
function formatValue(view: View, value: any, valueToHtml: (value: any) => string, numeralFmtString: string): string {
  if (isArray(value)) {
    return `[${value.map((v) => valueToHtml(isString(v) ? v : JSON.stringify(v))).join(', ')}]`;
  }

  if (!isObject(value)) {
    return valueToHtml(value);
  }
  let content = '';

  const {title, colorScale, isStacked, time_, ...rest} = value as any;

  if (title) {
    content += `<h2>${valueToHtml(title)}</h2>`;
  }

  let sortedItems: Array<{key: string, val: any}> = [];
  for (let [key, val] of Object.entries(rest)) {
    if (val === undefined) {
      continue;
    }
    if (isObject(val)) {
      val = JSON.stringify(val);
    }
    if (isNumber(val)) {
      val = numeral(val).format(numeralFmtString);
    }
    sortedItems.push({key, val});
  }
  if (isStacked) {
    // Sort by key if its a stacked graph.
    sortedItems = _.sortBy(sortedItems, (item: {key: string, val: any}) => item.key);
  } else {
    sortedItems = _.sortBy(sortedItems, (item: {key: string, val: any}) => -Number(item.val) || 0);
  }

  if (sortedItems.length > 0) {
    content += '<table>';
    // Special handling for time_ key.
    if (time_) {
      content += `<tr>`;
      // Add empty color column to be consistent with normal entries.
      if (colorScale) {
        content += `<td></td>`;
      }
      content += `<td class="key">${valueToHtml('time_')}</td>`;
      content += `<td class="value">${valueToHtml(new Date(time_).toLocaleString())}</td>`;

      content += `</tr>`;
    }

    for (const {key, val} of sortedItems) {
      content += `<tr>`;

      // Add color column.
      let shouldShowColor = true;
      if (!colorScale) {
        shouldShowColor = false;
      } else {
        // Handle the case where `colorScale` doesn't exist in vega scales.
        let scale: any;
        try {
          scale = (view as any).scale(colorScale);
        } catch (err) {
          shouldShowColor = false;
        }
        shouldShowColor = shouldShowColor && scale(key);
      }
      if (shouldShowColor) {
        const inlineStyles = `
          height: 6px;
          width: 6px;
          background-color: ${(view as any).scale(colorScale)(key)};
          border-radius: 50%;
          display: inline-block;
          padding: 3px;
          margin-right: 3px;
        `;
        content += `<td><div style="${inlineStyles}"></div></td>`;
      } else {
        content += `<td></td>`;
      }
      // Add key column.
      content += `<td class="key">${valueToHtml(key)}:</td>`;
      // Add val column.
      content += `<td class="value">${valueToHtml(val)}</td>`;

      content += `</tr>`;
    }
    content += `</table>`;
  }

  return content || '{}'; // show empty object if there are no properties
}

/**
 * The tooltip handler class.
 */
export class ColoredTooltipHandler {
  /**
   * The handler function. We bind this to this function in the constructor.
   */
  public call: TooltipHandler;

  /**
   * Complete tooltip options.
   */
  private options: Required<Options>;

  /**
   * The tooltip html element.
   */
  private el: HTMLElement;

  /**
   * The vega View object.
   */
  private view: View;

  /**
   * Create the tooltip handler and initialize the element and style.
   *
   * @param options Tooltip Options
   */
  constructor(view: View, options?: Options) {
    this.view = view;
    this.options = {...DEFAULT_OPTIONS, ...options};
    const elementId = this.options.id;

    // bind this to call
    this.call = this.tooltipHandler.bind(this);

    // append a div element that we use as a tooltip unless it already exists
    let elem = document.getElementById(elementId);
    if (!elem) {
      elem = document.createElement('div');
      elem.setAttribute('id', elementId);
      elem.classList.add('vg-tooltip');

      document.body.appendChild(elem);
    }
    this.el = elem;
  }

  /**
   * The tooltip handler function.
   */
  private tooltipHandler(handler, event: MouseEvent, item, value) {
    // hide tooltip for null, undefined, or empty string values
    if (value == null || value === '' || typeof value === 'undefined') {
      this.el.classList.remove('visible', `${this.options.theme}-theme`);
      return;
    }

    // set the tooltip content
    this.el.innerHTML = formatValue(this.view, value, this.options.sanitize, this.options.numeralFmtString);

    // make the tooltip visible
    this.el.classList.add('visible', `${this.options.theme}-theme`);

    const {x, y} = calculatePosition(
      event,
      this.el.getBoundingClientRect(),
      this.options.offsetX,
      this.options.offsetY,
    );

    this.el.setAttribute('style', `top: ${y}px; left: ${x}px`);
  }
}

/**
 * Escape special HTML characters.
 *
 * @param value A value to convert to string and HTML-escape.
 */
function escapeHTML(value: any): string {
  return String(value).replace(/&/g, '&amp;').replace(/</g, '&lt;');
}
