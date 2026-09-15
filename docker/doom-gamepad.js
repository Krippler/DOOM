//
// A game controller, translated into the keys and mouse the engine already
// understands.
//
// Nothing in the 1997 engine knows what a gamepad is, and nothing needs to:
// the picture arrives over VNC and the input goes back the same way, as X
// keysyms and pointer reports. So a controller is entirely a client-side
// concern -- this file turns buttons into key presses and sticks into the same
// relative mouse motion the pointer-lock path produces, and the engine cannot
// tell the difference.
//
// That is also why the bindings live here rather than in `.doomrc`. The
// container never sees a controller, so it has nothing to save; these are kept
// in the browser, per device, which is where "my Xbox pad on the desktop and a
// Backbone on the phone want different layouts" actually lives.
//
// The Gamepad API is a polling API by design -- there are no button events --
// so this is driven from the page's existing per-frame loop rather than a
// timer of its own.
//
import KeyTable from './core/input/keysym.js';

//
// What a button can be bound to.
//
// Each action is a list of keysyms rather than one, because two of them have
// to mean different things in the game and in a menu and the page has no way
// to know which is up. `act` sends space (open a door) and Return (choose a
// menu item) together: in play the Return does nothing, in a menu the space
// does nothing, and one button does the obvious thing in both places.
//
// The keys are the engine's defaults, from m_misc.c. They are what the engine
// listens for out of the box -- rebind something under Options → Setup →
// Controls and the matching action here has to be pointed at the new key too,
// because this sends keys, not intentions.
//
// Each carries the DOM code name beside its keysym so that what goes on the
// wire is byte for byte what the same key on a keyboard would put there. noVNC
// sends a QEMU extended key event, carrying a scancode, where it has a code
// name and the server supports the extension, and a plain keysym event
// otherwise -- so a keysym with no code name takes a different path from the
// real keyboard. Both paths are legal and the keyboard's is the one already
// known to work here, so this takes it too rather than relying on the other
// being equivalent.
//
const K = KeyTable;

export const ACTIONS = [
  { id: 'fire',        label: 'Fire',              keys: [[K.XK_Control_L, 'ControlLeft']] },
  { id: 'act',         label: 'Open / use',        keys: [[K.XK_space, 'Space'],
                                                         [K.XK_Return, 'Enter']],
    note: 'and confirms in menus' },
  { id: 'run',         label: 'Run',               keys: [[K.XK_Shift_L, 'ShiftLeft']] },
  { id: 'strafemod',   label: 'Strafe modifier',   keys: [[K.XK_Alt_L, 'AltLeft']],
    note: 'unbound: the left stick already sidesteps' },
  { id: 'forward',     label: 'Forward',           keys: [[K.XK_Up, 'ArrowUp']] },
  { id: 'back',        label: 'Back',              keys: [[K.XK_Down, 'ArrowDown']] },
  { id: 'turnleft',    label: 'Turn left',         keys: [[K.XK_Left, 'ArrowLeft']] },
  { id: 'turnright',   label: 'Turn right',        keys: [[K.XK_Right, 'ArrowRight']] },
  { id: 'strafeleft',  label: 'Sidestep left',     keys: [[K.XK_comma, 'Comma']],
    note: 'unbound: the left stick does this' },
  { id: 'straferight', label: 'Sidestep right',    keys: [[K.XK_period, 'Period']],
    note: 'unbound: the left stick does this' },
  { id: 'menu',        label: 'Game menu',         keys: [[K.XK_grave, 'Backquote']] },
  { id: 'map',         label: 'Automap',           keys: [[K.XK_Tab, 'Tab']] },
  { id: 'back_out',    label: 'Back out of menus', keys: [[K.XK_Escape, 'Escape']],
    note: 'the game\u2019s Escape, not this page\u2019s' },
  { id: 'weapon1',     label: 'Fist / chainsaw',   keys: [[K.XK_1, 'Digit1']] },
  { id: 'weapon2',     label: 'Pistol',            keys: [[K.XK_2, 'Digit2']] },
  { id: 'weapon3',     label: 'Shotgun',           keys: [[K.XK_3, 'Digit3']] },
  { id: 'weapon4',     label: 'Chaingun',          keys: [[K.XK_4, 'Digit4']] },
  { id: 'weapon5',     label: 'Rocket launcher',   keys: [[K.XK_5, 'Digit5']] },
  { id: 'weapon6',     label: 'Plasma rifle',      keys: [[K.XK_6, 'Digit6']] },
  { id: 'weapon7',     label: 'BFG9000',           keys: [[K.XK_7, 'Digit7']],
    note: 'unbound: one weapon more than there are buttons' },
];


