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
        // DIAGNOSTIC 1: Log the exact error why dynamic import or instantiation failed
        console.error("Wasm Engine Dynamic Import Failed:", err);
        throw err; // Throw the actual error so it propagates to the UI
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
      Module.ccall("tg_init_engine", null, ["number"], [payload?.ttMb || (isMobile ? 64 : 256)]);
      response = {
        version: readString(Module, "_tg_version"),
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
            Module.ccall("tg_bot_move", "number", ["string", "number"], ["dag", 0.1]);
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
    // DIAGNOSTIC 2: Log the exact error to the console
    console.error("Worker Listener Error Caught:", error);
    self.postMessage({
      id,
      ok: false,
      error: error && error.message ? error.message : "Engine initialization error"
    });
  }
});