# 9Q Browser Arena

Static browser app for playing the C++ Togyzkumalak engine through WebAssembly.

## Local setup

Install Emscripten locally, then build the Wasm artifacts:

```bash
npm run setup:emscripten
```

```bash
npm run build:wasm
```

Run the static app locally:

```bash
npm run dev
```

Open `http://localhost:3000`.

## Vercel

Commit the generated files in `web/public/wasm/`, then import the repository in Vercel.

- Build command: `npm run build`
- Output directory: `dist`

The root `vercel.json` already sets those values. No server functions are used; the engine runs in the browser in a Web Worker.
