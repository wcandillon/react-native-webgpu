/* eslint-disable @typescript-eslint/no-explicit-any */
export interface GUIParams {
  /**
   * Handles GUI's element placement for you.
   * @default true
   */
  autoPlace?: boolean | undefined;
  /**
   * If true, starts closed.
   * @default false
   */
  closed?: boolean | undefined;
  /**
   * If true, close/open button shows on top of the GUI.
   * @default false
   */
  closeOnTop?: boolean | undefined;
  /**
   * If true, GUI is closed by the "h" keypress.
   * @default true
   */
  hideable?: boolean | undefined;
  /**
   * JSON object representing the saved state of this GUI.
   */

  load?: any;
  /**
   * The name of this GUI.
   */
  name?: string | undefined;
  /**
   * The identifier for a set of saved values.
   */
  preset?: string | undefined;
  /**
   * The width of GUI element.
   */
  width?: number | undefined;
}

// TODO: it would be nice to implement
export class GUI {
  width = 0;
  height = 0;
  isOpen = false;

  static CLASS_AUTO_PLACE: string;
  static CLASS_AUTO_PLACE_CONTAINER: string;
  static CLASS_MAIN: string;
  static CLASS_CONTROLLER_ROW: string;
  static CLASS_TOO_TALL: string;
  static CLASS_CLOSED: string;
  static CLASS_CLOSE_BUTTON: string;
  static CLASS_CLOSE_TOP: string;
  static CLASS_CLOSE_BOTTOM: string;
  static CLASS_DRAG: string;
  static DEFAULT_WIDTH: number;
  static TEXT_CLOSED: string;
  static TEXT_OPEN: string;

  constructor(_option?: GUIParams) {}

  __controllers: GUIController[] = [];
  __folders: { [folderName: string]: GUI } = {};

  open() {
    this.isOpen = true;
  }

  addFolder(propName: string) {
    this.__folders[propName] = new GUI();
    return this;
  }

  add<T extends object>(
    _target: T,
    _propName: keyof T,
    ..._args: any[]
  ): GUIController {
    return new GUIController();
  }

  updateDisplay() {}
}

class GUIController {
  _name: string = "";
  _value: any = null;

  constructor() {}

  listen(): GUIController {
    return this;
  }
  remove(): GUIController {
    return this;
  }

  onChange(_fnc: (value?: any) => void): GUIController {
    return this;
  }
  onFinishChange(_fnc: (value?: any) => void): GUIController {
    return this;
  }

  setValue(_value: any): GUIController {
    return this;
  }
  getValue(): any {
    return this._value;
  }

  name(name: string): GUIController {
    this._name = name;
    return this;
  }
}
