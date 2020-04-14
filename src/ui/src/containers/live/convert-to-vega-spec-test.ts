import {convertWidgetDisplayToVegaSpec} from './convert-to-vega-spec';

// The output specs are all fully enumerated to make it easy to use https://vega.github.io/editor
// to verify that they look right.

// Disabling specific tslint rules to support easy copy paste of results into vega editor for visual inspection.
// tslint:disable quotemark object-literal-key-quotes trailing-comma

// Use this data for validating timeseries output specs in the Vega Lite editor.
// "data": {
//  "values": [
//    {"time_": "4/2/2020, 9:42:38 PM", "service": "px-sock-shop/catalogue", "bytes_per_second": 48259},
//    {"time_": "4/2/2020, 9:42:38 PM", "service": "px-sock-shop/orders", "bytes_per_second": 234},
//    {"time_": "4/2/2020, 9:42:39 PM", "service": "px-sock-shop/catalogue", "bytes_per_second": 52234},
//    {"time_": "4/2/2020, 9:42:39 PM", "service": "px-sock-shop/orders", "bytes_per_second": 23423},
//    {"time_": "4/2/2020, 9:42:40 PM", "service": "px-sock-shop/catalogue", "bytes_per_second": 18259},
//    {"time_": "4/2/2020, 9:42:40 PM", "service": "px-sock-shop/orders", "bytes_per_second": 28259},
//    {"time_": "4/2/2020, 9:42:41 PM", "service": "px-sock-shop/catalogue", "bytes_per_second": 38259},
//    {"time_": "4/2/2020, 9:42:42 PM", "service": "px-sock-shop/orders", "bytes_per_second": 10259},
//    {"time_": "4/2/2020, 9:42:43 PM", "service": "px-sock-shop/catalogue", "bytes_per_second": 58259}
//  ]
// }
// replace all instances of colorFieldName with a string, and all instances of valueFieldName with another string.

// When series is not provided to the Vis spec, we generate a random name for the series column
// so we need to extract that random name using this function.
function extractRandomFieldNamessFromSpec(spec): {colorFieldName: string, valueFieldName: string} {
  if (!spec || !spec.transform || spec.transform.length === 0 || !spec.transform[0] || !spec.transform[0].as) {
    return {colorFieldName: "", valueFieldName: ""};
  }
  return {
    colorFieldName: spec.transform[0].as[0],
    valueFieldName: spec.transform[0].as[1],
  };
}

