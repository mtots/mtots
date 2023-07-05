import type * as ir from "../ir";
import { MLocation } from "../location";



/** Signature Help */
export class SigHelp {
  readonly location: MLocation;
  readonly functionName: string | null;
  readonly functionDocumentation: string | null;
  readonly parameterTypes: ir.Type[];
  readonly parameterNames: string[] | null;
  readonly parameterIndex: number;
  readonly returnType: ir.Type | null;
  constructor(
      location: MLocation,
      functionName: string | null,
      functionDocumentation: string | null,
      parameterTypes: ir.Type[],
      parameterNames: string[] | null,
      parameterIndex: number,
      returnType: ir.Type | null) {
    this.location = location;
    this.functionName = functionName;
    this.functionDocumentation = functionDocumentation;
    this.parameterTypes = parameterTypes;
    this.parameterNames = parameterNames;
    this.parameterIndex = parameterIndex;
    this.returnType = returnType;
  }
}
