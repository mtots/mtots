import { Uri } from "vscode";
import { KeywordsMap, MToken, MTokenType, MTokenValue, SymbolsMap } from "./token";
import { MPosition } from "./position";
import { MRange } from "./range";
import { MLocation } from "./location";
import { swrap } from "../debug";

const stringLiteralReplaceMap = new Map([
  ['\\n', '\n'],
  ['\\t', '\t'],
  ['\\r', '\r'],
  ['\\"', '"'],
  ["\\'", "'"],
  ['\\\\', '\\'],
]);
const stringLiteralReplaceRe = new RegExp(
  Array.
    from(stringLiteralReplaceMap.keys()).
    map(k => k.replace(/\\/g, '\\\\')).join('|'), "g"
);
function stringLiteralUnescape(s: string): string {
  return s.replace(
    stringLiteralReplaceRe,
    (matched) => stringLiteralReplaceMap.get(matched) || '');
}

export function lex(filePath: string | Uri, s: string): MToken[] {
  return swrap('lex', filePath, () => _lex(filePath, s));
}

function _lex(filePath: string | Uri, s: string): MToken[] {
  const tokens: MToken[] = [];
  var indentPotential = 0, indentationLevel = 0, depth = 0;
  var i = 0, line = 0, column = 0;
  var startPos = new MPosition(0, 0, 0);

  function incr() {
    if (i < s.length) {
      if (s[i] === '\n') {
        line++;
        column = 0;
      } else {
        column++;
      }
      i++;
    }
  }
  function getPos(): MPosition {
    return new MPosition(line, column, i);
  }
  function getRange(): MRange {
    return new MRange(startPos, getPos());
  }
  function getLoc(): MLocation {
    return new MLocation(filePath, getRange());
  }
  function getSlice(a=0, b=0): string {
    return s.slice(startPos.index + a, i + b);
  }
  function emit(type: MTokenType, value: MTokenValue=null) {
    tokens.push(new MToken(getLoc(), type, value));
  }
  function cur(): string {
    return peek(0);
  }
  function peek(j: number): string {
    return i + j < s.length ? s[i + j] : '';
  }
  function skipSpacesAndComments() {
    while (true) {
      if (isSpace(cur(), depth)) {
        while (isSpace(cur(), depth)) {
          incr();
        }
        continue;
      }
      if (cur() === '#') {
        while (i < s.length && cur() !== '\n') {
          incr();
        }
        continue;
      }
      break;
    }
  }

  while (true) {
    skipSpacesAndComments();
    startPos = getPos();
    while (indentPotential > 0) {
      indentPotential--;
      emit('INDENT');
    }
    while (indentPotential < 0) {
      indentPotential++;
      emit('DEDENT');
    }
    if (i >= s.length) {
      emit('NEWLINE'); // synthetic newline
      emit('EOF');
      break;
    }
    const c = cur();
    if ((c === '"' || c === "'") ||
        (c === 'r' && (peek(1) === '"' || peek(1) === "'"))) {
      const raw = c === 'r';
      if (raw) {
        incr();
      }
      const qc = cur();
      const quote = peek(1) === qc && peek(2) === qc ?
        qc + qc + qc : qc;
      for (let j = 0; j < quote.length; j++) {
        incr();
      }
      while (i < s.length && !s.startsWith(quote, i)) {
        if (cur() === '\\') {
          incr();
        }
        incr();
      }
      const slice = getSlice((raw ? 1 : 0) + quote.length);
      const value = raw ? slice : stringLiteralUnescape(slice);
      if (i < s.length && s.startsWith(quote, i)) {
        for (let j = 0; j < quote.length; j++) {
          incr();
        }
      } else {
        emit('ERROR', `Unterminated string literal`);
      }
      emit('STRING', value);
      continue;
    }
    if (c === '_' || isLetter(c)) {
      while (isIdent(cur())) {
        incr();
      }
      const name = getSlice();
      const keyword = KeywordsMap.get(name);
      if (keyword) {
        emit(keyword);
      } else {
        emit('IDENTIFIER', name);
      }
      continue;
    }
    if (isDigit(c)) {
      if (c === '0') {
        switch (peek(1)) {
          case 'x':
            incr();
            incr();
            while (isHexDigit(cur())) {
              incr();
            }
            emit('NUMBER', parseInt(getSlice(2), 16));
            continue;
          case 'b':
            incr();
            incr();
            while (isBinDigit(cur())) {
              incr();
            }
            emit('NUMBER', parseInt(getSlice(2), 2));
            continue;
        }
      }
      while (isDigit(cur())) {
        incr();
      }
      if (cur() === '.') {
        incr();
        while (isDigit(cur())) {
          incr();
        }
      }
      emit('NUMBER', parseFloat(getSlice()));
      continue;
    }
    const symbol1 = SymbolsMap.get(c);
    if (symbol1) {
      incr();
      const symbol2 = SymbolsMap.get(c + cur());
      if (symbol2 && symbol2 !== symbol1) {
        incr();
        emit(symbol2);
      } else {
        emit(symbol1);
        switch (symbol1) {
          case '(':
          case '[':
          case '{':
            depth++;
            break;
          case ')':
          case ']':
          case '}':
            depth--;
            break;
        }
      }
      continue;
    }
    if (c === '\n') {
      incr();
      emit('NEWLINE');
      while (cur() === '\r' || cur() === '\n') {
        incr();
      }
      let spaceCount = 0;
      while (cur() === ' ') {
        incr();
        spaceCount++;
      }
      if (spaceCount % 2 !== 0) {
        emit('ERROR', `Indentations must always be a multiple of 2 but got ${spaceCount}`);
      }
      const newIndentationLevel = spaceCount / 2;
      indentPotential = newIndentationLevel - indentationLevel;
      indentationLevel = newIndentationLevel;
      continue;
    }
    while (i < s.length && !isSpace(cur(), depth)) {
      incr();
    }
    emit('ERROR', `Unrecognized token ${getSlice()}`);
  }
  return tokens;
}


function isDigit(c: string): boolean {
  switch (c) {
    case '0':case '1':case '2':case '3':case '4':
    case '5':case '6':case '7':case '8':case '9':
      return true;
  }
  return false;
}

function isBinDigit(c: string): boolean {
  return c === '0' || c === '1';
}

function isHexDigit(c: string): boolean {
  switch (c.toUpperCase()) {
    case 'A':case 'B':case 'C':case 'D':case 'E':
    case 'F':
      return true;
  }
  return isDigit(c);
}

function isLetter(c: string): boolean {
  switch (c.toUpperCase()) {
    case 'A':case 'B':case 'C':case 'D':case 'E':
    case 'F':case 'G':case 'H':case 'I':case 'J':
    case 'K':case 'L':case 'M':case 'N':case 'O':
    case 'P':case 'Q':case 'R':case 'S':case 'T':
    case 'U':case 'V':case 'W':case 'X':case 'Y':
    case 'Z':
      return true;
  }
  return false;
}

function isIdent(c: string): boolean {
  return c === '_' || isLetter(c) || isDigit(c);
}

function isSpace(c: string, depth: number): boolean {
  switch (c) {
    case ' ':
    case '\t':
    case '\r':
      return true;
    case '\n':
      return depth > 0;
  }
  return false;
}
