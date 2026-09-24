/*
 * This file is part of Zygisk Next Next.
 *
 * Zygisk Next Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zygisk Next Next is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zygisk Next Next. If not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2026 VeryBaaad <verybaaad@outlook.com>
 */

export interface IconSpec {
  strokes?: string[];
  fills?: string[];
  strokeWidth?: number;
}

export interface ResolvedIcon {
  strokes: string[];
  fills: string[];
  strokeWidth: number;
}

const dot = (cx: number, cy: number, r: number): string =>
  `M${cx - r} ${cy}a${r} ${r} 0 1 0 ${r * 2} 0a${r} ${r} 0 1 0 ${-r * 2} 0`;

const circle = (r: number): string => dot(12, 12, r);

const MDI_WEB =
  'M16.36 14c.08-.66.14-1.32.14-2s-.06-1.34-.14-2h3.38c.16.64.26 1.31.26 2s-.1 1.36-.26 2' +
  'm-5.15 5.56c.6-1.11 1.06-2.31 1.38-3.56h2.95a8.03 8.03 0 0 1-4.33 3.56' +
  'M14.34 14H9.66c-.1-.66-.16-1.32-.16-2s.06-1.35.16-2h4.68c.09.65.16 1.32.16 2s-.07 1.34-.16 2' +
  'M12 19.96c-.83-1.2-1.5-2.53-1.91-3.96h3.82c-.41 1.43-1.08 2.76-1.91 3.96' +
  'M8 8H5.08A7.92 7.92 0 0 1 9.4 4.44C8.8 5.55 8.35 6.75 8 8' +
  'm-2.92 8H8c.35 1.25.8 2.45 1.4 3.56A8 8 0 0 1 5.08 16' +
  'm-.82-2C4.1 13.36 4 12.69 4 12s.1-1.36.26-2h3.38c-.08.66-.14 1.32-.14 2s.06 1.34.14 2' +
  'M12 4.03c.83 1.2 1.5 2.54 1.91 3.97h-3.82c.41-1.43 1.08-2.77 1.91-3.97' +
  'M18.92 8h-2.95a15.7 15.7 0 0 0-1.38-3.56c1.84.63 3.37 1.9 4.33 3.56' +
  'M12 2C6.47 2 2 6.5 2 12a10 10 0 0 0 10 10a10 10 0 0 0 10-10A10 10 0 0 0 12 2';

const GITHUB_MARK =
  'M12 .297c-6.63 0-12 5.373-12 12 0 5.303 3.438 9.8 8.205 11.385.6.113.82-.258.82-.577 ' +
  '0-.285-.01-1.04-.015-2.04-3.338.724-4.042-1.61-4.042-1.61C4.422 18.07 3.633 17.7 3.633 17.7 ' +
  'c-1.087-.744.084-.729.084-.729 1.205.084 1.838 1.236 1.838 1.236 1.07 1.835 2.809 1.305 ' +
  '3.495.998.108-.776.417-1.305.76-1.605-2.665-.3-5.466-1.332-5.466-5.93 0-1.31.465-2.38 ' +
  '1.235-3.22-.135-.303-.54-1.523.105-3.176 0 0 1.005-.322 3.3 1.23.96-.267 1.98-.399 3-.405 ' +
  '1.02.006 2.04.138 3 .405 2.28-1.552 3.285-1.23 3.285-1.23.645 1.653.24 2.873.12 3.176.765.84 ' +
  '1.23 1.91 1.23 3.22 0 4.61-2.805 5.625-5.475 5.92.42.36.81 1.096.81 2.22 0 1.606-.015 ' +
  '2.896-.015 3.286 0 .315.21.69.825.57C20.565 22.092 24 17.592 24 12.297c0-6.627-5.373-12-12-12';

export const ICONS = {
  refresh: {
    strokes: ['M19.4 12a7.4 7.4 0 1 1-2.17-5.23', 'M19.4 4.4v4.2h-4.2'],
  },
  chevronDown: {
    strokes: ['M6 9.5l6 6 6-6'],
  },
  check: {
    strokes: ['M4.6 12.6l4.9 4.9L19.4 7'],
  },
  sentimentVerySatisfied: {
    strokes: [circle(9)],
    fills: [dot(8.5, 9.5, 1.5), dot(15.5, 9.5, 1.5), 'M7 14a5.125 5.125 0 0 0 10 0z'],
    strokeWidth: 2,
  },
  warning: {
    strokes: [circle(8.4), 'M12 7.6v5.1', 'M12 16.4h.01'],
  },
  themeAuto: {
    strokes: [circle(7.6)],
    fills: ['M12 4.4a7.6 7.6 0 0 1 0 15.2z'],
  },
  themeLight: {
    strokes: [
      circle(3.6),
      'M12 2.6v2.4',
      'M12 19v2.4',
      'M2.6 12H5',
      'M19 12h2.4',
      'M5.35 5.35L7.05 7.05',
      'M16.95 16.95l1.7 1.7',
      'M18.65 5.35l-1.7 1.7',
      'M7.05 16.95l-1.7 1.7',
    ],
  },
  themeDark: {
    fills: ['M20.2 14.8A8.6 8.6 0 0 1 9.2 3.8a8.6 8.6 0 1 0 11 11z'],
  },
  language: {
    fills: [MDI_WEB],
  },
  github: {
    strokes: [],
    fills: [GITHUB_MARK],
  },
} satisfies Record<string, IconSpec>;

export type IconName = keyof typeof ICONS;

export function resolveIcon(name: IconName): ResolvedIcon {
  const spec: IconSpec = ICONS[name];
  return {
    strokes: spec.strokes ?? [],
    fills: spec.fills ?? [],
    strokeWidth: spec.strokeWidth ?? 1.8,
  };
}
