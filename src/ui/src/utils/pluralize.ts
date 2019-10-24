export function pluralize(singular: string, count: number, plural = `${singular}s`): string {
  return Math.abs(count) === 1 ? singular : plural;
}
