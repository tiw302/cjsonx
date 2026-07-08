export enum Type {
    NULL = 0,
    BOOL = 1,
    NUMBER = 2,
    STRING = 3,
    ARRAY = 4,
    OBJECT = 5
}

export class Value {
    idx: number;
    type: Type;
    size: number;
    bool: boolean;
    num: number;
    str: string;
    get(key: string): Value | null;
    getIndex(index: number): Value | null;
    pointer(path: string): Value | null;
    toJS(): any;
}

export declare function parse(json: string): boolean;
export declare function free(): void;
export declare function getError(): string;
export declare function getErrorOffset(): number;
export declare function dump(maxLen?: number): string;
export declare function getRoot(): Value | null;
export declare const ready: Promise<void>;
export declare const isReady: boolean;
