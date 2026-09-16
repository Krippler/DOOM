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
  { id: 'act',         label: 'Open / use',        keys: [[K.XK_space, 'Space']],
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

//
// One key per action, and exactly one.
//
// 1.10.56 sent the configured key *and* the menu's hardcoded one, so that a
// rebound control still worked the menus. It worked and it was wrong: the menu
// treats any other character as a hotkey for the item starting with it, so
// pressing down sent 's', moved the cursor one line with the arrow and then
// jumped to SAVE GAME -- four lines from where it started. Two keys also meant
// two Returns for Open/use, which selects twice.
//
// The engine maps a bound control onto the menu's own key instead, in
// M_Responder, which is where it belongs: the keys you walk with are the keys
// you navigate with, and nothing is sent twice.
//
function oneKey(pair) {
  return [pair];
}

export function doomKeyToX(code) {
  const n = Number(code);
  if (!Number.isInteger(n)) return null;
  if (DOOM_KEY_TO_X.has(n)) return DOOM_KEY_TO_X.get(n);

  // A printable character: xlatekey maps those to themselves, so the keysym is
  // the number.
  if (n >= 0x20 && n <= 0x7e) {
    const ch = String.fromCharCode(n);
    let dom = null;
    if (ch >= 'a' && ch <= 'z') dom = 'Key' + ch.toUpperCase();
    else if (ch >= '0' && ch <= '9') dom = 'Digit' + ch;
    else if (ASCII_CODES[ch]) dom = ASCII_CODES[ch];
    return [n, dom];
  }

  //
  // Anything larger is already a keysym.
  //
  // `xlatekey`'s default branch returns the keysym unchanged for everything it
  // does not recognise, so a binding set to a keypad key, Caps Lock, the Menu
  // key or a right-hand modifier is stored as that keysym -- 0xffe5, 0xff8d and
  // so on. Sending it straight back is exactly right, and leaving this out was
  // a hole: such a setting fell through to the built-in default, so the pad
  // pressed Ctrl at an engine listening for something else, silently.
  //
  // No DOM code name is known for these, which is allowed -- noVNC then sends a
  // plain keysym event, the same as any client without a scancode.
  //
  if (n > 0xff) return [n, null];

  return null;
}

//
// The mouse button the engine will also accept for an action.
//
// `G_BuildTiccmd` reads fire as `gamekeydown[key_fire] || mousebuttons[mousebfire]`
// and strafe the same way. So a player whose fire is the mouse has no *key* for
// firing at all, and a controller that only ever sends keys cannot fire however
// it is bound -- which is exactly the shape of "I mapped fire to B and that does
// not work either".
//
// The engine's own setting says which button, and the page sends it alongside the
// key. Nothing else here can be a mouse button: there is no `mouseb_speed`, so Run
// is a key and only a key.
//
const ACTION_MOUSEB = {
  fire:      ['mouseb_fire', 0],
  strafemod: ['mouseb_strafe', 1],
};

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

//
// A binding names an input, not just a button.
//
// `b7` is button 7; `a5+` is axis 5 pushed positive, `a2-` negative. Axes are
// bindable because a trigger is not reliably a button: the standard mapping puts
// LT and RT at buttons 6 and 7, but plenty of pads and browsers report them as
// analog axes instead, and then nothing is at 6 and 7 at all. Every other
// control on such a pad works, which is exactly what "triggers don't work,
// everything else does" looks like from the outside.
//
// Rather than guess which axis a given pad uses, the panel binds whatever moves.
//
export const DEFAULT_BINDINGS = {
  b0: 'act',        b1: 'back_out',   b2: 'weapon3',   b3: 'weapon2',
  b4: 'weapon4',    b5: 'weapon5',    b6: 'run',       b7: 'fire',
  b8: 'map',        b9: 'menu',       b10: 'weapon1',  b11: 'weapon6',
  b12: 'forward',   b13: 'back',      b14: 'turnleft', b15: 'turnright',
};

// How far a trigger has to be squeezed, and an axis pushed, to count. A trigger
// reporting only `value` never sets `pressed` on some pads, and one that tops
// out around 0.5 would never have passed the old half-way mark.
const TRIGGER_AT = 0.3;
const AXIS_AT    = 0.5;

export function inputName(token) {
  const m = /^b(\d+)$/.exec(token);
  if (m) return BUTTON_NAMES[Number(m[1])] || 'Button ' + m[1];
  const a = /^a(\d+)([+-])$/.exec(token);
  if (a) return 'Axis ' + a[1] + ' ' + a[2];
  return String(token);
}

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

//
// Is the input this token names currently on?
//
// Buttons take `pressed` or an analog `value` past the trigger mark, since a
// trigger may report one and not the other. An axis binding is directional: the
// panel records which way the axis moved when it was bound, so a trigger that
// rests at -1 and travels to +1 binds as `+` and never reads as held at rest.
//
export function inputDown(pad, token) {
  if (!pad) return false;
  const b = /^b(\d+)$/.exec(token);
  if (b) {
    const btn = pad.buttons[Number(b[1])];
    return !!btn && (btn.pressed || btn.value > TRIGGER_AT);
  }
  const a = /^a(\d+)([+-])$/.exec(token);
  if (a) {
    const v = pad.axes[Number(a[1])];
    if (typeof v !== 'number') return false;
    return a[2] === '+' ? v >= AXIS_AT : v <= -AXIS_AT;
  }
  return false;
}

export class DoomGamepad {
  //
  // `press` and `release` are handed a keysym and the DOM code name that goes
  // with it; `turn` a number of mouse pixels to move sideways. The page
  // supplies all three, so this file never touches the RFB connection and can
  // be exercised without one.
  //
  constructor({ press, release, turn, mouse, onChange } = {}) {
    this._press = press || (() => {});
    this._release = release || (() => {});
    this._turn = turn || (() => {});
    // Which mouse buttons the pad is holding, as a bitmask the page sends on.
    this._mouse = mouse || (() => {});
    this._mouseMask = 0;
    this._onChange = onChange || (() => {});

    this.bindings = { ...DEFAULT_BINDINGS };
    this.settings = { ...DEFAULT_SETTINGS };
    this.keys = {};             // actionId -> [[keysym, code], ...], learned
    this.engineKeys = {};       // ... and the same, read from the engine's config
    this.unknownKeys = {};      // settings that could not be turned into a keysym
    this.engineMouse = {};      // mouse button each action may also press
    this.engineRaw = {};        // the engine's own number, for the panel
    this.engineSeen = false;

    // Actions the person deliberately took the button off, so that filling in
    // the defaults below does not hand it back every time the page loads.
    this._cleared = new Set();

    // ... and what filling in did, for the log.
    this.filledIn = [];

    this._load();
    this._fillDefaults();

    // Keysyms currently held down on the engine's behalf, against the code
    // name each was pressed with. Keyed by keysym rather than counted: two
    // buttons bound to the same action both press one key, and letting go of
    // one of them must not release it while the other is still held.
    this._held = new Map();

    // The last few keys put on the wire, for the panel. Kept because the panel
    // can only be read when the game is not being played, so "what did it send
    // while I was playing" is a question that can only be answered afterwards.
    this._log = [];
    this._logSeq = 0;

    this._rest = null;      // where each axis sits when nothing is touching it
    this._restFor = null;   // ... and which pad that was measured on
    this._autoDone = null;  // pad whose triggers have already been looked at
    this._autoBound = {};   // actions bound by trigger detection, for the panel

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
    this._learnRest(pad);
    this._autoBindTriggers(pad);
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
    let wantMouse = 0;          // ... and the mouse buttons, as a mask
    const add = id => {
      const keys = this.keysFor(id);
      for (const [sym, code] of keys) want.set(sym, code);
      //
      // The mouse button is a fallback, not an addition.
      //
      // Only where the engine has no key for this action at all. Sending it as
      // well as a working key would mean holding fire also holds the left mouse
      // button, and in a menu the engine reads that as Return -- so a player with
      // a perfectly good fire key would find menus confirming themselves.
      //
      if (!keys.length) {
        const mb = this.mouseFor(id);
        if (mb !== null) wantMouse |= 1 << mb;
      }
    };

    // Whatever each binding names -- a button, or an axis pushed one way.
    for (const [token, id] of Object.entries(this.bindings)) {
      if (id && inputDown(pad, token)) add(id);
    }

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
    this._applyMouse(wantMouse);

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

  //
  // Where the axes sit at rest, measured once per pad.
  //
  // A stick rests near zero. A trigger reported as an axis very often rests at
  // one extreme and travels to the other, and that difference is what tells the
  // two apart without knowing anything about the particular pad.
  //
  _learnRest(pad) {
    if (!pad) { this._rest = null; this._restFor = null; this._autoDone = null; return; }
    if (this._restFor === pad.id && this._rest) return;
    this._restFor = pad.id;
    this._rest = Array.from(pad.axes, v => (typeof v === 'number' ? v : 0));
  }

  //
  // Bind the triggers when the layout says they are buttons and the pad has none.
  //
  // The standard layout puts LT and RT at buttons 6 and 7. A pad reporting them
  // as axes has no button 6 or 7 at all, so those two actions point at nothing
  // that exists and nothing happens -- with every other control working, which
  // is what makes it look so strange.
  //
  // An axis resting at an extreme is taken to be a trigger. That is an
  // observation rather than a guess at indices: a stick resting at ±1 is a broken
  // stick. The lower-numbered one becomes Run and the next Fire, matching the
  // buttons they stand in for. This never replaces a binding the pad can satisfy.
  //
  _autoBindTriggers(pad) {
    if (!pad || !this._rest) return;
    if (this._autoDone === pad.id) return;
    this._autoDone = pad.id;

    const triggers = [];
    this._rest.forEach((r, i) => {
      if (Math.abs(r) >= 0.8) triggers.push({ i, dir: r < 0 ? '+' : '-' });
    });
    if (!triggers.length) return;

    //
    // Added beside the button, not instead of it.
    //
    // 1.10.60 only did this where the button index did not exist on the pad,
    // which misses the layout that actually causes the trouble: a pad reporting
    // eleven buttons where 6 and 7 are View and Menu rather than the triggers.
    // Nothing looks missing there, so nothing was bound, and the triggers stayed
    // dead. An action can have more than one input, so both are bound and
    // whichever the pad really uses works. If 6 and 7 are the triggers after
    // all, they go on working exactly as before.
    //
    const hasAxis = (id) =>
      Object.entries(this.bindings).some(([t, a]) => a === id && /^a/.test(t));

    const want = ['run', 'fire'].filter(id => !hasAxis(id));
    for (let n = 0; n < want.length && n < triggers.length; n++) {
      const t = triggers[n];
      this.bind('a' + t.i + t.dir, want[n]);
      this._autoBound[want[n]] = true;
    }
  }

  wasAutoBound(actionId) {
    return !!this._autoBound[actionId];
  }

  //
  // Actions that cannot work as things stand, for the page to say so plainly.
  //
  // Two ways an action goes dead without looking wrong: its key is a value this
  // page cannot express, or its input is a button the connected pad does not
  // have. Both were silent for several releases.
  //
  troubled() {
    const pad = this.pad();
    const out = [];
    for (const a of ACTIONS) {
      // No key it can press, and no mouse button either: nothing can make this
      // action happen, whatever it is bound to.
      if (!this.keysFor(a.id).length && this.mouseFor(a.id) === null) {
        out.push({ id: a.id, label: a.label, why: 'key' });
        continue;
      }
      // Only a problem if *none* of its inputs exist on this pad.
      const inputs = this.inputsFor(a.id);
      if (!pad || !inputs.length) continue;
      const anyReal = inputs.some(t => {
        const m = /^b(\d+)$/.exec(t);
        return m ? Number(m[1]) < pad.buttons.length : true;
      });
      if (!anyReal) out.push({ id: a.id, label: a.label, why: 'button' });
    }
    return out;
  }

  //
  // What every action will actually do, resolved.
  //
  // One line per action: which inputs press it, which keys it will send, where
  // that key came from, and the engine's own number. This is the block that
  // answers "why does fire do nothing" without anybody having to guess, and it
  // is the thing that should have been written first.
  //
  plan() {
    const out = [];
    for (const a of ACTIONS) {
      const inputs = this.inputsFor(a.id);
      const keys = this.keysFor(a.id);
      const mb = keys.length ? null : this.mouseFor(a.id);
      const unknown = this.unknownKeyFor(a.id);
      const raw = this.rawFor(a.id);

      let sends;
      if (keys.length) sends = keys.map(([k]) => keyName(k)).join('+');
      else if (mb !== null) sends = 'mouse' + (mb + 1);
      else sends = 'NOTHING';

      out.push(a.id
        + ' in=' + (inputs.length ? inputs.join(',') : '-')
        + ' sends=' + sends
        + ' src=' + this.keySource(a.id)
        + (raw !== null ? ' doomrc=' + raw : '')
        + (unknown !== null ? ' unusable=' + unknown : ''));
    }
    return out;
  }

  //
  // A one-line description of the pad, for the container's log.
  //
  // Which pad the browser admits to, what it calls itself, how many buttons it
  // claims and where its axes rest with nothing touched -- the four questions
  // every controller report so far has turned on, and none of them answerable
  // from inside the container. The page sends this to doom-wsproxy, which
  // prints it where the entrypoint can lift it into the log.
  //
  padReport() {
    const pad = this.pad();
    if (!pad) return null;
    return {
      id: pad.id,
      mapping: pad.mapping || 'none',
      buttons: String(pad.buttons.length),
      axes: (this._rest || []).map(v => v.toFixed(2)).join(','),
    };
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

  _applyMouse(mask) {
    if (mask === this._mouseMask) return;
    this._mouseMask = mask;
    this._mouse(mask);
  }

  _note(keysym, down) {
    // A sequence number as well as the entry: the log itself is a ring that
    // drops its oldest, so a reader cannot tell what is new by its length.
    this._logSeq++;
    this._log.push({ keysym, down, seq: this._logSeq,
                     at: Math.round(performance.now()) });
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
    if (this._mouseMask) { this._mouseMask = 0; this._mouse(0); }
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
      // Every input currently on, named the way a binding names it, so an axis
      // that is really a trigger shows up as one.
      down: (() => {
        const on = [];
        if (pad) {
          pad.buttons.forEach((b, i) => {
            if (b && (b.pressed || b.value > TRIGGER_AT)) on.push('b' + i);
          });
          pad.axes.forEach((v, i) => {
            if (typeof v === 'number' && Math.abs(v) >= AXIS_AT)
              on.push('a' + i + (v > 0 ? '+' : '-'));
          });
        }
        return on;
      })(),
      buttonsPresent: pad ? pad.buttons.length : 0,
      log: this._log.slice(-14),
      logSeq: this._logSeq,
      mouseMask: this._mouseMask,
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
      // binds itself the instant capture starts. Axes are remembered where they
      // are resting, so an axis is bound by moving it rather than by holding it
      // -- which is the only way to bind a trigger that rests at -1.
      const pad = this.pad();
      const ignore = new Set();
      const rest = [];
      if (pad) {
        pad.buttons.forEach((b, i) => {
          if (b && (b.pressed || b.value > TRIGGER_AT)) ignore.add('b' + i);
        });
        pad.axes.forEach((v, i) => { rest[i] = typeof v === 'number' ? v : 0; });
      }
      this._capture = { resolve, ignore, rest };
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
      const down = b && (b.pressed || b.value > TRIGGER_AT);
      if (!down) { cap.ignore.delete('b' + i); continue; }
      if (cap.ignore.has('b' + i)) continue;
      this._capture = null;
      cap.resolve('b' + i);
      return;
    }

    // An axis counts once it has travelled well clear of where it was resting
    // when binding started, which is what separates a squeezed trigger from a
    // stick that never quite centres.
    for (let i = 0; i < pad.axes.length; i++) {
      const v = pad.axes[i];
      if (typeof v !== 'number') continue;
      const moved = v - (cap.rest[i] || 0);
      if (Math.abs(moved) < 0.6) continue;
      this._capture = null;
      cap.resolve('a' + i + (moved > 0 ? '+' : '-'));
      return;
    }
  }

  //
  // Bindings.
  //
  bind(token, actionId) {
    if (actionId === null || actionId === undefined) delete this.bindings[token];
    else this.bindings[token] = actionId;
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

    // The engine's setting for this action is known and cannot be expressed.
    // Send nothing rather than the built-in default: the default is a key this
    // engine is not listening for, and pressing it might well be bound to
    // something else entirely. The panel says so and offers Key.
    if (this.unknownKeys && this.unknownKeys[actionId] !== undefined) return [];

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
    const unknown = {};
    const mice = {};
    const raw = {};
    if (config && typeof config === 'object') {
      for (const [id, [setting, fallback]] of Object.entries(ACTION_MOUSEB)) {
        const n = Number(config[setting]);
        mice[id] = Number.isInteger(n) && n >= 0 && n <= 2 ? n : fallback;
      }
      for (const [id, setting] of Object.entries(ACTION_DOOMRC)) {
        const value = config[setting];
        if (value === undefined || value === null) continue;
        raw[id] = value;
        const pair = doomKeyToX(value);
        if (!pair) {
          // Nothing sensible to send. Recorded rather than ignored, so the panel
          // can say "the game uses key 164, which this page cannot express"
          // instead of quietly pressing the default at an engine that is
          // listening for something else.
          unknown[id] = value;
          continue;
        }
        out[id] = oneKey(pair);
      }
    }
    this.engineKeys = out;
    this.unknownKeys = unknown;
    this.engineMouse = mice;
    this.engineRaw = raw;
    this._onChange();
  }

  // The mouse button this action should also press, or null.
  mouseFor(actionId) {
    if (!(actionId in ACTION_MOUSEB)) return null;
    const m = this.engineMouse && this.engineMouse[actionId];
    return m === undefined || m === null ? ACTION_MOUSEB[actionId][1] : m;
  }

  // The engine's raw setting for this action, for the panel to show alongside.
  rawFor(actionId) {
    if (this.engineRaw && actionId in this.engineRaw) return this.engineRaw[actionId];
    return null;
  }

  // The engine's setting for this action, where it could not be turned into a
  // keysym at all. Null when there is no such problem.
  unknownKeyFor(actionId) {
    // Not `|| null`: the value is very often 0 -- a control the engine has no
    // key for at all -- and 0 is falsy, so the warning was suppressed for
    // exactly the case that most needs it.
    if (!this.unknownKeys || !(actionId in this.unknownKeys)) return null;
    return this.unknownKeys[actionId];
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
    this.keys[actionId] = oneKey([keysym, code || null]);
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
  bindAction(actionId, token) {
    for (const [t, id] of Object.entries(this.bindings))
      if (id === actionId) delete this.bindings[t];
    if (token !== null && token !== undefined) {
      this.bindings[token] = actionId;
      this._cleared.delete(actionId);
    } else {
      // Cleared on purpose. Written down, or _fillDefaults would hand the
      // default button straight back on the next page load.
      this._cleared.add(actionId);
    }
    this._save();
    this._onChange();
  }

  // Which input currently presses this action, if any.
  inputFor(actionId) {
    for (const [t, id] of Object.entries(this.bindings))
      if (id === actionId) return t;
    return null;
  }

  // All of them, since an action may have a button and a trigger axis both.
  inputsFor(actionId) {
    return Object.entries(this.bindings)
      .filter(([, id]) => id === actionId)
      .map(([t]) => t);
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
    this._cleared = new Set();
    this.filledIn = [];
    this._save();
    this._onChange();
  }

  //
  // Give an action its default button back when it has none at all.
  //
  // What is saved is the whole binding set, and loading it used to replace the
  // defaults outright. So a layout saved by an earlier build kept working and
  // every action added after it was saved had no button for ever -- with no way
  // to find that out and no way back except Reset to defaults. A log from a real
  // pad had seven weapons, Escape and both strafes reading `in=-`, which is
  // exactly what it looks like: a set saved before those actions existed.
  //
  // Only ever fills a gap. An action that already has a button keeps it, and a
  // default button somebody has since put another action on is left alone --
  // so a rebind is never undone, and nothing is ever bound twice.
  //
  _fillDefaults() {
    this.filledIn = [];

    const used = new Set(Object.values(this.bindings));

    for (const [token, id] of Object.entries(DEFAULT_BINDINGS)) {
      if (token in this.bindings) continue;   // that button is spoken for
      if (used.has(id)) continue;             // that action already has one
      if (this._cleared.has(id)) continue;    // ... and this one was cleared on purpose

      this.bindings[token] = id;
      used.add(id);
      this.filledIn.push(id + '=' + token);
    }

    if (this.filledIn.length) this._save();
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
        cleared: [...this._cleared],
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
      for (const [key, id] of Object.entries(saved.bindings)) {
        if (!ACTION_BY_ID.has(id)) continue;
        // Bindings were bare button numbers before axes could be bound. Read
        // those as buttons rather than throwing away somebody's layout.
        const token = /^\d+$/.test(key) ? 'b' + key : key;
        if (/^b\d{1,2}$/.test(token) || /^a\d{1,2}[+-]$/.test(token))
          clean[token] = id;
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
    // Missing rather than empty means the blob predates the Clear button being
    // remembered at all. Then nothing was cleared on purpose, and every gap in
    // it is an action that did not exist when it was written.
    if (Array.isArray(saved.cleared))
      this._cleared = new Set(saved.cleared.filter(id => ACTION_BY_ID.has(id)));

    if (saved.settings && typeof saved.settings === 'object') {
      for (const [k, v] of Object.entries(saved.settings)) {
        if (!(k in DEFAULT_SETTINGS)) continue;
        if (typeof v !== typeof DEFAULT_SETTINGS[k]) continue;
        this.settings[k] = v;
      }
    }
  }
}
