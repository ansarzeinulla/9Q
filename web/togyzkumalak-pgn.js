// Togyzkumalak PGN Viewer

import { I18N, getStoredLanguage, storeLanguage, translate } from "/i18n.js";

const INITIAL_FEN = "9,9,9,9,9,9,9,9,9/9,9,9,9,9,9,9,9,9 0,0 -,- w 0";

const STORAGE_KEYS = {
  pgn: "togyzkumalak_pgn",
  analysisMoveIndex: "togyzkumalak_analysis_move_index",
  resumeFen: "togyzkumalak_resume_fen"
};

// State
let currentPosition = null;
let moveHistory = [];
let currentMoveIndex = -1;
let isRotated = false;
let allPositions = [];
let allFens = [INITIAL_FEN];
let engineReady = false;
let dagEnabled = false;
let currentEvaluations = [];
let syncPromise = Promise.resolve();
let dagTimer = null;
let currentAnalysisGen = 0;
let lang = getStoredLanguage();

// DOM elements
const els = {
  board: document.getElementById('board'),
  moveHistory: document.getElementById('move-history'),
  evaluations: document.getElementById('evaluations'),
  playFromHere: document.getElementById('play-from-here'),
  language: document.getElementById('analysis-language'),
  rotateBoard: document.getElementById('rotate-board'),
  navFirst: document.getElementById('nav-first'),
  navPrev: document.getElementById('nav-prev'),
  navNext: document.getElementById('nav-next'),
  navLast: document.getElementById('nav-last'),
  dagToggle: document.getElementById('dag-toggle'),
  turnLabel: document.getElementById('turn-label'),
  whiteKazan: document.getElementById('white-kazan'),
  blackKazan: document.getElementById('black-kazan')
};

function t(key, args) {
  return translate(lang, key, args);
}

function applyTranslations() {
  document.documentElement.lang = lang;
  document.title = t("analysisTitle");
  const topLinks = document.querySelector(".top-links");
  if (topLinks) {
    const spans = topLinks.querySelectorAll("span.muted-link, button, a");
    if (spans[0]) spans[0].textContent = t("analysisTitle");
    if (spans[1]) spans[1].textContent = t("playFromHere");
    if (spans[2]) spans[2].textContent = t("backToGame");
  }
  const headings = document.querySelectorAll(".sidebar-heading h2");
  const hints = document.querySelectorAll(".sidebar-heading p");
  if (headings[0]) headings[0].textContent = t("moveHistory");
  if (hints[0]) hints[0].textContent = t("moveHistoryHint");
  if (headings[1]) headings[1].textContent = t("engineEvaluations");
  if (hints[1]) hints[1].textContent = t("engineHint");
  const dagLabel = document.querySelector(".toggle-label");
  if (dagLabel) dagLabel.textContent = t("dagAnalysis");
  const whiteLabel = document.querySelector("#white-kazan").parentElement?.querySelector(".side-label");
  const blackLabel = document.querySelector("#black-kazan").parentElement?.querySelector(".side-label");
  if (whiteLabel) whiteLabel.textContent = t("white");
  if (blackLabel) blackLabel.textContent = t("black");
  if (els.language) els.language.value = lang;
  if (els.navFirst) els.navFirst.title = t("navFirst");
  if (els.navPrev) els.navPrev.title = t("navPrev");
  if (els.navNext) els.navNext.title = t("navNext");
  if (els.navLast) els.navLast.title = t("navLast");
  if (els.rotateBoard) els.rotateBoard.title = t("rotateBoard");
}

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
  if (typeof pgn !== "string") return [];

  const cleanPgn = pgn
    .replace(/\[[^\]]*]/g, " ")
    .replace(/\{[^}]*}/g, " ")
    .replace(/;.*$/gm, " ")
    .replace(/\d+\.(\.\.)?/g, " ")
    .replace(/\$\d+/g, " ")
    .replace(/1-0|0-1|1\/2-1\/2|\*/g, " ")
    .trim();

  return cleanPgn
    .split(/\s+/)
    .map((token) => token.trim())
    .filter((token) => /^[1-9][1-9][+x]?$/.test(token));
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
function renderStoneField(count, side) {
  // 2×5 grid with staircase fill towards player's side.
  // All 10 positions rendered; empty ones hidden via CSS.
  const filled = Math.min(count, 10);
  const stacked = Math.min(Math.max(count - 10, 0), 10);
  let html = "";
  for (let i = 0; i < 10; i++) {
    let cls = "stone";
    if (i < stacked) cls += " stacked";
    else if (i < filled) cls += " filled";
    else cls += " empty";
    html += `<span class="${cls}"></span>`;
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
    <div class="${classes}" data-side="${side}" data-pit="${pit}" aria-label="${label}">
      <span class="pit-label">${displayNumber}</span>
      <span class="stone-field" aria-hidden="true">${renderStoneField(count, side)}</span>
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
  
  // Add click handlers for hand play
  document.querySelectorAll('.pit').forEach(pit => {
    pit.addEventListener('click', () => {
      const side = pit.dataset.side;
      const pitIndex = parseInt(pit.dataset.pit);
      handlePitClick(side, pitIndex);
    });
  });
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
        els.turnLabel.textContent = t("whiteWins");
      } else if (currentPosition.winner === "black") {
        els.turnLabel.textContent = t("blackWins");
      } else {
        els.turnLabel.textContent = t("draw");
      }
    } else {
      els.turnLabel.textContent = currentPosition.toPlay === "white" ? t("whiteToMove") : t("blackToMove");
    }
  }
}