describe('simple timeseries', () => {
  it('produces the expected spec for a simple case', () => {
    const input = {
      "@type": "pixielabs.ai/pl.vispb.TimeseriesChart",
      "timeseries": [
        {
          "value": "bytes_per_second",
          "mode": "MODE_LINE"
        }
      ],
    };
    const spec = convertWidgetDisplayToVegaSpec(input, "mysource");
    const {colorFieldName, valueFieldName} = extractRandomFieldNamessFromSpec(spec);
    expect(colorFieldName).toBeTruthy();
    expect(valueFieldName).toBeTruthy();
    expect(spec).toStrictEqual({
      "$schema": "https://vega.github.io/schema/vega-lite/v4.json",
      "data": {
        "name": "mysource",
      },
      "encoding": {
       "x": {
          "axis": {
            "grid": false
          },
          "field": "time_",
          "title": null,
          "type": "temporal"
        }
      },
      "layer": [
        {
          "encoding": {
            "color": {
              "field": colorFieldName,
              "legend": null,
              "type": "nominal"
            },
            "y": {
              "field": "bytes_per_second",
              "type": "quantitative"
            }
          },
          "layer": [
            {"mark": "line"}
          ]
        },
        {
          "encoding": {
            "opacity": {
              "condition": {"selection": "hover", "value": 0.3},
              "value": 0,
            }
          },
          "mark": "rule",
          "selection": {
            "hover": {
              "clear": "mouseout",
              "empty": "none",
              "fields": ["time_"],
              "nearest": true,
              "on": "mouseover",
              "type": "single"
            }
          },
          "transform": [
            {
              "groupby": ["time_"],
              "pivot": colorFieldName,
              "value": "bytes_per_second",
            }
          ]
        }
      ],
      "transform": [
        {
          "as": [colorFieldName, valueFieldName],
          "fold": ["bytes_per_second"],
        }
      ]
    });
  });

  it('produces a spec with a custom title and custom x/y axis titles', () => {
    const input = {
      "@type": "pixielabs.ai/pl.vispb.TimeseriesChart",
      "title": "My custom title",
      "timeseries": [
        {
          "value": "bytes_per_second",
          "mode": "MODE_LINE"
        },
      ],
      "xAxis": {"label": "My custom x axis title"},
      "yAxis": {"label": "My custom y axis title"}
    };
    const spec = convertWidgetDisplayToVegaSpec(input, "mysource");
    const {colorFieldName, valueFieldName} = extractRandomFieldNamessFromSpec(spec);
    expect(colorFieldName).toBeTruthy();
    expect(valueFieldName).toBeTruthy();
    expect(spec).toStrictEqual({
      "$schema": "https://vega.github.io/schema/vega-lite/v4.json",
      "data": {
        "name": "mysource",
      },
      "encoding": {
       "x": {
          "axis": {
            "grid": false
          },
          "field": "time_",
          "title": "My custom x axis title",
          "type": "temporal"
        }
      },
      "title": "My custom title",
      "layer": [
        {
          "encoding": {
            "y": {
              "field": "bytes_per_second",
              "title": "My custom y axis title",
              "type": "quantitative"
            },
            "color": {
              "field": colorFieldName,
              "legend": null,
              "type": "nominal"
            }
          },
          "layer": [
            {"mark": "line"}
          ]
        },
        {
          "encoding": {
            "opacity": {
              "condition": {"selection": "hover", "value": 0.3},
              "value": 0,
            }
          },
          "mark": "rule",
          "selection": {
            "hover": {
              "clear": "mouseout",
              "empty": "none",
              "fields": ["time_"],
              "nearest": true,
              "on": "mouseover",
              "type": "single"
            }
          },
          "transform": [
            {
              "groupby": ["time_"],
              "pivot": colorFieldName,
              "value": "bytes_per_second",
            }
          ]
        }
      ],
      "transform": [
        {
          "as": [colorFieldName, valueFieldName],
          "fold": ["bytes_per_second"],
        }
      ]
    });
  });

  it('produces a spec with bars', () => {
    const input = {
      "@type": "pixielabs.ai/pl.vispb.TimeseriesChart",
      "timeseries": [
        {
          "value": "bytes_per_second",
          "mode": "MODE_BAR"
        }
      ],
    };
    const spec = convertWidgetDisplayToVegaSpec(input, "mysource");
    const {colorFieldName, valueFieldName} = extractRandomFieldNamessFromSpec(spec);
    expect(colorFieldName).toBeTruthy();
    expect(valueFieldName).toBeTruthy();
    expect(spec).toStrictEqual({
      "$schema": "https://vega.github.io/schema/vega-lite/v4.json",
      "data": {
        "name": "mysource",
      },
      "encoding": {
       "x": {
          "axis": {
            "grid": false
          },
          "field": "time_",
          "title": null,
          "type": "temporal"
        }
      },
      "layer": [
        {
          "encoding": {
            "y": {
              "field": "bytes_per_second",
              "type": "quantitative"
            },
            "color": {
              "field": colorFieldName,
              "legend": null,
              "type": "nominal"
            }
          },
          "layer": [
            {"mark": "bar"}
          ]
        },
        {
          "encoding": {
            "opacity": {
              "condition": {"selection": "hover", "value": 0.3},
              "value": 0,
            }
          },
          "mark": "rule",
          "selection": {
            "hover": {
              "clear": "mouseout",
              "empty": "none",
              "fields": ["time_"],
              "nearest": true,
              "on": "mouseover",
              "type": "single"
            }
          },
          "transform": [
            {
              "groupby": ["time_"],
              "pivot": colorFieldName,
              "value": "bytes_per_second",
            }
          ]
        }
      ],
      "transform": [
        {
          "as": [colorFieldName, valueFieldName],
          "fold": ["bytes_per_second"],
        }
      ]
    });
  });

  it('produces a spec with points', () => {
    const input = {
      "@type": "pixielabs.ai/pl.vispb.TimeseriesChart",
      "timeseries": [
        {
          "value": "bytes_per_second",
          "mode": "MODE_POINT"
        }
      ],
    };
    const spec = convertWidgetDisplayToVegaSpec(input, "mysource");
    const {colorFieldName, valueFieldName} = extractRandomFieldNamessFromSpec(spec);
    expect(colorFieldName).toBeTruthy();
    expect(valueFieldName).toBeTruthy();
    expect(spec).toStrictEqual({
      "$schema": "https://vega.github.io/schema/vega-lite/v4.json",
      "data": {
        "name": "mysource",
      },
      "encoding": {
       "x": {
          "axis": {
            "grid": false
          },
          "field": "time_",
          "title": null,
          "type": "temporal"
        }
      },
      "layer": [
        {
          "encoding": {
            "y": {
              "field": "bytes_per_second",
              "type": "quantitative"
            },
            "color": {
              "field": colorFieldName,
              "legend": null,
              "type": "nominal"
            }
          },
          "layer": [
            {"mark": "point"}
          ]
        },
        {
          "encoding": {
            "opacity": {
              "condition": {"selection": "hover", "value": 0.3},
              "value": 0,
            }
          },
          "mark": "rule",
          "selection": {
            "hover": {
              "clear": "mouseout",
              "empty": "none",
              "fields": ["time_"],
              "nearest": true,
              "on": "mouseover",
              "type": "single"
            }
          },
          "transform": [
            {
              "groupby": ["time_"],
              "pivot": colorFieldName,
              "value": "bytes_per_second",
            }
          ]
        }
      ],
      "transform": [
        {
          "as": [colorFieldName, valueFieldName],
          "fold": ["bytes_per_second"],
        }
      ]
    });
  });
});

