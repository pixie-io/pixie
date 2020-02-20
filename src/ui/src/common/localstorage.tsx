const LIVE_VIEW_EDITOR_OPENED_KEY = 'px-live-editor-opened';
const LIVE_VIEW_EDITOR_SPLITS_KEY = 'px-live-editor-splits';

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