// Go to specific move
function goToMove(index) {
  if (!engineReady) return;
  if (index < -1 || index >= moveHistory.length) return;
  
  // 1) Stop analysis and bounce generation instantly to kill lag
  currentAnalysisGen++;
  clearTimeout(dagTimer);
  
  currentMoveIndex = index;
  if (index === -1) {
    currentPosition = allPositions[0];
  } else {
    currentPosition = allPositions[index + 1];
  }
  
  renderBoard();
  renderMoveHistory();
  updateGameInfo();
  currentEvaluations = [];
  renderEvaluations();
  
  syncPromise = syncPromise.then(async () => {
    try {
      const targetFen = allFens[index + 1];
      let synced = false;
      
      if (targetFen) {
        try {
          await engine.send("setFen", { fen: targetFen });
          synced = true;
        } catch (e1) {
          try {
            await engine.send("loadFen", { fen: targetFen });
            synced = true;
          } catch (e2) {}
        }
      }
      
      if (!synced) {
        await engine.send("reset");
        for (let i = 0; i <= index; i++) {
          const move = parseMove(moveHistory[i]);
          if (move) {
            await engine.send("humanMove", { pit: move.from });
          }
        }
      }
    } catch (error) {
      console.error("Failed to sync engine state:", error);
    }
    
    // 2) DEBOUNCE the engine queue. Wait 200ms of user inactivity before analyzing
    if (dagEnabled && currentMoveIndex === index) {
      clearTimeout(dagTimer);
      dagTimer = setTimeout(runDagAnalysis, 200); 
    }
  }).catch(err => console.error(err));
}

// Handle pit click for hand play
function handlePitClick(side, pit) {
  if (!engineReady || !currentPosition) return;
  
  const sideToPlay = currentPosition.toPlay;
  if ((side === 'white' && sideToPlay !== 'white') || (side === 'black' && sideToPlay !== 'black')) {
    return;
  }
  
  const tuzdyk = side === 'white' ? currentPosition.tuzdyks.black : currentPosition.tuzdyks.white;
  if (pit === tuzdyk) return;

  // Bust lag and stop background tasks instantly on click
  currentAnalysisGen++;
  clearTimeout(dagTimer);
  
  syncPromise = syncPromise.then(async () => {
    // Re-verify it is still our turn (in case of rapid clicks)
    if (currentPosition.toPlay !== sideToPlay) return;

    // We MUST calculate notation BEFORE sending the move to the engine, 
    // because notation depends on the board state prior to the stones moving
    let notation = getMoveNotation(currentPosition, side, pit);

    const payload = await engine.send("humanMove", { pit });
    if (!payload.accepted) return;
    
    // Handle history override/extend properly
    if (moveHistory.length === 0 || currentMoveIndex === -1) {
      // Starting from beginning
      moveHistory = [notation];
      allPositions = [allPositions[0], payload.state];
      allFens = [allFens[0], payload.fen];
      currentMoveIndex = 0;
    } else if (currentMoveIndex === moveHistory.length - 1) {
      // Adding to the very end of current history
      moveHistory.push(notation);
      allPositions.push(payload.state);
      allFens.push(payload.fen);
      currentMoveIndex++;
    } else if (currentMoveIndex < moveHistory.length - 1) {
      // Overriding a past move: slice off the old "future" and start a new timeline
      moveHistory = moveHistory.slice(0, currentMoveIndex + 1);
      allPositions = allPositions.slice(0, currentMoveIndex + 2);
      allFens = allFens.slice(0, currentMoveIndex + 2);
      
      moveHistory.push(notation);
      allPositions.push(payload.state);
      allFens.push(payload.fen);
      currentMoveIndex++;
    }
    
    currentPosition = payload.state;
    renderBoard();
    renderMoveHistory();
    updateGameInfo();
    currentEvaluations = [];
    renderEvaluations();
    
    // Debounce the DAG analysis so it doesn't lag the UI
    if (dagEnabled) {
      clearTimeout(dagTimer);
      dagTimer = setTimeout(runDagAnalysis, 200); 
    }
  }).catch(err => console.error(err));
}