const ACTION_BY_ID = new Map(ACTIONS.map(a => [a.id, a]));

export function defaultKeysFor(actionId) {
  const a = ACTION_BY_ID.get(actionId);
  return a ? a.keys : [];
}

//
// The engine's own key numbers, back to the keysyms that produce them.
//
// This is i_video.c's `xlatekey` read backwards. That function turns an X
// keysym into the number the engine stores in `.doomrc`, so inverting it says
// which keysym to send to press a key the engine has been bound to. The values
// are doomdef.h's KEY_* constants.
//
const DOOM_KEY_TO_X = new Map([
  [0xae, [K.XK_Right,     'ArrowRight']],   // KEY_RIGHTARROW
  [0xac, [K.XK_Left,      'ArrowLeft']],    // KEY_LEFTARROW
  [0xad, [K.XK_Up,        'ArrowUp']],      // KEY_UPARROW
  [0xaf, [K.XK_Down,      'ArrowDown']],    // KEY_DOWNARROW
  [27,   [K.XK_Escape,    'Escape']],
  [13,   [K.XK_Return,    'Enter']],
  [9,    [K.XK_Tab,       'Tab']],
  [127,  [K.XK_BackSpace, 'Backspace']],
  [0xff, [K.XK_Pause,     'Pause']],
  [0x80 + 0x36, [K.XK_Shift_L,   'ShiftLeft']],    // KEY_RSHIFT
  [0x80 + 0x1d, [K.XK_Control_L, 'ControlLeft']],  // KEY_RCTRL
  [0x80 + 0x38, [K.XK_Alt_L,     'AltLeft']],      // KEY_RALT
]);
for (let i = 0; i < 12; i++)                        // KEY_F1 .. KEY_F12
  DOOM_KEY_TO_X.set(0x80 + 0x3b + i, [K['XK_F' + (i + 1)], 'F' + (i + 1)]);

// The DOM code name for a printable character, so a config-derived key goes on
// the wire the same way a typed one does. Only the ones a binding is plausibly
// set to; anything else sends the keysym with no code, which is still valid.
const ASCII_CODES = {
  ' ': 'Space', ',': 'Comma', '.': 'Period', '/': 'Slash', ';': 'Semicolon',
  "'": 'Quote', '[': 'BracketLeft', ']': 'BracketRight', '\\': 'Backslash',
  '`': 'Backquote', '-': 'Minus', '=': 'Equal',
};

export function doomKeyToX(code) {
  const n = Number(code);
  if (!Number.isInteger(n)) return null;
  if (DOOM_KEY_TO_X.has(n)) return DOOM_KEY_TO_X.get(n);

  // Everything else the engine stores is a printable character, and xlatekey
  // maps those to themselves -- so the keysym is the number.
  if (n >= 0x20 && n <= 0x7e) {
    const ch = String.fromCharCode(n);
    let dom = null;
    if (ch >= 'a' && ch <= 'z') dom = 'Key' + ch.toUpperCase();
    else if (ch >= '0' && ch <= '9') dom = 'Digit' + ch;
    else if (ASCII_CODES[ch]) dom = ASCII_CODES[ch];
    return [n, dom];
  }
  return null;
}

// Which `.doomrc` setting each action is pressing.
const ACTION_DOOMRC = {
  fire: 'key_fire',        act: 'key_use',            run: 'key_speed',
  strafemod: 'key_strafe', forward: 'key_up',         back: 'key_down',
  turnleft: 'key_left',    turnright: 'key_right',    menu: 'key_menu',
  strafeleft: 'key_strafeleft', straferight: 'key_straferight',
};

//
// The standard mapping's button order, which is what an Xbox pad and a
// Backbone One both report. Anything claiming `mapping: "standard"` puts the
// face buttons at 0-3, the shoulders at 4-7, view/menu at 8-9, the stick
// clicks at 10-11 and the d-pad at 12-15.
//
// Weapons sit on the face and shoulder buttons because there is nothing else
// for them to sit on: the 1997 engine has no next-weapon or previous-weapon
// key, only "select weapon N", and a page cannot cycle for you because it has
// no idea which weapons you are carrying -- a digit for a weapon you have not
// picked up is silently ignored, so a cycle would stall on the gaps.
//
// The BFG is deliberately left unbound. There are more weapons than there are
// comfortable buttons, and it is the one you will miss least.
//
export const BUTTON_NAMES = [
  'A', 'B', 'X', 'Y', 'LB', 'RB', 'LT', 'RT',
  'View', 'Menu', 'Left stick', 'Right stick',
  'D-pad up', 'D-pad down', 'D-pad left', 'D-pad right', 'Guide',
];

