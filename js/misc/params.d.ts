export type Anyify<D extends { [key: string]: any }> = { [key in keyof D]?: any };

export function parse<D extends { [key: string]: any }, P extends { [key: string]: any }>(params: P, defaults: D, allowExtras: true): D & typeof params;
export function parse<D extends { [key: string]: any }, P extends Anyify<D>>(params: P, defaults: D, allowExtras: false): D;
export function parse<D extends { [key: string]: any }, P extends Anyify<D>>(params: P, defaults: D): D;
export function parse<D extends { [key: string]: any }, P extends Anyify<D> | { [key: string]: any }>(params: P, defaults: D, allowExtras: boolean): D | D & typeof params;
