import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import { CSSProperties } from '@material-ui/core/styles/withStyles';
import Legend, { LegendInteractState } from 'components/legend/legend';
import { buildHoverDataCache, formatLegendData, HoverDataCache, LegendData } from 'components/legend/legend-data';
import {
  EXTERNAL_HOVER_SIGNAL,
  EXTERNAL_TS_DOMAIN_SIGNAL,
  HOVER_PIVOT_TRANSFORM,
  HOVER_SIGNAL,
  INTERNAL_HOVER_SIGNAL,
  INTERNAL_TS_DOMAIN_SIGNAL,
  LEGEND_HOVER_SIGNAL,
  LEGEND_SELECT_SIGNAL,
  REVERSE_HOVER_SIGNAL,
  REVERSE_SELECT_SIGNAL,
  REVERSE_UNSELECT_SIGNAL,
  VegaSpecWithProps,
} from 'containers/live/convert-to-vega-spec';
import * as _ from 'lodash';
import * as React from 'react';
import { View } from 'vega-typings';

import { VegaContext } from './vega-context';

const useStyles = makeStyles((theme: Theme) => {
  return createStyles({
    flexbox: {
      height: '100%',
      width: '100%',
      display: 'flex',
      flexDirection: 'column',
    },
  });
});

interface VegaProps {
  data: Array<{}>;
  specWithProps: VegaSpecWithProps;
  tableName: string;
  reactVegaModule: any;
  className?: string;
}