export const DEFAULT_BINDINGS = {
  0: 'act',         1: 'back_out',    2: 'weapon3',    3: 'weapon2',
  4: 'weapon4',     5: 'weapon5',     6: 'run',        7: 'fire',
  8: 'map',         9: 'menu',        10: 'weapon1',   11: 'weapon6',
  12: 'forward',    13: 'back',       14: 'turnleft',  15: 'turnright',
};

export const DEFAULT_SETTINGS = {
  enabled:    true,
  deadzone:   0.18,   // how far a stick must move before it counts
  turnSpeed:  14,     // mouse pixels per frame at full deflection
  turnCurve:  2,      // >1 puts finer control near the centre
  invertTurn: false,
  swapSticks: false,  // left stick turns, right stick moves
  autoRun:    true,   // push the stick all the way to break into a run
  runAt:      0.85,
  moveAt:     0.35,   // a digital key needs this much of the stick
};

const STORE_KEY = 'doom.gamepad.v1';

//
// Which key each action presses, where the engine is not on its defaults.
//
// The keys in ACTIONS are what a fresh `.doomrc` listens for. Anybody who has
// been through Options → Setup → Controls has a different one, and the page
// cannot read it -- it lives in the container, which has never heard of a
// controller. The result is a pad that works perfectly in menus, where the
// engine hardcodes the arrows and Return, and does nothing in the game, where
// every action goes through a binding that has been changed.
//
// So an action's key can be learned from the keyboard instead: press the key
// the game actually uses and the pad presses that from then on. Overrides only
// -- anything not set here still uses the default above.
//

//
// Keysym numbers back to their names, for the readout in the panel.
//
// Built from noVNC's own table rather than written out, so a name in the
// readout is the name of the thing actually put on the wire. First entry wins
// where several names share a number, which is why the readout can say
// something slightly unexpected for an obscure key; the ones this file sends
// are all early in that table and come out right.
//
const KEY_NAMES = (() => {
  const m = new Map();
  for (const [name, value] of Object.entries(K))
    if (typeof value === 'number' && !m.has(value)) m.set(value, name.slice(3));
  return m;
})();

export function keyName(keysym) {
  return KEY_NAMES.get(keysym) || ('0x' + Number(keysym).toString(16));
}

// A stick, past its deadzone, rescaled so the first countable movement is a
// small one rather than a jump to 18% of full speed.
function curve(v, deadzone, exponent) {
  const m = Math.abs(v);
  if (m <= deadzone) return 0;
  const scaled = (m - deadzone) / (1 - deadzone);
  return Math.sign(v) * Math.pow(scaled, exponent);
}

// Deadzone the stick as a stick rather than as two separate axes, so a
// diagonal is not held to a higher bar than a straight push.
function stick(x, y, deadzone) {
  const m = Math.hypot(x, y);
  if (m <= deadzone) return [0, 0, 0];
  const scaled = (m - deadzone) / (1 - deadzone);
  return [(x / m) * scaled, (y / m) * scaled, scaled];
}

export class DoomGamepad {
  //
  // `press` and `release` are handed a keysym and the DOM code name that goes
  // with it; `turn` a number of mouse pixels to move sideways. The page
  // supplies all three, so this file never touches the RFB connection and can
  // be exercised without one.
  //
  constructor({ press, release, turn, onChange } = {}) {
    this._press = press || (() => {});
    this._release = release || (() => {});
    this._turn = turn || (() => {});
    this._onChange = onChange || (() => {});

    this.bindings = { ...DEFAULT_BINDINGS };
    this.settings = { ...DEFAULT_SETTINGS };
    this.keys = {};             // actionId -> [[keysym, code], ...], learned
    this.engineKeys = {};       // ... and the same, read from the engine's config
    this.engineSeen = false;
    this._load();

    // Keysyms currently held down on the engine's behalf, against the code
    // name each was pressed with. Keyed by keysym rather than counted: two
    // buttons bound to the same action both press one key, and letting go of
    // one of them must not release it while the other is still held.
    this._held = new Map();

    // The last few keys put on the wire, for the panel. Kept because the panel
    // can only be read when the game is not being played, so "what did it send
    // while I was playing" is a question that can only be answered afterwards.
    this._log = [];

    this._capture = null;   // a pending "press a button to bind it"
    this._lastPoll = 0;
    this._seen = new Set(); // pad ids, so a reconnect is not announced twice

    // A pad that goes away mid-game is holding nothing, whatever it was
    // holding a moment ago.
    window.addEventListener('gamepaddisconnected', () => {
      this.releaseAll();
      this._onChange();
    });
    window.addEventListener('gamepadconnected', () => this._onChange());
  }

