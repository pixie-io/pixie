const LIVE_VIEW_DATA_DRAWER_OPENED_KEY = 'px-live-data-drawer-opened';
const LIVE_VIEW_EDITOR_OPENED_KEY = 'px-live-editor-opened';
const LIVE_VIEW_EDITOR_SPLITS_KEY = 'px-live-editor-splits';
const LIVE_VIEW_OLD_MODE_KEY = 'px-live-view-mode';
const LIVE_VIEW_PIXIE_SCRIPT_KEY = 'px-live-pixie-script';
const LIVE_VIEW_PLACEMENT_SPEC_KEY = 'px-live-placement';
const LIVE_VIEW_TITLE_KEY = 'px-live-title';
const LIVE_VIEW_VEGA_SPEC_KEY = 'px-live-vega-spec';
const LIVE_VIEW_VIS_SPEC_KEY = 'px-live-vis';

export function getLiveViewDataDrawerOpened(): boolean {
  const stored = localStorage.getItem(LIVE_VIEW_DATA_DRAWER_OPENED_KEY);
  return stored && stored === 'true';
}

export function setLiveViewDataDrawerOpened(opened: boolean) {
  localStorage.setItem(LIVE_VIEW_DATA_DRAWER_OPENED_KEY, String(opened));
}

export function getLiveViewEditorOpened(): boolean {
  const stored = localStorage.getItem(LIVE_VIEW_EDITOR_OPENED_KEY);
  return stored && stored === 'true';
}

export function setLiveViewEditorOpened(open: boolean) {
  localStorage.setItem(LIVE_VIEW_EDITOR_OPENED_KEY, String(open));
}

export function getLiveViewEditorSplits(): [number, number] {
  const stored = localStorage.getItem(LIVE_VIEW_EDITOR_SPLITS_KEY);
  let parsed;
  try {
    parsed = JSON.parse(stored);
  } catch (e) {
    //
  }
  if (!stored || !parsed || parsed.length !== 2) {
    return [50, 50];
  }
  return parsed;
}

export function setLiveViewEditorSplits(splits: [number, number]) {
  localStorage.setItem(LIVE_VIEW_EDITOR_SPLITS_KEY, JSON.stringify(splits));
}

export function getLiveViewVegaSpecOld(): string {
  return localStorage.getItem(LIVE_VIEW_VEGA_SPEC_KEY) || '';
}

export function setLiveViewVegaSpecOld(spec: string) {
  localStorage.setItem(LIVE_VIEW_VEGA_SPEC_KEY, spec);
}

export function getLiveViewPixieScript(): string {
  return localStorage.getItem(LIVE_VIEW_PIXIE_SCRIPT_KEY) || '';
}

export function setLiveViewPixieScript(script: string) {
  localStorage.setItem(LIVE_VIEW_PIXIE_SCRIPT_KEY, script);
}

export function getLiveViewPlacementSpecOld(): string {
  return localStorage.getItem(LIVE_VIEW_PLACEMENT_SPEC_KEY) || '';
}

export function setLiveViewPlacementSpecOld(spec: string) {
  localStorage.setItem(LIVE_VIEW_PLACEMENT_SPEC_KEY, spec);
}

export function getLiveViewVisSpec(): string {
  return localStorage.getItem(LIVE_VIEW_VIS_SPEC_KEY) || '';
}

export function setLiveViewVisSpec(spec: string) {
  localStorage.setItem(LIVE_VIEW_VIS_SPEC_KEY, spec);
}

export function getLiveViewTitle(): { title: string, id: string } {
  const stored = localStorage.getItem(LIVE_VIEW_TITLE_KEY);
  let parsed;
  try {
    parsed = JSON.parse(stored);
  } catch (e) {
    //
  }
  if (!stored || !parsed || typeof parsed !== 'object' || !parsed.title || !parsed.id) {
    return { title: 'untitled', id: 'unknown' };
  }
  return parsed;
}

export function setLiveViewTitle(title: { title: string, id: string }) {
  localStorage.setItem(LIVE_VIEW_TITLE_KEY, JSON.stringify(title));
}

export function getOldLiveViewMode(): boolean {
  const stored = localStorage.getItem(LIVE_VIEW_OLD_MODE_KEY);
  return stored ? stored === 'true' : true;
}

export function setOldLiveViewMode(newMode: boolean) {
  localStorage.setItem(LIVE_VIEW_OLD_MODE_KEY, newMode.toString());
}
