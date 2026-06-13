// Togyzkumalak PGN Viewer

const INITIAL_FEN = "9,9,9,9,9,9,9,9,9/9,9,9,9,9,9,9,9,9 0,0 -,- w 0";

// State
let currentPosition = null;
let moveHistory = [];
let currentMoveIndex = -1;
let isRotated = false;
let allPositions = [];
let allFens = [INITIAL_FEN];
let engineReady = false;

// DOM elements
const els = {
  board: document.getElementById('board'),
  moveHistory: document.getElementById('move-history'),
  rotateBoard: document.getElementById('rotate-board'),
  navFirst: document.getElementById('nav-first'),
  navPrev: document.getElementById('nav-prev'),
  navNext: document.getElementById('nav-next'),
  navLast: document.getElementById('nav-last'),
  turnLabel: document.getElementById('turn-label'),
  whiteKazan: document.getElementById('white-kazan'),
  blackKazan: document.getElementById('black-kazan')
};

// Engine client (similar to app.js)
class EngineClient {
  constructor() {
    this.nextId = 1;
    this.pending = new Map();
    this.worker = new Worker("/engine-worker.js", { type: "module" });
    this.worker.addEventListener("message", (event) => {
      const { id, ok, payload, error } = event.data;
      const request = this.pending.get(id);
      if (!request) return;
      this.pending.delete(id);
      ok ? request.resolve(payload) : request.reject(new Error(error || "engine_error"));
    });
    this.worker.addEventListener("error", (event) => {
      for (const request of this.pending.values()) {
        request.reject(new Error(event.message || "engine_error"));
      }
      this.pending.clear();
    });
  }

  send(type, payload = {}) {
    const id = this.nextId++;
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
      this.worker.postMessage({ id, type, payload });
    });
  }
}

const engine = new EngineClient();

// Parse 9Q-PGN moves (format: "19 59 22+ 72x" etc.)
function parsePgn(pgn) {
  const moves = [];
  // Remove move numbers and result
  const cleanPgn = pgn
    .replace(/\d+\./g, '')
    .replace(/1-0|0-1|1\/2-1\/2|\*/g, '')
    .trim();
  
  // Split by whitespace
  const tokens = cleanPgn.split(/\s+/);
  
  for (const token of tokens) {
    if (token && token.length >= 2) {
      moves.push(token);
    }
  }
  
  return moves;
}

// Parse a single move (format: "19", "59", "22+", "72x")
function parseMove(moveStr) {
  const move = moveStr.trim();
  if (move.length < 2) return null;
  
  const from = parseInt(move[0]) - 1; // Convert to 0-indexed
  const to = parseInt(move[1]) - 1;
  
  if (isNaN(from) || isNaN(to) || from < 0 || from > 8 || to < 0 || to > 8) {
    return null;
  }
  
  const capture = move.includes('+');
  const tuzdyk = move.includes('x');
  
  return { from, to, capture, tuzdyk, notation: move };
}

// Render stone field (from app.js)
function renderStoneField(count) {
  const visible = Math.min(count, 18);
  let html = "";
  for (let i = 0; i < visible; i++) {
    html += '<span class="stone"></span>';
  }
  return html;
}

// Render pit (from app.js)
function renderPit(side, pit, count, displayNumber, blocked) {
  const classes = ["pit", side, blocked ? "tuzdyk-cell" : ""].filter(Boolean).join(" ");
  const label = blocked
    ? `${side} pit ${displayNumber}, tuzdyk`
    : `${side} pit ${displayNumber}, ${count} stones`;

  return `
    <div class="${classes}" aria-label="${label}">
      <span class="pit-label">${displayNumber}</span>
      <span class="stone-field" aria-hidden="true">${renderStoneField(count, blocked)}</span>
      <span class="pit-count">${blocked ? "" : count}</span>
    </div>
  `;
}

// Render board (from app.js)
function renderBoard() {
  if (!currentPosition) return;
  
  const blackPits = [];
  for (let pit = 8; pit >= 0; pit--) {
    blackPits.push(renderPit("black", pit, currentPosition.pits.black[pit], pit + 1, currentPosition.tuzdyks.white === pit));
  }

  const whitePits = [];
  for (let pit = 0; pit < 9; pit++) {
    whitePits.push(renderPit("white", pit, currentPosition.pits.white[pit], pit + 1, currentPosition.tuzdyks.black === pit));
  }

  if (isRotated) {
    els.board.innerHTML = `
      <div class="pit-row white-row">${whitePits.join("")}</div>
      <div class="pit-row black-row">${blackPits.join("")}</div>
    `;
  } else {
    els.board.innerHTML = `
      <div class="pit-row black-row">${blackPits.join("")}</div>
      <div class="pit-row white-row">${whitePits.join("")}</div>
    `;
  }
}

