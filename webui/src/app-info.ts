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

declare const __ZNN_MODULE_ID__: string;
declare const __ZNN_MODULE_NAME__: string;
declare const __ZNN_VER_NAME__: string;
declare const __ZNN_COMMIT_HASH__: string;

export const MODULE_ID: string = __ZNN_MODULE_ID__;
export const MODULE_NAME: string = __ZNN_MODULE_NAME__;
export const VER_NAME: string = __ZNN_VER_NAME__;
export const COMMIT_HASH: string = __ZNN_COMMIT_HASH__;
export const GITHUB_URL: string = 'https://github.com/VeryBaaad/ZygiskNextNext';
