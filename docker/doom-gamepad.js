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
const K = KeyTable;

export const ACTIONS = [
  { id: 'fire',        label: 'Fire',              keys: [K.XK_Control_L] },
  { id: 'act',         label: 'Open / use',        keys: [K.XK_space, K.XK_Return],
    note: 'and confirms in menus' },
  { id: 'run',         label: 'Run',               keys: [K.XK_Shift_L] },
  { id: 'strafemod',   label: 'Strafe modifier',   keys: [K.XK_Alt_L],
    note: 'unbound: the left stick already sidesteps' },
  { id: 'forward',     label: 'Forward',           keys: [K.XK_Up] },
  { id: 'back',        label: 'Back',              keys: [K.XK_Down] },
  { id: 'turnleft',    label: 'Turn left',         keys: [K.XK_Left] },
  { id: 'turnright',   label: 'Turn right',        keys: [K.XK_Right] },
  { id: 'strafeleft',  label: 'Sidestep left',     keys: [K.XK_comma],
    note: 'unbound: the left stick does this' },
  { id: 'straferight', label: 'Sidestep right',    keys: [K.XK_period],
    note: 'unbound: the left stick does this' },
  { id: 'menu',        label: 'Game menu',         keys: [K.XK_grave] },
  { id: 'map',         label: 'Automap',           keys: [K.XK_Tab] },
  { id: 'back_out',    label: 'Back out of menus', keys: [K.XK_Escape],
    note: 'the game’s Escape, not this page’s' },
  { id: 'weapon1',     label: 'Fist / chainsaw',   keys: [K.XK_1] },
  { id: 'weapon2',     label: 'Pistol',            keys: [K.XK_2] },
  { id: 'weapon3',     label: 'Shotgun',           keys: [K.XK_3] },
  { id: 'weapon4',     label: 'Chaingun',          keys: [K.XK_4] },
  { id: 'weapon5',     label: 'Rocket launcher',   keys: [K.XK_5] },
  { id: 'weapon6',     label: 'Plasma rifle',      keys: [K.XK_6] },
  { id: 'weapon7',     label: 'BFG9000',           keys: [K.XK_7],
    note: 'unbound: one weapon more than there are buttons' },
];

const ACTION_BY_ID = new Map(ACTIONS.map(a => [a.id, a]));

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
  // `press` and `release` are handed a keysym; `turn` a number of mouse pixels
  // to move sideways. The page supplies all three, so this file never touches
  // the RFB connection and can be exercised without one.
  //
  constructor({ press, release, turn, onChange } = {}) {
    this._press = press || (() => {});
    this._release = release || (() => {});
    this._turn = turn || (() => {});
    this._onChange = onChange || (() => {});

    this.bindings = { ...DEFAULT_BINDINGS };
    this.settings = { ...DEFAULT_SETTINGS };
    this._load();

    // Keysyms currently held down on the engine's behalf. A set rather than a
    // count: two buttons bound to the same action both press one key, and
    // letting go of one of them must not release it while the other is held.
    this._held = new Set();

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

    const want = new Set();
    const add = id => {
      const a = ACTION_BY_ID.get(id);
      if (a) for (const k of a.keys) want.add(k);
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
    for (const k of this._held) {
      if (!want.has(k)) {
        this._held.delete(k);
        this._release(k);
      }
    }
    for (const k of want) {
      if (!this._held.has(k)) {
        this._held.add(k);
        this._press(k);
      }
    }
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
    for (const k of this._held) this._release(k);
    this._held.clear();
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
        bindings: this.bindings, settings: this.settings,
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
    if (saved.settings && typeof saved.settings === 'object') {
      for (const [k, v] of Object.entries(saved.settings)) {
        if (!(k in DEFAULT_SETTINGS)) continue;
        if (typeof v !== typeof DEFAULT_SETTINGS[k]) continue;
        this.settings[k] = v;
      }
    }
  }
}