  //
  // The Gamepad API hands out snapshots, not live objects: getGamepads() has
  // to be called again every frame or the button states never change.
  //
  pads() {
    if (!navigator.getGamepads) return [];
    return Array.from(navigator.getGamepads()).filter(p => p && p.connected);
  }

  // The one being played with. First connected pad wins; there is no
  // multiplayer here to want the second.
  pad() {
    return this.pads()[0] || null;
  }

  available() {
    return !!this.pad();
  }

  //
  // Called once per animation frame by the page.
  //
  // `active` is whether input should reach the game at all -- false while the
  // start screen is up, so a pad knocked off a desk cannot fire into a game
  // nobody is looking at. Binding still reads buttons when inactive, which is
  // the whole point of the remap panel.
  //
  poll(active) {
    const pad = this.pad();
    const now = performance.now();
    const dt = this._lastPoll ? Math.min(100, now - this._lastPoll) : 16.7;
    this._lastPoll = now;

    if (!pad || !this.settings.enabled) {
      this.releaseAll();
      return;
    }

    if (this._capture) {
      this._takeCapture(pad);
      return;
    }

    if (!active) {
      this.releaseAll();
      return;
    }

    const want = new Map();     // keysym -> DOM code name
    const add = id => {
      for (const [sym, code] of this.keysFor(id)) want.set(sym, code);
    };

    // Buttons. A trigger reports an analog `value` as well as `pressed`, and
    // the standard mapping's own threshold is what the pad decided; take
    // either, so a half-squeezed trigger still fires.
    pad.buttons.forEach((b, i) => {
      const down = b && (b.pressed || b.value > 0.5);
      if (!down) return;
      const id = this.bindings[i];
      if (id) add(id);
    });

    // Sticks.
    const s = this.settings;
    const ax = i => (typeof pad.axes[i] === 'number' ? pad.axes[i] : 0);
    const [moveAxes, turnAxes] = s.swapSticks ? [[2, 3], [0, 1]] : [[0, 1], [2, 3]];

    const [mx, my, mag] = stick(ax(moveAxes[0]), ax(moveAxes[1]), s.deadzone);
    if (my < -s.moveAt) add('forward');
    if (my > s.moveAt) add('back');
    if (mx < -s.moveAt) add('strafeleft');
    if (mx > s.moveAt) add('straferight');
    if (s.autoRun && mag > s.runAt) add('run');

    this._apply(want);

    // Turning goes out as mouse motion, which is analog where a key is not:
    // the engine turns by however many pixels it is told, so a gentle push
    // turns gently. The frame time is folded in so a 30 Hz tab and a 144 Hz
    // one turn at the same rate.
    const t = curve(ax(turnAxes[0]), s.deadzone, s.turnCurve);
    if (t) {
      const dx = t * s.turnSpeed * (dt / 16.7) * (s.invertTurn ? -1 : 1);
      if (dx) this._turn(dx);
    }
  }

  // Send only the differences. Re-pressing a key that is already down every
  // frame would work, but it would also put sixty key events a second on a
  // link this project spent fifteen releases making quiet.
  _apply(want) {
    for (const [sym, code] of [...this._held]) {
      if (!want.has(sym)) {
        this._held.delete(sym);
        this._release(sym, code);
        this._note(sym, false);
      }
    }
    for (const [sym, code] of want) {
      if (!this._held.has(sym)) {
        this._held.set(sym, code);
        this._press(sym, code);
        this._note(sym, true);
      }
    }
  }

  _note(keysym, down) {
    this._log.push({ keysym, down, at: Math.round(performance.now()) });
    if (this._log.length > 24) this._log.shift();
  }

