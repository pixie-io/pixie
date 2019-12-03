const PIXIE_EDITOR_CODE_KEY_PREFIX = 'px-';

export function codeIdToKey(id: string): string {
  return `${PIXIE_EDITOR_CODE_KEY_PREFIX}${id}`;
}

export function saveCodeToStorage(id: string, code: string) {
  localStorage.setItem(codeIdToKey(id), code);
}

export function getCodeFromStorage(id: string): string {
  return localStorage.getItem(codeIdToKey(id)) || '';
}