// Format move notation
// Calculate standard notation for a move directly from rules
function getMoveNotation(state, side, pit) {
  if (!state || !state.pits || !state.pits[side]) return `${pit + 1}?`;
  
  const count = state.pits[side][pit];
  if (count === 0) return `${pit + 1}?`;
  
  const distance = count === 1 ? 1 : count - 1;
  const absoluteStart = side === 'white' ? pit : pit + 9;
  const absoluteEnd = (absoluteStart + distance) % 18;
  
  const endSide = absoluteEnd < 9 ? 'white' : 'black';
  const endPit = absoluteEnd % 9;
  
  let notation = `${pit + 1}${endPit + 1}`;
  
  if (endSide !== side) {
    const isWhiteTuzdyk = state.tuzdyks.white === endPit && endSide === 'black';
    const isBlackTuzdyk = state.tuzdyks.black === endPit && endSide === 'white';
    
    if (!isWhiteTuzdyk && !isBlackTuzdyk) {
      const newStones = state.pits[endSide][endPit] + 1;
      
      if (newStones === 3 && endPit !== 8) {
        // Check if player already has a tuzdyk
        const hasTuzdyk = state.tuzdyks[side] != null && state.tuzdyks[side] !== -1;
        // Check symmetry rule
        const opponent = side === 'white' ? 'black' : 'white';
        const opponentTuzdyk = state.tuzdyks[opponent];
        const isSymmetric = opponentTuzdyk === endPit;
        
        if (!hasTuzdyk && !isSymmetric) {
          notation += 'x';
        }
      } else if (newStones % 2 === 0) {
        notation += '+';
      }
    }
  }
  
  return notation;
}

// Calculate weighted average of evaluations
function calculateWeightedAverage(depthEvals) {
  if (!depthEvals || depthEvals.length === 0) return 0;
  
  const validEvals = depthEvals.filter(e => typeof e.score === 'number' && !isNaN(e.score));
  if (validEvals.length === 0) return 0;
  
  const totalWeight = validEvals.reduce((sum, e) => sum + e.depth, 0);
  if (totalWeight === 0) return 0;
  
  const weightedSum = validEvals.reduce((sum, e) => sum + (e.score * e.depth), 0);
  
  return weightedSum / totalWeight;
}

// Run DAG analysis
async function runDagAnalysis() {
  if (!engineReady || !currentPosition || !dagEnabled) return;
  
  // Lock this request to the current UI generation
  const generation = currentAnalysisGen;
  
  try {
    const payload = await engine.send("analyzePosition");
    
    // Drop results if the user already clicked away to another move
    if (generation !== currentAnalysisGen) return;
    
    currentEvaluations = payload.evaluations || [];
    renderEvaluations();
    
    // Loop the polling, securely bound to the current generation
    if (dagEnabled && !currentPosition.gameOver) {
      clearTimeout(dagTimer);
      dagTimer = setTimeout(() => {
        if (generation === currentAnalysisGen) runDagAnalysis();
      }, 1000);
    }
  } catch (error) {
    if (generation === currentAnalysisGen) {
      console.error("DAG analysis failed:", error);
      currentEvaluations = [];
      renderEvaluations();
    }
  }
}

