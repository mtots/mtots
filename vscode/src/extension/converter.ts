import * as vscode from "vscode"
import { MLocation } from "../lang/location";
import { MPosition } from "../lang/position";
import { MRange } from "../lang/range";


export function convertFilePath(filePath: string | vscode.Uri): vscode.Uri {
  if (typeof filePath === 'string') {
    filePath = vscode.Uri.file(filePath);
  }
  return filePath;
}

export function convertMPosition(position: MPosition): vscode.Position {
  return new vscode.Position(position.line, position.column);
}

export function convertMRange(range: MRange): vscode.Range {
  return new vscode.Range(
    convertMPosition(range.start),
    convertMPosition(range.end));
}

export function convertMLocation(location: MLocation): vscode.Location {
  return new vscode.Location(
    convertFilePath(location.filePath),
    convertMRange(location.range));
}

export function convertPosition(position: vscode.Position): MPosition {
  return new MPosition(position.line, position.character, 0);
}

export function convertRange(range: vscode.Range): MRange {
  return new MRange(convertPosition(range.start), convertPosition(range.end));
}

export function convertLocation(location: vscode.Location): MLocation {
  return new MLocation(location.uri, convertRange(location.range));
}