describe('timeseries with series', () => {
  it('produces the expected spec', () => {
    const input = {
      "@type": "pixielabs.ai/pl.vispb.TimeseriesChart",
      "timeseries": [
        {
          "value": "bytes_per_second",
          "mode": "MODE_LINE",
          "series": "service"
        }
      ],
    };
    expect(convertWidgetDisplayToVegaSpec(input, "mysource")).toStrictEqual({
      "$schema": "https://vega.github.io/schema/vega-lite/v4.json",
      "data": {
        "name": "mysource",
      },
      "encoding": {
       "x": {
          "axis": {
            "grid": false
          },
          "field": "time_",
          "title": null,
          "type": "temporal"
        }
      },
      "layer": [
        {
          "encoding": {
            "y": {
              "field": "bytes_per_second",
              "type": "quantitative"
            },
            "color": {
              "field": "service",
              "legend": null,
              "type": "nominal"
            }
          },
          "layer": [
            {"mark": "line"}
          ]
        },
        {
          "encoding": {
            "opacity": {
              "condition": {"selection": "hover", "value": 0.3},
              "value": 0,
            }
          },
          "mark": "rule",
          "selection": {
            "hover": {
              "clear": "mouseout",
              "empty": "none",
              "fields": ["time_"],
              "nearest": true,
              "on": "mouseover",
              "type": "single"
            }
          },
          "transform": [
            {
              "groupby": ["time_"],
              "pivot": "service",
              "value": "bytes_per_second",
            }
          ]
        }
      ]
    });
  });

  it('produces the expected spec with a stacked series', () => {
    const input = {
      "@type": "pixielabs.ai/pl.vispb.TimeseriesChart",
      "timeseries": [
        {
          "value": "bytes_per_second",
          "mode": "MODE_LINE",
          "series": "service",
          "stackBySeries": true
        }
      ],
    };
    expect(convertWidgetDisplayToVegaSpec(input, "mysource")).toStrictEqual({
      "$schema": "https://vega.github.io/schema/vega-lite/v4.json",
      "data": {
        "name": "mysource",
      },
      "encoding": {
       "x": {
          "axis": {
            "grid": false
          },
          "field": "time_",
          "title": null,
          "type": "temporal"
        }
      },
      "layer": [
        {
          "encoding": {
            "y": {
              "field": "bytes_per_second",
              "type": "quantitative",
              "stack": "zero",
              "aggregate": "sum"
            },
            "color": {
              "field": "service",
              "legend": null,
              "type": "nominal"
            }
          },
          "layer": [
            {"mark": "line"}
          ]
        },
        {
          "encoding": {
            "opacity": {
              "condition": {"selection": "hover", "value": 0.3},
              "value": 0,
            }
          },
          "mark": "rule",
          "selection": {
            "hover": {
              "clear": "mouseout",
              "empty": "none",
              "fields": ["time_"],
              "nearest": true,
              "on": "mouseover",
              "type": "single"
            }
          },
          "transform": [
            {
              "groupby": ["time_"],
              "pivot": "service",
              "value": "bytes_per_second",
            }
          ]
        }
      ],
    });
  });
});

