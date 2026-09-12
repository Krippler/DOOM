//
// The sound the container sends, played at the rate this machine wants.
//
// Loaded as a file rather than built from a blob: URL. Both are legal, and
// the blob version worked everywhere it was tried, but a worklet module that
// fails to load takes the sound with it and says nothing, and a plain URL is
// the version with fewer ways to be refused.
//
// The container sends at a steady real-time rate and this machine consumes at
// its own, which are never quite the same. The ring buffer in here is what
// absorbs that -- padding when it runs dry, dropping the oldest frame when it
// runs long -- where a queue of scheduled buffers would drift until it either
// stuttered or fell behind.
//
class DoomAudio extends AudioWorkletProcessor {
  constructor(options) {
    super();
    const o = options.processorOptions;
    this.ratio  = o.srcRate / sampleRate;   // source frames per output frame
    this.max    = Math.ceil(o.maxMs * o.srcRate / 1000);
    this.target = Math.ceil(o.targetMs * o.srcRate / 1000);
    this.left   = new Float32Array(this.max * 2);
    this.right  = new Float32Array(this.max * 2);
    this.size   = this.left.length;
    this.head   = 0;     // next write position
    this.tail   = 0;     // read position, fractional
    this.count  = 0;     // frames available
    this.started = false;
    this.port.onmessage = (e) => this.push(e.data);
  }

  push(frames) {
    // frames: interleaved Float32, left/right.
    const n = frames.length >> 1;
    for (let i = 0; i < n; i++) {
      if (this.count >= this.max) {
        // Running long: drop the oldest frame rather than the newest, so the
        // delay shrinks instead of the sound tearing at the end.
        this.tail = (this.tail + 1) % this.size;
        this.count--;
      }
      this.left[this.head]  = frames[i*2];
      this.right[this.head] = frames[i*2 + 1];
      this.head = (this.head + 1) % this.size;
      this.count++;
    }
  }

  process(inputs, outputs) {
    const out = outputs[0];
    const l = out[0];
    const r = out.length > 1 ? out[1] : out[0];
    const n = l.length;

    // Wait for a cushion before starting, or the first note underruns
    // immediately and clicks.
    if (!this.started) {
      if (this.count < this.target) { l.fill(0); if (r !== l) r.fill(0); return true; }
      this.started = true;
    }

    for (let i = 0; i < n; i++) {
      if (this.count < 2) {
        // Dry. Silence is the honest answer; repeating the last frame buzzes.
        l[i] = 0; if (r !== l) r[i] = 0;
        this.started = false;
        continue;
      }

      const idx  = Math.floor(this.tail);
      const frac = this.tail - idx;
      const a = idx % this.size;
      const b = (idx + 1) % this.size;

      l[i] = this.left[a]  + (this.left[b]  - this.left[a])  * frac;
      if (r !== l) r[i] = this.right[a] + (this.right[b] - this.right[a]) * frac;

      const step = this.ratio;
      this.tail += step;
      const used = Math.floor(this.tail) - idx;
      if (used) { this.count -= used; }
      if (this.tail >= this.size) this.tail -= this.size;
    }

    return true;
  }
}
registerProcessor('doom-audio', DoomAudio);
