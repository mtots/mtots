import { MLocation } from "./location";


export class MError {
  location: MLocation;
  message: string;
  constructor(location: MLocation, message: string) {
    this.location = location;
    this.message = message;
  }
}