// Use this data for verifying bar chart specs in the Vega Lite editor.
// "data": {
//   "values": [
//     {"service": "carts", "endpoint": "/create", "cluster": "prod", "num_errors": 14},
//     {"service": "carts", "endpoint": "/create", "cluster": "staging", "num_errors": 60},
//     {"service": "carts", "endpoint": "/create", "cluster": "dev", "num_errors": 3},
//     {"service": "carts", "endpoint": "/create", "cluster": "prod", "num_errors": 80},
//     {"service": "carts", "endpoint": "/create", "cluster": "staging", "num_errors": 38},
//     {"service": "carts", "endpoint": "/update", "cluster": "dev", "num_errors": 55},
//     {"service": "carts", "endpoint": "/submit", "cluster": "prod", "num_errors": 11},
//     {"service": "carts", "endpoint": "/submit", "cluster": "staging", "num_errors": 58},
//     {"service": "carts", "endpoint": "/submit", "cluster": "dev", "num_errors": 79},
//     {"service": "orders", "endpoint": "/remove", "cluster": "prod", "num_errors": 83},
//     {"service": "orders", "endpoint": "/remove", "cluster": "staging", "num_errors": 87},
//     {"service": "orders", "endpoint": "/remove", "cluster": "dev", "num_errors": 67},
//     {"service": "orders", "endpoint": "/add", "cluster": "prod", "num_errors": 97},
//     {"service": "orders", "endpoint": "/add", "cluster": "staging", "num_errors": 84},
//     {"service": "orders", "endpoint": "/add", "cluster": "dev", "num_errors": 90},
//     {"service": "orders", "endpoint": "/add", "cluster": "prod", "num_errors": 74},
//     {"service": "orders", "endpoint": "/new", "cluster": "staging", "num_errors": 64},
//     {"service": "orders", "endpoint": "/new", "cluster": "dev", "num_errors": 19},
//     {"service": "frontend", "endpoint": "/orders", "cluster": "prod", "num_errors": 57},
//     {"service": "frontend", "endpoint": "/orders", "cluster": "staging", "num_errors": 35},
//     {"service": "frontend", "endpoint": "/orders", "cluster": "dev", "num_errors": 49},
//     {"service": "frontend", "endpoint": "/redirect", "cluster": "prod", "num_errors": 91},
//     {"service": "frontend", "endpoint": "/signup", "cluster": "staging", "num_errors": 38},
//     {"service": "frontend", "endpoint": "/signup", "cluster": "dev", "num_errors": 91},
//     {"service": "frontend", "endpoint": "/signup", "cluster": "prod", "num_errors": 99},
//     {"service": "frontend", "endpoint": "/signup", "cluster": "staging", "num_errors": 80},
//     {"service": "frontend", "endpoint": "/signup", "cluster": "dev", "num_errors": 37}
//   ]
// }

