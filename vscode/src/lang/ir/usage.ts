import { Identifier } from "../ast";
import { Variable } from "./scope";
import type * as ir from "../ir";


/** Records a usage -
 * a variable (specified by the given Variable) was used at a given location
 * (specified by the given Identifier) */
export class Usage {
  readonly identifier: Identifier;
  readonly variable: Variable;

  /** For function calls that involve type inference, it's useful for
   * the hover to also show the reified versions of a function's type.
   */
  boundType: ir.FunctionType | null = null;

  constructor(identifier: Identifier, variable: Variable) {
    this.identifier = identifier;
    this.variable = variable;
  }
}