const Vega = React.memo((props: VegaProps) => {
  const classes = useStyles();
  const { data: inputData, specWithProps: { spec, hasLegend, legendColumnName, isStacked }, tableName } = props;
  const data = React.useMemo(() => ({ [tableName]: inputData }), [tableName, inputData]);

  const {
    setHoverTime,
    setTimeseriesDomain: setTSDomain,
    hoverTime: externalHoverTime,
    timeseriesDomain: externalTSDomain,
  } = React.useContext(VegaContext);

  const [currentView, setCurrentView] = React.useState<View>(null);
  const [vegaOrigin, setVegaOrigin] = React.useState<number[]>([]);
  const [legendData, setLegendData] = React.useState<LegendData>({ time: '', entries: [] });
  const [legendInteractState, setLegendInteractState] = React.useState<LegendInteractState>(
    { selectedSeries: [], hoveredSeries: '' });
  const [hoverDataCache, setHoverDataCache] = React.useState<HoverDataCache>(null);

  const chartRef = React.useRef(null);

  const setTimeDomainWrapper = (v) => {
    if (!v || v.length === 0) {
      setTSDomain(v);
      return;
    }
    setTSDomain((s: number[]) => {
      if (s && s.length === 2) {
        return [Math.min(s[0], v[0]), Math.max(s[1], v[1])];
      }
      return [v[0].getTime(), v[1].getTime()];
    });
  };

  const widthListener = React.useCallback((name, value) => {
    if (!currentView) {
      return;
    }
    setVegaOrigin(currentView.origin());
  }, [currentView]);

  const hoverListener = React.useCallback((name, value) => {
    if (!currentView || !hoverDataCache) {
      return;
    }
    if (value && value.time_) {
      const unformattedEntries = hoverDataCache.timeHashMap[value.time_];
      if (unformattedEntries) {
        setLegendData(formatLegendData(currentView, value.time_, unformattedEntries));
      }
    }
  }, [currentView, hoverDataCache]);

  const signalListeners = {
    // Add listener for internal tooltip signal.
    // This internal signal is null, unless the chart is active, so this listener only updates the global
    // hover time context, if this is the active chart.
    [INTERNAL_HOVER_SIGNAL]: (name, value) => {
      if (value && value.time_) {
        setHoverTime(value.time_);
      }
    },

    [INTERNAL_TS_DOMAIN_SIGNAL]: (name, value) => {
      if (!value) {
        return;
      }
      if (value.length !== 2) {
        return;
      }
      setTimeDomainWrapper(value);
    },

    // Add signal listener for width, because the origin changes when width changes.
    width: widthListener,
    // Add signal listener for the merged hover signal. This listener updates the values in the legend.
    [HOVER_SIGNAL]: hoverListener,
    [REVERSE_HOVER_SIGNAL]: (name, value) => {
      if (!value) {
        setLegendInteractState((state) => ({...state, hoveredSeries: ''}));
        return;
      }
      setLegendInteractState((state) => {
        return {
          ...state,
          hoveredSeries: value,
        };
      });
    },
    [REVERSE_SELECT_SIGNAL]: (name, value) => {
      if (!value) {
        return;
      }
      setLegendInteractState((state) => {
        if (_.includes(state.selectedSeries, value)) {
          return {...state, selectedSeries: state.selectedSeries.filter((s) => s !== value)};
        } else {
          return {...state, selectedSeries: [...state.selectedSeries, value]};
        }
      });
    },
    [REVERSE_UNSELECT_SIGNAL]: (name, value) => {
      if (!value) {
        return;
      }
      setLegendInteractState((state) => {
        return {...state, selectedSeries: []};
      });
    },
  };

  const onNewView = (view: View) => {
    // Disable default tooltip handling in vega.
    view.tooltip((handler, event, item, value) => {
      return;
    });
    // Add listener for changes to the legend data. If the data changes we rebuild our hash map cache of the data.
    view.addDataListener(HOVER_PIVOT_TRANSFORM, (name, value) => {
      if (value) {
        const cache = buildHoverDataCache(value);
        if (cache) {
          setHoverDataCache(cache);
        }
      }
    });
    // Width listener only kicks in on width changes, so we have to update on new view as well.
    setVegaOrigin(view.origin());
    setCurrentView(view);
  };

  // This effect sets the hover initial state to be the last time value.
  React.useEffect(() => {
    if (currentView && hoverDataCache && hoverDataCache.maxTime) {
      setHoverTime(hoverDataCache.maxTime);
      // set the legend data since the signal listener might not be added yet when we set the hover signal.
      const unformattedEntries = hoverDataCache.timeHashMap[hoverDataCache.maxTime];
      setLegendData(formatLegendData(currentView, hoverDataCache.maxTime, unformattedEntries));
    }
  }, [hoverDataCache, currentView]);

  React.useEffect(() => {
    if (currentView) {
      currentView.signal(EXTERNAL_TS_DOMAIN_SIGNAL, externalTSDomain);
      currentView.runAsync();
    }
  }, [externalTSDomain, currentView]);

  // Inject the selected series into the corresponding vega signal for this chart.
  React.useEffect(() => {
    if (currentView) {
      currentView.signal(LEGEND_HOVER_SIGNAL, legendInteractState.hoveredSeries);
      currentView.signal(LEGEND_SELECT_SIGNAL, legendInteractState.selectedSeries);
      if (externalHoverTime && hoverDataCache) {
        let time = externalHoverTime;
        // Handle cases where externalHoverTime is not contained within the limits of this chart.
        if (externalHoverTime < hoverDataCache.minTime) {
          time = hoverDataCache.minTime;
        } else if (externalHoverTime > hoverDataCache.maxTime) {
          time = hoverDataCache.maxTime;
        }
        currentView.signal(EXTERNAL_HOVER_SIGNAL, { time_: time });
      }
      currentView.runAsync();
    }
  }, [legendInteractState, externalHoverTime, currentView, hoverDataCache]);

  // If this chart has a legend, then make vega use only 80% of the height and leave 20% for the legend.
  const vegaStyles: CSSProperties = React.useMemo(() => {
    return { height: (hasLegend) ? '85%' : '100%' };
  }, [hasLegend]);
  const legendStyles: CSSProperties = React.useMemo(() => {
    return { height: (hasLegend) ? '15%' : '0%' };
  }, [hasLegend]);

  return (
    <div className={props.className}>
      <div className={classes.flexbox} ref={chartRef}>
        <props.reactVegaModule.Vega
          spec={spec}
          data={data}
          actions={false}
          onNewView={onNewView}
          style={vegaStyles}
          // Have to check for currentView, otherwise vega complains about signals not existing.
          signalListeners={!currentView ? {} : signalListeners}
        />
        <div style={legendStyles}>
          {!hasLegend ? null :
            <Legend
              data={legendData}
              vegaOrigin={vegaOrigin}
              chartWidth={chartRef.current ? chartRef.current.getBoundingClientRect().width : 0}
              name={legendColumnName}
              interactState={legendInteractState}
              setInteractState={setLegendInteractState}
            />
          }
        </div>
      </div>
    </div>
  );
});

export default Vega;