describe('bar', () => {
  it('produces the expected spec for a simple case', () => {
    const input = {
      "@type": "pixielabs.ai/pl.vispb.BarChart",
      "bar": {
        "label": "service",
        "value": "num_errors"
      }
    };
    expect(convertWidgetDisplayToVegaSpec(input, "mysource")).toStrictEqual({
      "$schema": "https://vega.github.io/schema/vega-lite/v4.json",
      "data": {
        "name": "mysource",
      },
      "encoding": {
        "x": {
          "field": "service",
          "type": "ordinal"
        },
        "y": {
          "field": "num_errors",
          "type": "quantitative"
        }
      },
      "mark": "bar"
    });
  });

  it('produces a spec with a custom title and custom x/y axis titles', () => {
    const input = {
      "@type": "pixielabs.ai/pl.vispb.BarChart",
      "bar": {
        "label": "service",
        "value": "num_errors"
      },
      "title": "My custom title",
      "xAxis": {"label": "My custom x axis"},
      "yAxis": {"label": "My custom y axis"}
    };
    expect(convertWidgetDisplayToVegaSpec(input, "mysource")).toStrictEqual({
      "$schema": "https://vega.github.io/schema/vega-lite/v4.json",
      "title": "My custom title",
      "data": {
        "name": "mysource",
      },
      "encoding": {
        "x": {
          "field": "service",
          "type": "ordinal",
          "title": "My custom x axis"
        },
        "y": {
          "field": "num_errors",
          "type": "quantitative",
          "title": "My custom y axis"
        }
      },
      "mark": "bar"
    });
  });

  it('produces a spec with a stack by series', () => {
    const input = {
      "@type": "pixielabs.ai/pl.vispb.BarChart",
      "bar": {
        "label": "service",
        "value": "num_errors",
        "stackBy": "endpoint"
      }
    };
    expect(convertWidgetDisplayToVegaSpec(input, "mysource")).toStrictEqual({
      "$schema": "https://vega.github.io/schema/vega-lite/v4.json",
      "data": {
        "name": "mysource",
      },
      "encoding": {
        "color": {
          "field": "endpoint",
          "type": "nominal"
        },
        "x": {
          "field": "service",
          "type": "ordinal"
        },
        "y": {
          "aggregate": "sum",
          "field": "num_errors",
          "type": "quantitative"
        }
      },
      "mark": "bar"
    });
  });
});

