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
      .catch(() => {
        throw new Error("Engine Wasm not found. Run npm run build:wasm before npm run build.");
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
      Module.ccall("tg_bot_move", "number", ["string", "number"], [payload.player, payload.seconds]);
      response = {
        bot: readJson(Module, "_tg_last_bot_json"),
        move: readJson(Module, "_tg_last_move_json"),
        state: readJson(Module, "_tg_state_json"),
        fen: readString(Module, "_tg_fen_string")
      };
    } else {
      throw new Error(`Unknown worker command: ${type}`);
    }

    self.postMessage({ id, ok: true, payload: response });
  } catch (error) {
    self.postMessage({
      id,
      ok: false,
      error: error && error.message
        ? error.message
        : "Engine Wasm not found. Run npm run build:wasm before npm run build."
    });
  }
});