// Render move history
function renderMoveHistory() {
  const container = els.moveHistory;
  container.innerHTML = '';
  
  for (let i = 0; i < moveHistory.length; i += 2) {
    const row = document.createElement('div');
    row.className = 'move-row';
    
    const moveNum = document.createElement('span');
    moveNum.className = 'move-number';
    moveNum.textContent = `${Math.floor(i / 2) + 1}.`;
    row.appendChild(moveNum);
    
    const whiteMove = document.createElement('span');
    whiteMove.className = 'move-code';
    whiteMove.textContent = moveHistory[i] || '';
    whiteMove.dataset.index = i;
    // Highlight first move even at initial position
    if (i === currentMoveIndex || (currentMoveIndex === -1 && i === 0)) whiteMove.classList.add('active');
    whiteMove.addEventListener('click', () => goToMove(i));
    row.appendChild(whiteMove);
    
    const blackMove = document.createElement('span');
    blackMove.className = 'move-code';
    blackMove.textContent = moveHistory[i + 1] || '';
    blackMove.dataset.index = i + 1;
    if (i + 1 === currentMoveIndex) blackMove.classList.add('active');
    if (moveHistory[i + 1]) {
      blackMove.addEventListener('click', () => goToMove(i + 1));
    }
    row.appendChild(blackMove);
    
    container.appendChild(row);
  }
}

// Update game info
function updateGameInfo() {
  if (currentPosition) {
    els.whiteKazan.textContent = currentPosition.kazans.white;
    els.blackKazan.textContent = currentPosition.kazans.black;
    
    if (currentPosition.gameOver) {
      if (currentPosition.winner === "white") {
        els.turnLabel.textContent = "Beginner wins";
      } else if (currentPosition.winner === "black") {
        els.turnLabel.textContent = "Follower wins";
      } else {
        els.turnLabel.textContent = "Draw";
      }
    } else {
      els.turnLabel.textContent = currentPosition.toPlay === "white" ? "Beginner's turn" : "Follower's turn";
    }
  }
}

// Go to specific move
function goToMove(index) {
  if (index < -1 || index >= moveHistory.length) return;
  
  currentMoveIndex = index;
  if (index === -1) {
    currentPosition = allPositions[0];
  } else {
    currentPosition = allPositions[index + 1];
  }
  
  renderBoard();
  renderMoveHistory();
  updateGameInfo();
}

// Navigation functions
function goToFirst() {
  goToMove(-1);
}

function goToPrev() {
  goToMove(currentMoveIndex - 1);
}

function goToNext() {
  goToMove(currentMoveIndex + 1);
}

function goToLast() {
  goToMove(moveHistory.length - 1);
}

// Load PGN
async function loadPgn(pgnString) {
  if (!engineReady) {
    alert('Engine not ready yet. Please wait...');
    return;
  }
  
  const pgn = pgnString || localStorage.getItem('togyzkumalak_pgn') || '';
  if (!pgn) return;
  
  const moves = parsePgn(pgn);
  if (moves.length === 0) {
    alert('No valid moves found in PGN');
    return;
  }
  
  moveHistory = moves;
  currentMoveIndex = -1;
  allPositions = [];
  allFens = [];
  
  try {
    // Reset engine to initial position
    const resetPayload = await engine.send("reset");
    allPositions.push(resetPayload.state);
    allFens.push(resetPayload.fen);
    
    // Apply each move
    for (const moveStr of moves) {
      const move = parseMove(moveStr);
      if (!move) {
        alert(`Invalid move: ${moveStr}`);
        return;
      }
      
      const payload = await engine.send("humanMove", { pit: move.from });
      if (!payload.accepted) {
        alert(`Move not accepted: ${moveStr}`);
        return;
      }
      
      allPositions.push(payload.state);
      allFens.push(payload.fen);
    }
    
    currentPosition = allPositions[0];
    renderBoard();
    renderMoveHistory();
    updateGameInfo();
    
  } catch (error) {
    alert(`Error loading PGN: ${error.message}`);
    console.error(error);
  }
}

// Rotate board
function rotateBoard() {
  isRotated = !isRotated;
  renderBoard();
}

// Keyboard navigation
function handleKeydown(event) {
  switch (event.key) {
    case 'ArrowLeft':
      event.preventDefault();
      goToPrev();
      break;
    case 'ArrowRight':
      event.preventDefault();
      goToNext();
      break;
    case 'ArrowUp':
      event.preventDefault();
      goToFirst();
      break;
    case 'ArrowDown':
      event.preventDefault();
      goToLast();
      break;
  }
}

// Event listeners
els.rotateBoard.addEventListener('click', rotateBoard);
els.navFirst.addEventListener('click', goToFirst);
els.navPrev.addEventListener('click', goToPrev);
els.navNext.addEventListener('click', goToNext);
els.navLast.addEventListener('click', goToLast);
document.addEventListener('keydown', handleKeydown);

// Initialize engine and load PGN
engine
  .send("init")
  .then((payload) => {
    engineReady = true;
    allPositions.push(payload.state);
    allFens.push(payload.fen);
    currentPosition = payload.state;
    renderBoard();
    renderMoveHistory();
    updateGameInfo();
    
    // Load PGN from localStorage if available
    loadPgn();
  })
  .catch((error) => {
    console.error("Engine initialization failed:", error);
    alert("Engine initialization failed. Please make sure the WASM files are built.");
  });