// Render evaluations
function renderEvaluations() {
  const container = els.evaluations;
  container.innerHTML = '';
  
  if (!dagEnabled) {
    container.style.display = 'none';
    return;
  }
  
  container.style.display = 'block';
  
  const title = document.createElement('div');
  title.className = 'evaluations-title';
  title.textContent = t("dagAnalysis");
  container.appendChild(title);
  
  if (currentPosition.gameOver) {
    const terminalMsg = document.createElement('div');
    terminalMsg.className = 'evaluation-moves';
    terminalMsg.textContent = currentPosition.winner === currentPosition.toPlay ? t("winningPosition") : t("losingPosition");
    container.appendChild(terminalMsg);
    return;
  }
  
  if (currentEvaluations.length === 0) {
    const noMovesMsg = document.createElement('div');
    noMovesMsg.className = 'evaluation-moves';
    noMovesMsg.textContent = t("analyze");
    container.appendChild(noMovesMsg);
    return;
  }
  
  const movesContainer = document.createElement('div');
  movesContainer.className = 'evaluation-moves';
  
  const isWhiteToPlay = currentPosition.toPlay === 'white';

  // 1) Assign calculated scores for sorting
  const evaluatedMoves = currentEvaluations.map(ev => ({
    ...ev,
    calculatedScore: calculateWeightedAverage(ev.depthEvals)
  }));

  // 2) Sort logically: White searches for MAX, Black searches for MIN
  evaluatedMoves.sort((a, b) => {
    return isWhiteToPlay 
      ? b.calculatedScore - a.calculatedScore // Descending for White
      : a.calculatedScore - b.calculatedScore; // Ascending for Black
  });
  
  for (const evaluation of evaluatedMoves) {
    const rawScore = evaluation.calculatedScore;
    const safeScore = typeof rawScore === 'number' && !isNaN(rawScore) ? rawScore : 0;
    
    const pit = evaluation.pit !== undefined ? evaluation.pit : evaluation.move;
    const engineNotation = evaluation.move && typeof evaluation.move === "object" ? evaluation.move.notation : "";
    const safeNotation = engineNotation || getMoveNotation(currentPosition, currentPosition.toPlay, pit);
    
    const moveEl = document.createElement('div');
    moveEl.className = 'evaluation-move';
    
    // Highlight based on current player perspective
    if (safeScore > 0.3) moveEl.classList.add(isWhiteToPlay ? 'positive' : 'negative');
    if (safeScore < -0.3) moveEl.classList.add(isWhiteToPlay ? 'negative' : 'positive');
    
    moveEl.innerHTML = `
      <span class="move-notation">${safeNotation}</span>
      <span class="move-score">${safeScore > 0 ? '+' : ''}${safeScore.toFixed(2)}</span>
    `;
    
    movesContainer.appendChild(moveEl);
  }
  
  container.appendChild(movesContainer);
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
function loadPgn(pgnString) {
  if (!engineReady) {
    alert('Engine not ready yet. Please wait...');
    return;
  }
  
  const storedPgn = pgnString ?? localStorage.getItem(STORAGE_KEYS.pgn) ?? '';
  const pgn = typeof storedPgn === 'string' ? storedPgn : '';
  
  const moves = parsePgn(pgn);
  
  // Ensure parsing & loading happens safely in the queue
  syncPromise = syncPromise.then(async () => {
    currentAnalysisGen++;
    clearTimeout(dagTimer);
    currentEvaluations = [];
    renderEvaluations();

    moveHistory = moves;
    currentMoveIndex = -1;
    allPositions = [];
    allFens = [];
    
    try {
      // 1) Reset engine and save initial state
      const resetPayload = await engine.send("reset");
      allPositions.push(resetPayload.state);
      allFens.push(resetPayload.fen);

      if (moves.length === 0) {
        currentPosition = resetPayload.state;
        localStorage.removeItem(STORAGE_KEYS.analysisMoveIndex);
        renderBoard();
        renderMoveHistory();
        updateGameInfo();
        currentEvaluations = [];
        renderEvaluations();

        if (dagEnabled) {
          runDagAnalysis();
        }
        return;
      }
      
      // 2) Build states and FENs loop natively
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
      
      // 3) Crucial fix: rewind engine back to the start state so it matches the UI visually
      await engine.send("reset");
      
      currentMoveIndex = -1;
      currentPosition = allPositions[0];
      const storedIndex = Number(localStorage.getItem(STORAGE_KEYS.analysisMoveIndex));
      if (Number.isInteger(storedIndex) && storedIndex >= 0 && storedIndex < moveHistory.length) {
        currentMoveIndex = storedIndex;
        currentPosition = allPositions[storedIndex + 1] || allPositions[0];
      }
      
      renderBoard();
      renderMoveHistory();
      updateGameInfo();
      currentEvaluations = [];
      renderEvaluations();
      
      if (dagEnabled) {
        runDagAnalysis();
      }
    } catch (error) {
      alert(`Error loading PGN: ${error.message}`);
      console.error(error);
    }
  }).catch(err => console.error(err));
}

function playFromHere() {
  if (!currentPosition) return;
  localStorage.setItem(STORAGE_KEYS.resumeFen, allFens[currentMoveIndex + 1] || allFens[0] || INITIAL_FEN);
  window.location.href = '/';
}
// Rotate board
function rotateBoard() {
  isRotated = !isRotated;
  renderBoard();
  
  // Re-render evaluations to mirror scores if DAG is enabled
  if (dagEnabled) {
    renderEvaluations();
  }
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
els.playFromHere?.addEventListener('click', playFromHere);
els.language?.addEventListener('change', () => {
  lang = els.language.value;
  storeLanguage(lang);
  applyTranslations();
  renderMoveHistory();
  updateGameInfo();
  renderEvaluations();
});
els.dagToggle.addEventListener('change', () => {
  dagEnabled = els.dagToggle.checked;
  if (dagEnabled) {
    currentAnalysisGen++;
    runDagAnalysis();
  } else {
    clearTimeout(dagTimer);
    els.evaluations.style.display = 'none';
  }
});
document.addEventListener('keydown', handleKeydown);

// Initialize engine and load PGN
engine
  .send("init")
  .then((payload) => {
    engineReady = true;
    allPositions.push(payload.state);
    allFens.push(payload.fen);
    currentPosition = payload.state;
    applyTranslations();
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
