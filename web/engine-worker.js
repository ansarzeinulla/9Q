// ============================================================================
// CHROME RESIZABLE ARRAYBUFFER WORKAROUND POLYFILL
// Intercepts crypto.getRandomValues to prevent Chrome from crashing when 
// passed views into resizable WebAssembly memory bounds.
// ============================================================================
if (self.crypto && typeof self.crypto.getRandomValues === "function") {
  const originalGetRandomValues = self.crypto.getRandomValues.bind(self.crypto);
  self.crypto.getRandomValues = function (array) {
    try {
      return originalGetRandomValues(array);
    } catch (err) {
      // If Chrome throws because the array view is backed by resizable memory
      if (err instanceof TypeError && array && array.buffer) {
        // 1) Create a temporary, stable, non-resizable typed array of the exact same size
        const temp = new Uint8Array(array.byteLength);
        
        // 2) Safely fill the temporary buffer with cryptographically strong values
        originalGetRandomValues(temp);
        
        // 3) Copy the values back into the resizable WebAssembly memory view
        const destView = new Uint8Array(array.buffer, array.byteOffset, array.byteLength);
        destView.set(temp);
        
        return array;
      }
      throw err;
    }
  };
}
// ============================================================================

let modulePromise = null;

async function loadEngine() {
  if (!modulePromise) {
    modulePromise = import("/wasm/togyz_engine.js")
      .then(({ default: createEngineModule }) =>
        createEngineModule({
          locateFile(path) {
            return `/wasm/${path}`;
          }
        })
      )
      .catch((err) => {
        console.error("Wasm Engine Dynamic Import Failed:", err);
        throw err;
      });
  }
  return modulePromise;
}

function readJson(Module, fnName) {
  const pointer = Module[fnName]();
  return JSON.parse(Module.UTF8ToString(pointer));
}

function readString(Module, fnName) {
  const pointer = Module[fnName]();
  return Module.UTF8ToString(pointer);
}

self.addEventListener("message", async (event) => {
  const { id, type, payload } = event.data;
  try {
    const Module = await loadEngine();
    let response;

    if (type === "init") {
      const isMobile = /Mobi|Android|iPhone|iPad|iPod/i.test(
        typeof navigator !== "undefined" ? navigator.userAgent : ""
      );
      // TT size: explicit setting wins; otherwise size to the device.
      // navigator.deviceMemory is in GB (Chrome/Edge/Android); absent on
      // Safari/iOS/Firefox, where we fall back to platform defaults.
      let ttMb = Number(payload?.ttMb) || 0;
      if (!ttMb) {
        const deviceGb = typeof navigator !== "undefined" && navigator.deviceMemory
          ? navigator.deviceMemory
          : 0;
        if (deviceGb) {
          ttMb = Math.min(512, Math.max(64, Math.floor((deviceGb * 1024) / 8)));
          if (isMobile) ttMb = Math.min(ttMb, 128);
        } else {
          ttMb = isMobile ? 128 : 256;
        }
      }
      ttMb = Math.min(1024, Math.max(16, ttMb));
      Module.ccall("tg_init_engine", null, ["number"], [ttMb]);
      self.__ttMb = ttMb;
      response = {
        version: readString(Module, "_tg_version"),
        ttMb,
        state: readJson(Module, "_tg_state_json"),
        fen: readString(Module, "_tg_fen_string")
      };
    } else if (type === "reset") {
      Module._tg_reset();
      response = {
        state: readJson(Module, "_tg_state_json"),
        fen: readString(Module, "_tg_fen_string")
      };
    } else if (type === "getFen") {
      response = { fen: readString(Module, "_tg_fen_string") };
    } else if (type === "setFen") {
      const accepted = Module.ccall("tg_set_fen", "number", ["string"], [payload.fen || ""]) === 1;
      if (!accepted) {
        const code = readString(Module, "_tg_last_error") || "fen_invalid";
        throw new Error(code);
      }
      response = {
        state: readJson(Module, "_tg_state_json"),
        fen: readString(Module, "_tg_fen_string")
      };
    } else if (type === "humanMove") {
      const accepted = Module._tg_make_move(payload.pit) === 1;
      response = {
        accepted,
        move: accepted ? readJson(Module, "_tg_last_move_json") : null,
        state: readJson(Module, "_tg_state_json"),
        fen: readString(Module, "_tg_fen_string")
      };
    } else if (type === "botMove") {
      const move = Module.ccall("tg_bot_move", "number", ["string", "number"], [payload.player, payload.seconds]);
      const stats = readJson(Module, "_tg_get_last_search_stats");
      response = {
        bot: readJson(Module, "_tg_last_bot_json"),
        move: readJson(Module, "_tg_last_move_json"),
        stats,
        state: readJson(Module, "_tg_state_json"),
        fen: readString(Module, "_tg_fen_string")
      };
    } else if (type === "analyzePosition") {
      const state = readJson(Module, "_tg_state_json");
      const legalPits = Array.isArray(state.legal) && state.legal.length > 0
        ? state.legal
        : [0, 1, 2, 3, 4, 5, 6, 7, 8];
      const evaluations = [];
      
      for (const pit of legalPits) {
        const currentFen = readString(Module, "_tg_fen_string");
        const accepted = Module._tg_make_move(pit) === 1;
        if (accepted) {
          const move = readJson(Module, "_tg_last_move_json");
          const depthEvals = [];
          for (let depth = 1; depth <= 4; depth++) {
            Module.ccall("tg_bot_move", "number", ["string", "number"], ["dagv2", 0.1]);
            const bot = readJson(Module, "_tg_last_bot_json");
            let score = 0;
            if (typeof bot.score === 'number') {
              score = bot.score;
            } else if (typeof bot.eval === 'number') {
              score = bot.eval;
            } else if (typeof bot.value === 'number') {
              score = bot.value;
            }
            depthEvals.push({
              depth: depth,
              score: score
            });
            Module.ccall("tg_set_fen", "number", ["string"], [currentFen]);
            Module._tg_make_move(pit);
          }
          evaluations.push({
            pit: pit,
            move: move,
            depthEvals: depthEvals
          });
        }
        Module.ccall("tg_set_fen", "number", ["string"], [currentFen]);
      }
      
      response = {
        evaluations: evaluations,
        state: readJson(Module, "_tg_state_json"),
        fen: readString(Module, "_tg_fen_string")
      };
    } else {
      throw new Error(`Unknown worker command: ${type}`);
    }

    self.postMessage({ id, ok: true, payload: response });
  } catch (error) {
    console.error("Worker Listener Error Caught:", error);
    self.postMessage({
      id,
      ok: false,
      error: error && error.message ? error.message : "Engine initialization error"
    });
  }
});