describe('grouped bar', () => {
  it('produces the expected spec', () => {
    const input = {
      "@type": "pixielabs.ai/pl.vispb.BarChart",
      "bar": {
        "label": "service",
        "value": "num_errors",
        "groupBy": "cluster"
      }
    };
    expect(convertWidgetDisplayToVegaSpec(input, "mysource")).toStrictEqual({
      "$schema": "https://vega.github.io/schema/vega-lite/v4.json",
      "data": {
        "name": "mysource",
      },
      "encoding": {
        "column": {
          "field": "cluster",
          "header": {
            "labelOrient": "bottom",
            "title": "cluster, service",
            "titleOrient": "bottom"
          },
          "type": "nominal"
        },
        "x": {
          "title": null,
          "field": "service",
          "type": "ordinal"
        },
        "y": {
          "field": "num_errors",
          "type": "quantitative"
        }
      },
      "mark": "bar"
    });
  });

  it('produces a spec with a title and custom x and y axis titles', () => {
    const input = {
      "@type": "pixielabs.ai/pl.vispb.BarChart",
      "bar": {
        "label": "service",
        "value": "num_errors",
        "groupBy": "cluster",
      },
      "title": "My custom title",
      "xAxis": {"label": "My custom x axis"},
      "yAxis": {"label": "My custom y axis"}
    };
    expect(convertWidgetDisplayToVegaSpec(input, "mysource")).toStrictEqual({
      "$schema": "https://vega.github.io/schema/vega-lite/v4.json",
      "data": {
        "name": "mysource",
      },
      "title": {
        "anchor": "middle",
        "text": "My custom title"
      },
      "encoding": {
        "column": {
          "field": "cluster",
          "header": {
            "labelOrient": "bottom",
            "title": "My custom x axis",
            "titleOrient": "bottom"
          },
          "type": "nominal"
        },
        "x": {
          "field": "service",
          "type": "ordinal",
          "title": null
        },
        "y": {
          "field": "num_errors",
          "type": "quantitative",
          "title": "My custom y axis"
        }
      },
      "mark": "bar"
    });
  });

  it('produces a spec with a stack by series', () => {
    const input = {
      "@type": "pixielabs.ai/pl.vispb.BarChart",
      "bar": {
        "label": "service",
        "value": "num_errors",
        "groupBy": "cluster",
        "stackBy": "endpoint",
      }
    };
    expect(convertWidgetDisplayToVegaSpec(input, "mysource")).toStrictEqual({
      "$schema": "https://vega.github.io/schema/vega-lite/v4.json",
      "data": {
        "name": "mysource",
      },
      "encoding": {
        "column": {
          "field": "cluster",
          "header": {
            "labelOrient": "bottom",
            "title": "cluster, service",
            "titleOrient": "bottom"
          },
          "type": "nominal"
        },
        "x": {
          "title": null,
          "field": "service",
          "type": "ordinal"
        },
        "y": {
          "field": "num_errors",
          "type": "quantitative",
          "aggregate": "sum"
        },
        "color": {
          "field": "endpoint",
          "type": "nominal"
        }
      },
      "mark": "bar"
    });
  });

  it('produces a spec with a stack by series and custom x and y axis titles', () => {
    const input = {
      "@type": "pixielabs.ai/pl.vispb.BarChart",
      "bar": {
        "label": "service",
        "value": "num_errors",
        "stackBy": "endpoint",
        "groupBy": "cluster"
      },
      "title": "My custom title",
      "xAxis": {"label": "My custom x axis"},
      "yAxis": {"label": "My custom y axis"}
    };
    expect(convertWidgetDisplayToVegaSpec(input, "mysource")).toStrictEqual({
      "$schema": "https://vega.github.io/schema/vega-lite/v4.json",
      "data": {
        "name": "mysource",
      },
      "title": {
        "anchor": "middle",
        "text": "My custom title"
      },
      "encoding": {
        "column": {
          "field": "cluster",
          "header": {
            "labelOrient": "bottom",
            "title": "My custom x axis",
            "titleOrient": "bottom"
          },
          "type": "nominal"
        },
        "x": {
          "title": null,
          "field": "service",
          "type": "ordinal"
        },
        "y": {
          "field": "num_errors",
          "type": "quantitative",
          "title": "My custom y axis",
          "aggregate": "sum"
        },
        "color": {
          "field": "endpoint",
          "type": "nominal"
        }
      },
      "mark": "bar"
    });
  });
});

