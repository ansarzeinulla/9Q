import { createServer } from "node:http";
import { readFile, stat } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const publicRoot = path.join(root, "public");
const port = Number(process.env.PORT || 3000);
const host = process.env.HOST || "127.0.0.1";

const mime = new Map([
  [".html", "text/html; charset=utf-8"],
  [".css", "text/css; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".wasm", "application/wasm"],
  [".json", "application/json; charset=utf-8"]
]);

function safeRelativePath(urlPath) {
  const decoded = decodeURIComponent(urlPath.split("?")[0]);
  const normalized = path.normalize(decoded).replace(/^(\.\.[/\\])+/, "");
  return normalized === "/" ? "index.html" : normalized.replace(/^[/\\]/, "");
}

const server = createServer(async (req, res) => {
  try {
    const relativePath = safeRelativePath(req.url || "/");
    let filePath = path.join(root, relativePath);
    let info = await stat(filePath).catch(() => null);
    if (!info) {
      filePath = path.join(publicRoot, relativePath);
      info = await stat(filePath).catch(() => null);
    }
    if (!info || info.isDirectory()) {
      filePath = path.join(root, "index.html");
      info = await stat(filePath);
    }

    const ext = path.extname(filePath);
    res.writeHead(200, {
      "Content-Type": mime.get(ext) || "application/octet-stream",
      "Cache-Control": ext === ".wasm" ? "no-cache" : "no-store"
    });
    res.end(await readFile(filePath));
  } catch (error) {
    res.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
    res.end("Not found");
  }
});

server.listen(port, host, () => {
  console.log(`9Q Browser Arena running at http://${host}:${port}`);
});