  //
  // Let go of everything, now.
  //
  // The same hazard as a mouse button held while the window goes away: a key
  // left down is a key the engine goes on obeying, which in DOOM means firing
  // into an empty room until something else happens.
  //
  releaseAll() {
    if (!this._held.size) return;
    for (const [sym, code] of this._held) { this._release(sym, code); this._note(sym, false); }
    this._held.clear();
  }

  //
  // Everything the page knows about the pad, for the panel to display.
  //
  // Read-only and side-effect free on purpose: the panel needs this while the
  // start screen is up, which is exactly when poll() refuses to send anything,
  // so the two cannot share a path.
  //
  // It exists because the first field report of this feature was "left and
  // right work, the menu button works, nothing else does" -- and there was no
  // way to tell from here whether the buttons were not being read, not being
  // sent, or not being understood at the far end. Three different faults with
  // one symptom is what the rest of this project spent eleven releases on.
  //
  snapshot() {
    const pad = this.pad();
    const round = v => Math.round((Number(v) || 0) * 100) / 100;
    return {
      pads: this.pads().length,
      id: pad ? pad.id : null,
      mapping: pad ? (pad.mapping || '(not standard)') : null,
      buttonCount: pad ? pad.buttons.length : 0,
      axisCount: pad ? pad.axes.length : 0,
      // Every button showing any movement at all, not just the ones over the
      // threshold, so a trigger that only ever reaches 0.4 is visible.
      active: pad ? pad.buttons
        .map((b, i) => ({ i, pressed: !!(b && b.pressed), value: round(b && b.value) }))
        .filter(b => b.pressed || b.value > 0.05) : [],
      axes: pad ? Array.from(pad.axes, round) : [],
      held: [...this._held.keys()],
      log: this._log.slice(-14),
      enabled: this.settings.enabled,
    };
  }

  //
  // "Press a button to bind it." Resolves with the button index, or null if
  // cancelled. Buttons only -- a stick has a job already.
  //
  captureButton() {
    this.cancelCapture();
    return new Promise(resolve => {
      // Ignore whatever is already held, or the button that opened the panel
      // binds itself the instant capture starts.
      const pad = this.pad();
      const ignore = new Set();
      if (pad) pad.buttons.forEach((b, i) => {
        if (b && (b.pressed || b.value > 0.5)) ignore.add(i);
      });
      this._capture = { resolve, ignore };
    });
  }

  cancelCapture() {
    if (!this._capture) return;
    const { resolve } = this._capture;
    this._capture = null;
    resolve(null);
  }

  _takeCapture(pad) {
    const cap = this._capture;
    for (let i = 0; i < pad.buttons.length; i++) {
      const b = pad.buttons[i];
      const down = b && (b.pressed || b.value > 0.5);
      if (!down) { cap.ignore.delete(i); continue; }
      if (cap.ignore.has(i)) continue;
      this._capture = null;
      cap.resolve(i);
      return;
    }
  }

  //
  // Bindings.
  //
  bind(buttonIndex, actionId) {
    if (actionId === null || actionId === undefined) delete this.bindings[buttonIndex];
    else this.bindings[buttonIndex] = actionId;
    this._save();
    this._onChange();
  }

  //
  // The keys an action presses: what was learned from the keyboard, or the
  // engine's default if nothing was.
  //
  keysFor(actionId) {
    const own = this.keys[actionId];
    if (own && own.length) return own;
    const eng = this.engineKeys[actionId];
    if (eng && eng.length) return eng;
    const a = ACTION_BY_ID.get(actionId);
    return a ? a.keys : [];
  }

  // Where an action's key came from, for the panel to say so.
  keySource(actionId) {
    if (this.keys[actionId] && this.keys[actionId].length) return 'learned';
    if (this.engineKeys[actionId] && this.engineKeys[actionId].length) return 'engine';
    return 'default';
  }