const testInputVega = {
  "$schema": "https://vega.github.io/schema/vega/v5.json",
  "width": 400,
  "height": 200,
  "padding": 5,
  "scales": [
    {
      "name": "xscale",
      "type": "band",
      "domain": {"data": "table", "field": "category"},
      "range": "width",
      "padding": 0.05,
      "round": true
    },
    {
      "name": "yscale",
      "domain": {"data": "table", "field": "amount"},
      "nice": true,
      "range": "height"
    }
  ],
  "axes": [
    { "orient": "bottom", "scale": "xscale" },
    { "orient": "left", "scale": "yscale" }
  ],
  "marks": [
    {
      "type": "rect",
      "from": {"data": "table"},
      "encode": {
        "enter": {
          "x": {"scale": "xscale", "field": "category"},
          "width": {"scale": "xscale", "band": 1},
          "y": {"scale": "yscale", "field": "amount"},
          "y2": {"scale": "yscale", "value": 0}
        }
      }
    }
  ]
};

describe('vega spec', () => {
  it('produces the expected spec for vega lite', () => {
    const inputVegaLite = {
      "$schema": "https://vega.github.io/schema/vega-lite/v2.json",
      "mark": "bar",
      "encoding": {
        "x": {"field": "a", "type": "ordinal", "axis": {"labelAngle": 0}},
        "y": {"field": "b", "type": "quantitative"}
      }
    };
    const input = {
      "@type": "pixielabs.ai/pl.vispb.VegaChart",
      "spec": JSON.stringify(inputVegaLite),
    };
    expect(convertWidgetDisplayToVegaSpec(input, "mysource")).toStrictEqual({
      "$schema": "https://vega.github.io/schema/vega-lite/v2.json",
      "data": {
        "name": "mysource"
      },
      "encoding": {
        "x": {
          "axis": {
            "labelAngle": 0
          },
          "field": "a",
          "type": "ordinal"
        },
        "y": {
          "field": "b",
          "type": "quantitative"
        }
      },
      "mark": "bar"
    });
  });

  it('produces the expected spec for vega lite (no $schema field)', () => {
    const inputVegaLite = {
      "mark": "bar",
      "encoding": {
        "x": {"field": "a", "type": "ordinal", "axis": {"labelAngle": 0}},
        "y": {"field": "b", "type": "quantitative"}
      }
    };
    const input = {
      "@type": "pixielabs.ai/pl.vispb.VegaChart",
      "spec": JSON.stringify(inputVegaLite),
    };
    expect(convertWidgetDisplayToVegaSpec(input, "mysource")).toStrictEqual({
      "$schema": "https://vega.github.io/schema/vega-lite/v4.json",
      "data": {
        "name": "mysource"
      },
      "encoding": {
        "x": {
          "axis": {
            "labelAngle": 0
          },
          "field": "a",
          "type": "ordinal"
        },
        "y": {
          "field": "b",
          "type": "quantitative"
        }
      },
      "mark": "bar"
    });
  });

  it('produces the expected spec for vega (not lite)', () => {
    const input = {
      "@type": "pixielabs.ai/pl.vispb.VegaChart",
      spec: JSON.stringify(testInputVega),
    };
    expect(convertWidgetDisplayToVegaSpec(input, "mysource")).toStrictEqual({
      ...testInputVega,
      "data": [
        {"name": "mysource"},
      ]
    });
  });

  it('produces the expected spec for vega (not lite) with an existing source', () => {
    const existingData = {
      "name": "table",
      "values": [
        {"category": "A", "amount": 28},
        {"category": "B", "amount": 55},
        {"category": "C", "amount": 43},
        {"category": "D", "amount": 91},
        {"category": "E", "amount": 81},
        {"category": "F", "amount": 53},
        {"category": "G", "amount": 19},
        {"category": "H", "amount": 87}
      ]
    };

    const testVegaWithData = {
      ...testInputVega,
      "data": [existingData],
    };
    const input = {
      "@type": "pixielabs.ai/pl.vispb.VegaChart",
      spec: JSON.stringify(testVegaWithData),
    };
    expect(convertWidgetDisplayToVegaSpec(input, "mysource")).toStrictEqual({
      ...testInputVega,
      "data": [existingData, {"name": "mysource"}]
    });
  });
});
