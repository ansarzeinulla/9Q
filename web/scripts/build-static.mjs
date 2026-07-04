import { cp, mkdir, rm, stat } from "node:fs/promises";
import { existsSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const web = path.join(root, "web");
const out = path.join(root, "dist");
const allowMissingWasm = process.env.ALLOW_MISSING_WASM === "1";

const wasmArtifacts = [
  path.join(web, "public/wasm/togyz_engine.js"),
  path.join(web, "public/wasm/togyz_engine.wasm")
];

const missingWasm = wasmArtifacts.filter((file) => !existsSync(file));
if (missingWasm.length > 0 && !allowMissingWasm) {
  console.error("Missing Wasm artifacts:");
  for (const file of missingWasm) {
    console.error(`  - ${path.relative(root, file)}`);
  }
  console.error("");
  console.error("Run `npm run build:wasm` before `npm run build`.");
  process.exit(1);
}

await rm(out, { recursive: true, force: true });
await mkdir(out, { recursive: true });

for (const file of ["index.html", "analysis.html", "styles.css", "app.js", "engine-worker.js", "i18n.js", "togyzkumalak-pgn.js"]) {
  await cp(path.join(web, file), path.join(out, file));
}

if (existsSync(path.join(web, "public"))) {
  await cp(path.join(web, "public"), out, { recursive: true });
}

const outputStats = await stat(out);
if (!outputStats.isDirectory()) {
  throw new Error("Static build did not create dist/");
}

if (missingWasm.length > 0) {
  console.warn("Built static files without Wasm because ALLOW_MISSING_WASM=1.");
} else {
  console.log("Built static Vercel output in dist/.");
}
