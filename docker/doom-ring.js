//
// The buffer between the container's clock and this machine's.
//
// The container sends at a steady real-time rate and the sound card consumes
// at its own, which are never quite the same. This absorbs the difference --
// padding when it runs dry, dropping the oldest frame when it runs long --
// and resamples to whatever rate the AudioContext turned out to be. Handing
// the browser a queue of scheduled buffers instead would drift until it
// either stuttered or fell behind.
//
// Assigned onto globalThis rather than exported, because this file is loaded
// two ways: as an ordinary script by the page, and as a worklet module into
// the AudioWorkletGlobalScope, which does not share the page's. A module
// export would only reach one of them.
//
globalThis.DoomRing = class DoomRing {
  constructor(srcRate, outRate, targetMs, maxMs) {
    this.ratio  = srcRate / outRate;        // source frames per output frame
    this.max    = Math.ceil(maxMs * srcRate / 1000);
    this.target = Math.ceil(targetMs * srcRate / 1000);
    this.left   = new Float32Array(this.max * 2);
    this.right  = new Float32Array(this.max * 2);
    this.size   = this.left.length;
    this.head   = 0;     // next write position
    this.tail   = 0;     // read position, fractional
    this.count  = 0;     // frames available
    this.started = false;
  }

  // Interleaved stereo floats, as they came off the socket.
  push(frames) {
    const n = frames.length >> 1;

    for (let i = 0; i < n; i++) {
      if (this.count >= this.max) {
        // Running long: drop the oldest frame rather than refuse the newest,
        // so the delay shrinks instead of the sound tearing at the end.
        this.tail = (this.tail + 1) % this.size;
        this.count--;
      }

      this.left[this.head]  = frames[i*2];
      this.right[this.head] = frames[i*2 + 1];
      this.head = (this.head + 1) % this.size;
      this.count++;
    }
  }

  read(l, r) {
    const n = l.length;

    // Wait for a cushion before starting, or the first note underruns
    // immediately and clicks.
    if (!this.started) {
      if (this.count < this.target) {
        l.fill(0);
        if (r !== l) r.fill(0);
        return;
      }
      this.started = true;
    }

    for (let i = 0; i < n; i++) {
      if (this.count < 2) {
        // Dry. Silence is the honest answer; repeating the last frame buzzes.
        l[i] = 0;
        if (r !== l) r[i] = 0;
        this.started = false;
        continue;
      }

      const idx  = Math.floor(this.tail);
      const frac = this.tail - idx;
      const a = idx % this.size;
      const b = (idx + 1) % this.size;

      l[i] = this.left[a] + (this.left[b] - this.left[a]) * frac;
      if (r !== l) r[i] = this.right[a] + (this.right[b] - this.right[a]) * frac;

      this.tail += this.ratio;
      const used = Math.floor(this.tail) - idx;
      if (used) this.count -= used;
      if (this.tail >= this.size) this.tail -= this.size;
    }
  }
};
