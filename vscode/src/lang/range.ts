import { MPosition } from "./position";


export class MRange {
  readonly start: MPosition;
  readonly end: MPosition;

  constructor(start: MPosition, end: MPosition) {
    this.start = start;
    this.end = end;
  }

  merge(other: MRange) {
    return new MRange(
      (this.start.lt(other.start) ? this.start : other.start).clone(),
      (this.end.lt(other.end) ? other.end : this.end).clone());
  }

  overlaps(other: MRange): boolean {
    return this.start.lt(other.end) && other.start.lt(this.end);
  }

  contains(position: MPosition): boolean {
    return this.start.le(position) && position.lt(this.end);
  }

  toString() {
    return `MRange(${this.start}, ${this.end})`;
  }
}
