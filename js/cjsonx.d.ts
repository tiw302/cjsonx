export declare function parse(json: string): boolean;
export declare function free(): void;
export declare function getError(): string;
export declare function getErrorOffset(): number;
export declare function dump(maxLen?: number): string;
export declare const ready: Promise<void>;
export declare const isReady: boolean;
