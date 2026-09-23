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

export type CubicBezier = (x: number) => number;

const coefficientA = (p1: number, p2: number): number => 1 - 3 * p2 + 3 * p1;

const coefficientB = (p1: number, p2: number): number => 3 * p2 - 6 * p1;

const coefficientC = (p1: number): number => 3 * p1;

const bezierAt = (t: number, p1: number, p2: number): number =>
  ((coefficientA(p1, p2) * t + coefficientB(p1, p2)) * t + coefficientC(p1)) * t;

const bezierSlope = (t: number, p1: number, p2: number): number =>
  3 * coefficientA(p1, p2) * t * t + 2 * coefficientB(p1, p2) * t + coefficientC(p1);

export function cubicBezier(x1: number, y1: number, x2: number, y2: number): CubicBezier {
  return (x: number): number => {
    if (x <= 0) return 0;
    if (x >= 1) return 1;

    let t = x;
    for (let i = 0; i < 8; i += 1) {
      const error = bezierAt(t, x1, x2) - x;
      if (Math.abs(error) < 1e-5) break;
      const slope = bezierSlope(t, x1, x2);
      if (slope === 0) break;
      t -= error / slope;
    }
    return bezierAt(t, y1, y2);
  };
}