  //
  // Take the keys from the engine's own `.doomrc`, which the container reads and
  // serves because the page cannot see it.
  //
  // This is what stops the pad working in menus and nowhere else: the engine
  // hardcodes the arrows and Return in its menus but reads these settings during
  // play, so a pad on the built-in defaults goes dead in a level the moment
  // anybody has been through Options → Setup → Controls. Anything learned by
  // hand still wins over this; anything absent or unrecognised leaves the
  // built-in default alone.
  //
  useEngineKeys(config) {
    this.engineSeen = true;
    const out = {};
    if (config && typeof config === 'object') {
      for (const [id, setting] of Object.entries(ACTION_DOOMRC)) {
        const raw = config[setting];
        if (raw === undefined || raw === null) continue;
        const pair = doomKeyToX(raw);
        if (!pair) continue;
        // `act` has to confirm in menus as well as open doors, and Return is
        // hardcoded there, so it keeps its second key whatever key_use says.
        out[id] = id === 'act' && pair[0] !== K.XK_Return
          ? [pair, [K.XK_Return, 'Enter']]
          : [pair];
      }
    }
    this.engineKeys = out;
    this._onChange();
  }

  isCustomKey(actionId) {
    return !!(this.keys[actionId] && this.keys[actionId].length);
  }

  // One key, learned from a real keypress. The keysym and code name come from
  // noVNC's own translation of the event, so what the pad sends afterwards is
  // byte for byte what pressing that key sends.
  setKey(actionId, keysym, code) {
    if (!ACTION_BY_ID.has(actionId)) return;
    if (!keysym) return;
    this.keys[actionId] = [[keysym, code || null]];
    this._save();
    this._onChange();
  }

  clearKey(actionId) {
    delete this.keys[actionId];
    this._save();
    this._onChange();
  }

  //
  // Point an action at a button, taking it off whatever button had it.
  //
  // Two buttons doing the same thing is harmless to play but confusing to
  // read, and the panel shows one button per action, so a rebind moves the
  // binding rather than adding a second one.
  //
  bindAction(actionId, buttonIndex) {
    for (const [i, id] of Object.entries(this.bindings))
      if (id === actionId) delete this.bindings[i];
    if (buttonIndex !== null && buttonIndex !== undefined)
      this.bindings[buttonIndex] = actionId;
    this._save();
    this._onChange();
  }

  // Which button currently presses this action, if any.
  buttonFor(actionId) {
    for (const [i, id] of Object.entries(this.bindings))
      if (id === actionId) return Number(i);
    return null;
  }

  set(key, value) {
    this.settings[key] = value;
    this._save();
    this._onChange();
  }

  reset() {
    this.bindings = { ...DEFAULT_BINDINGS };
    this.settings = { ...DEFAULT_SETTINGS };
    this.keys = {};
    this._save();
    this._onChange();
  }

  //
  // Kept in this browser, because this is where the controller is. Wrapped
  // because storage throws rather than returns in a private window, and a pad
  // that works but forgets is better than a page that does not load.
  //
  _save() {
    try {
      localStorage.setItem(STORE_KEY, JSON.stringify({
        bindings: this.bindings, settings: this.settings, keys: this.keys,
      }));
    } catch (e) { /* nothing to do about it */ }
  }

  _load() {
    let raw;
    try { raw = localStorage.getItem(STORE_KEY); } catch (e) { return; }
    if (!raw) return;
    let saved;
    try { saved = JSON.parse(raw); } catch (e) { return; }
    if (!saved || typeof saved !== 'object') return;

    // Take only what is recognised. A binding to an action this build no
    // longer has, or a setting it never had, is dropped rather than trusted.
    if (saved.bindings && typeof saved.bindings === 'object') {
      const clean = {};
      for (const [i, id] of Object.entries(saved.bindings)) {
        const n = Number(i);
        if (Number.isInteger(n) && n >= 0 && n < 32 && ACTION_BY_ID.has(id))
          clean[n] = id;
      }
      this.bindings = clean;
    }
    if (saved.keys && typeof saved.keys === 'object') {
      const clean = {};
      for (const [id, pairs] of Object.entries(saved.keys)) {
        if (!ACTION_BY_ID.has(id) || !Array.isArray(pairs)) continue;
        const ok = pairs.filter(pr => Array.isArray(pr) && typeof pr[0] === 'number'
                                      && pr[0] > 0
                                      && (pr[1] === null || typeof pr[1] === 'string'));
        if (ok.length) clean[id] = ok.map(pr => [pr[0], pr[1] || null]);
      }
      this.keys = clean;
    }
    if (saved.settings && typeof saved.settings === 'object') {
      for (const [k, v] of Object.entries(saved.settings)) {
        if (!(k in DEFAULT_SETTINGS)) continue;
        if (typeof v !== typeof DEFAULT_SETTINGS[k]) continue;
        this.settings[k] = v;
      }
    }
  }
}
