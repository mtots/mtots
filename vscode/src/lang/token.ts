import { MLocation } from "./location";

export const Keywords = [
  'and', 'class', 'def', 'elif', 'else', 'false', 'for',
  'if', 'nil', 'or', 'return', 'super', 'this', 'true',
  'var', 'while', 'as', 'assert', 'async', 'await', 'break',
  'continue', 'del', 'except', 'final', 'finally', 'from',
  'global', 'import', 'in', 'is', 'lambda', 'not', 'pass',
  'raise', 'static', 'then', 'trait', 'try', 'with', 'yield',
] as const;

export const Symbols = [
  // grouping tokens
  '(', ')',
  '[', ']',
  '{', '}',

  // other single character tokens
  ':', ';', ',', '.', '-', '+', '/', '%', '*',
  '@', '|', '&', '^', '~', '?', '!', '=', '<', '>',

  // double character tokens
  '//', '**', '!=', '==', '<<', '<=', '>>', '>='
] as const;

type MTokenTypeKeyword = typeof Keywords[number];

type MTokenTypeSymbol = typeof Symbols[number];

export type MTokenType = (
  'IDENTIFIER' | 'STRING' | 'ERROR' |
  'NUMBER' |
  'NEWLINE' | 'INDENT' | 'DEDENT' | 'EOF' |
  MTokenTypeKeyword |
  MTokenTypeSymbol
);

export type MTokenValue = number | string | null;

export const SymbolsMap: Map<string, MTokenTypeSymbol> = new Map(
  Symbols.map(symbol => [symbol, symbol])
);

export const KeywordsMap: Map<string, MTokenTypeKeyword> = new Map(
  Keywords.map(keyword => [keyword, keyword])
);

export class MToken {
  readonly location: MLocation;
  readonly type: MTokenType;
  readonly value: MTokenValue;
  constructor(location: MLocation, type: MTokenType, value: MTokenValue = null) {
    this.location = location;
    this.type = type;
    this.value = value;
  }
}
