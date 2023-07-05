import { Uri } from "vscode";
import { MPosition } from "./position";
import { MRange } from "./range";


export class MLocation {
  readonly filePath: string | Uri;
  readonly range: MRange;

  static of(filePath: string | Uri): MLocation {
    const position = new MPosition(0, 0, 0);
    return new MLocation(filePath, new MRange(position, position));
  }

  constructor(filePath: string | Uri, range: MRange) {
    this.filePath = filePath;
    this.range = range;
  }

  merge(other: MLocation | null | undefined): MLocation {
    if (!other) {
      return this;
    }
    if (this.filePath !== other.filePath) {
      throw new Error(`assertionError ${this.filePath}, ${other.filePath}`);
    }
    return new MLocation(
      this.filePath,
      this.range.merge(other.range));
  }

  overlaps(other: MLocation): boolean {
    return this.filePath === other.filePath && this.range.overlaps(other.range);
  }
}

export const BUILTIN_LOCATION = new MLocation(
  '(builtin)',
  new MRange(
    new MPosition(0, 0, 0),
    new MPosition(0, 0, 0)));
