
export class MPosition {

  /** Line number, zero indexed */
  readonly line: number;

  /** Column number, zero indexed */
  readonly column: number;

  /** UTF-16 offset */
  readonly index: number;

  constructor(line: number, column: number, index: number) {
    this.line = line;
    this.column = column;
    this.index = index;
  }

  clone(): MPosition {
    return new MPosition(this.line, this.column, this.index);
  }

  lt(other: MPosition): boolean {
    return this.line === other.line ?
        this.column < other.column :
        this.line < other.line;
  }

  le(other: MPosition): boolean {
    return this.line === other.line ?
        this.column <= other.column :
        this.line <= other.line;
  }

  equals(other: MPosition): boolean {
    return this.line === other.line && this.column === other.column;
  }

  toString() {
    return `MPosition(${this.line}, ${this.column})`;
  }
}
