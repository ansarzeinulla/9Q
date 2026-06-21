const INITIAL_FEN = "9,9,9,9,9,9,9,9,9/9,9,9,9,9,9,9,9,9 0,0 -,- w 0";

const I18N = {
  en: {
    language: "Language",
    arxivSoon: "arXiv soon",
    mainTitle: "Browser Togyzkumalak engine",
    intro: "Choose players, set time per move for bots, optionally paste an initial position's FEN, then start.",
    instructionOne: "Click one of your nine pits to move.",
    instructionTwo: "Bots run locally in WebAssembly inside your browser.",
    instructionThree: "Download the move list as 9Q-PGN from the game page.",
    white: "Beginner",
    black: "Follower",
    human: "Human",
    randombot: "Random Bot",
    minimax: "Mini Max",
    dag: "DAG",
    timePerMove: "Time per move",
    noTimer: "No timer",
    fen: "FEN",
    pgnInput: "PGN (for analysis)",
    startGame: "Start",
    initialFen: "Initial FEN",
    analyzePgn: "Analyze PGN",
    analysisButton: "Analysis",
    downloadPgnButton: "PGN",
    formats: "Formats",
    fenTitle: "9Q-FEN",
    fenExplain: "FEN has five fields: white pits / black pits, kazans, tuzdyks, side to move, and half-move count.",
    fenDetails: "Pits are listed 1-9 for each owner. Tuzdyks use owner pit numbers or “-”. Beginner is “w” and Follower is “b”.",
    pgnTitle: "9Q-PGN",
    pgnExplain: "PGN stores chess-like tags followed by move pairs. Each move is two digits: origin pit and landing pit, both 1-9 relative to the owner of that cell.",
    pgnDetails: "“+” marks a capture. “x” marks tuzdyk creation.",
    setup: "Setup",
    statusLoading: "",
    statusReady: "",
    statusPlaying: "",
    wasmMissing: "Engine Wasm not found. Run npm run build:wasm before npm run build.",
    fenInvalid: "Invalid FEN. Use the 9Q-FEN format shown below.",
    unknownEngineError: "Engine request failed.",
    whiteToMove: "Beginner's turn",
    blackToMove: "Follower's turn",
    whiteThinking: "Beginner is thinking...",
    blackThinking: "Follower is thinking...",
    whiteWins: "Beginner wins",
    blackWins: "Follower wins",
    draw: "Draw",
    gameOver: "Game over",
    pitLabel: "{side} pit {pit}, {stones} stones",
    tuzdykPitLabel: "{side} pit {pit}, tuzdyk"
  },
  ru: {
    language: "Язык",
    arxivSoon: "Скоро на arXiv",
    mainTitle: "Браузерный движок тогызкумалака",
    intro: "Выберите игроков, установите время на ход для ботов, при желании вставьте FEN начальной позиции и начните игру.",
    instructionOne: "Нажмите на одну из ваших девяти лунок, чтобы сделать ход.",
    instructionTwo: "Боты работают локально через WebAssembly прямо в вашем браузере.",
    instructionThree: "Скачайте список ходов в формате 9Q-PGN со страницы игры.",
    white: "Начинающий",
    black: "Отвечающий",
    human: "Человек",
    randombot: "Случайный бот",
    minimax: "Mini Max",
    dag: "DAG",
    timePerMove: "Время на ход",
    noTimer: "Без таймера",
    fen: "FEN",
    pgnInput: "PGN (для анализа)",
    startGame: "Начать",
    initialFen: "Начальный FEN",
    analyzePgn: "Анализировать PGN",
    analysisButton: "Анализ",
    downloadPgnButton: "PGN",
    formats: "Форматы",
    fenTitle: "9Q-FEN",
    fenExplain: "FEN состоит из пяти полей: лунки начинающего / лунки отвечающего, казаны, туздыки, чей ход и счетчик полуходов.",
    fenDetails: "Лунки нумеруются от 1 до 9 для каждого игрока. Для туздыков используются номера лунок владельца или «-». Начинающий — «w», отвечающий — «b».",
    pgnTitle: "9Q-PGN",
    pgnExplain: "PGN содержит теги, подобные шахматным, за которыми следуют пары ходов. Каждый ход состоит из двух цифр: исходная лунка и конечная лунка, обе от 1 до 9 относительно владельца этой лунки.",
    pgnDetails: "«+» означает захват (выигрыш). «x» означает создание туздыка.",
    setup: "Настройки",
    statusLoading: "",
    statusReady: "",
    statusPlaying: "",
    wasmMissing: "Wasm движка не найден. Запустите npm run build:wasm перед npm run build.",
    fenInvalid: "Неверный FEN. Используйте формат 9Q-FEN, показанный ниже.",
    unknownEngineError: "Ошибка запроса к движку.",
    whiteToMove: "Ход начинающего",
    blackToMove: "Ход отвечающего",
    whiteThinking: "Начинающий думает...",
    blackThinking: "Отвечающий думает...",
    whiteWins: "Победа начинающего",
    blackWins: "Победа отвечающего",
    draw: "Ничья",
    gameOver: "Игра окончена",
    pitLabel: "{side} лунка {pit}, {stones} кумалаков",
    tuzdykPitLabel: "{side} лунка {pit}, туздык"
},

kk: {
    language: "Тіл",
    arxivSoon: "Жақында arXiv-те",
    mainTitle: "Браузердегі тоғызқұмалақ қозғалтқышы",
    intro: "Ойыншыларды таңдаңыз, боттар үшін жүріс уақытын орнатыңыз, қажет болса бастапқы позицияның FEN кодын қойып, ойынды бастаңыз.",
    instructionOne: "Жүріс жасау үшін өзіңіздің тоғыз отауыңыздың бірін басыңыз.",
    instructionTwo: "Боттар браузеріңіздің ішінде WebAssembly арқылы локальді түрде жұмыс істейді.",
    instructionThree: "Жүрістер тізімін ойын бетінен 9Q-PGN форматында жүктеп алыңыз.",
    white: "Бастаушы",
    black: "Қостаушы",
    human: "Адам",
    randombot: "Кездейсоқ бот",
    minimax: "Mini Max",
    dag: "DAG",
    timePerMove: "Жүріс уақыты",
    noTimer: "Таймерсіз",
    fen: "FEN",
    pgnInput: "PGN (талдау үшін)",
    startGame: "Бастау",
    initialFen: "Бастапқы FEN",
    analyzePgn: "PGN талдау",
    analysisButton: "Талдау",
    downloadPgnButton: "PGN",
    formats: "Форматтар",
    fenTitle: "9Q-FEN",
    fenExplain: "FEN бес бөліктен тұрады: бастаушының отаулары / қостаушының отаулары, қазандар, тұздықтар, жүріс кезегі және жартылай жүріс санағышы.",
    fenDetails: "Әр ойыншының отаулары 1-ден 9-ға дейін нөмірленеді. Тұздықтар үшін иесінің отау нөмірі немесе «-» белгісі қолданылады. Бастаушы — «w», ал қостаушы — «b».",
    pgnTitle: "9Q-PGN",
    pgnExplain: "PGN шахматқа ұқсас тегтерді және одан кейінгі жүрістер жұбын сақтайды. Әр жүріс екі санмен жазылады: бастапқы отау және түскен отау, екеуі де осы отау иесіне қатысты 1-9 аралығында көрсетіледі.",
    pgnDetails: "«+» ұтып алуды (құмалақ түсіруді) білдіреді. «x» тұздық алуды білдіреді.",
    setup: "Баптаулар",
    statusLoading: "",
    statusReady: "",
    statusPlaying: "",
    wasmMissing: "Қозғалтқыштың Wasm файлы табылмады. npm run build алдында npm run build:wasm іске қосыңыз.",
    fenInvalid: "Қате FEN. Төменде көрсетілген 9Q-FEN форматын пайдаланыңыз.",
    unknownEngineError: "Қозғалтқышқа сұраныс қатемен аяқталды.",
    whiteToMove: "Бастаушының жүрісі",
    blackToMove: "Қостаушының жүрісі",
    whiteThinking: "Бастаушы ойлануда...",
    blackThinking: "Қостаушы ойлануда...",
    whiteWins: "Бастаушы жеңді",
    blackWins: "Қостаушы жеңді",
    draw: "Тең түсу",
    gameOver: "Ойын аяқталды",
    pitLabel: "{side} {pit}-отау, {stones} құмалақ",
    tuzdykPitLabel: "{side} {pit}-отау, тұздық"
},

ky: {
    language: "Тил",
    arxivSoon: "Жакында arXiv'де",
    mainTitle: "Браузердеги тогуз коргоол кыймылдаткычы",
    intro: "Оюнчуларды тандаңыз, боттор үчүн жүрүш убактысын орнотуңуз, кааласаңыз баштапкы позициянын FEN кодун киргизип, оюнду баштаңыз.",
    instructionOne: "Жүрүш жасоо үчүн өзүңүздүн тогуз үйүңүздүн бирин басыңыз.",
    instructionTwo: "Боттор WebAssembly аркылуу браузериңиздин ичинде локалдуу иштешет.",
    instructionThree: "Жүрүштөрдүн тизмесин оюн барагынан 9Q-PGN форматында жүктөп алыңыз.",
    white: "Баштоочу",
    black: "Коштоочу",
    human: "Адам",
    randombot: "Кокустук бот",
    minimax: "Mini Max",
    dag: "DAG",
    timePerMove: "Жүрүш убактысы",
    noTimer: "Таймерсиз",
    fen: "FEN",
    pgnInput: "PGN (анализ үчүн)",
    startGame: "Баштоо",
    initialFen: "Баштапкы FEN",
    analyzePgn: "PGN анализ",
    analysisButton: "Анализ",
    downloadPgnButton: "PGN",
    formats: "Форматтар",
    fenTitle: "9Q-FEN",
    fenExplain: "FEN беш бөлүктөн турат: баштоочунун үйлөрү / коштоочунун үйлөрү, казандар, туздар, жүрүү кезеги жана жарым жүрүштөрдүн эсептегичи.",
    fenDetails: "Ар бир оюнчунун үйлөрү 1ден 9га чейин номерленет. Туздар үчүн ээсинин үйүнүн номери же «-» колдонулат. Баштоочу — «w», ал эми коштоочу — «b».",
    pgnTitle: "9Q-PGN",
    pgnExplain: "PGN шахматка окшош тегдерди жана андан кийинки жүрүштөр жуптарын сактайт. Ар бир жүрүш эки сан менен жазылат: баштапкы үй жана түшкөн үй, экөө тең ошол үйдүн ээсине карата 1ден 9га чейин көрсөтүлөт.",
    pgnDetails: "«+» утуп алууну билдирет. «x» туз алууну билдирет.",
    setup: "Жөндөөлөр",
    statusLoading: "",
    statusReady: "",
    statusPlaying: "",
    wasmMissing: "Кыймылдаткычтын Wasm файлы табылган жок. npm run build алдында npm run build:wasm иштетиңиз.",
    fenInvalid: "Туура эмес FEN. Төмөндө көрсөтүлгөн 9Q-FEN форматын колдонуңуз.",
    unknownEngineError: "Кыймылдаткычка суроо-талап ката менен аяктады.",
    whiteToMove: "Баштоочунун жүрүшү",
    blackToMove: "Коштоочунун жүрүшү",
    whiteThinking: "Баштоочу ойлонууда...",
    blackThinking: "Коштоочу ойлонууда...",
    whiteWins: "Баштоочу жеңди",
    blackWins: "Коштоочу жеңди",
    draw: "Тең чыгуу",
    gameOver: "Оюн бүттү",
    pitLabel: "{side} {pit}-үй, {stones} коргоол",
    tuzdykPitLabel: "{side} {pit}-үй, туз"
}
};

const PLAYER_KEYS = {
  human: "human",
  randombot: "randombot",
  minimax: "minimax",
  dag4: "dag"
};

const STORAGE_KEYS = {
  pgn: "togyzkumalak_pgn",
  analysisMoveIndex: "togyzkumalak_analysis_move_index",
  resumeFen: "togyzkumalak_resume_fen",
  language: "togyzkumalak_language"
};

const els = {
  setupPage: document.querySelector("#setup-page"),
  gamePage: document.querySelector("#game-page"),
  setupForm: document.querySelector("#setup-form"),
  status: document.querySelector("#engine-status"),
  language: document.querySelector("#language-select"),
  whitePlayer: document.querySelector("#white-player"),
  blackPlayer: document.querySelector("#black-player"),
  whiteTime: document.querySelector("#white-time"),
  blackTime: document.querySelector("#black-time"),
  whiteTimeControl: document.querySelector("#white-time-control"),
  blackTimeControl: document.querySelector("#black-time-control"),
  fenInput: document.querySelector("#fen-input"),
  pgnInput: document.querySelector("#pgn-input"),
  start: document.querySelector("#start-button"),
  initialFen: document.querySelector("#initial-fen-button"),
  analyzePgn: document.querySelector("#analyze-pgn-button"),
  setup: document.querySelector("#setup-button"),
  downloadPgn: document.querySelector("#download-pgn-button"),
  whiteKazan: document.querySelector("#white-kazan"),
  blackKazan: document.querySelector("#black-kazan"),
  turnLabel: document.querySelector("#turn-label"),
  gameEnd: document.querySelector("#game-end"),
  board: document.querySelector("#board"),
  moveLog: document.querySelector("#move-log")
};

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
let lang = localStorage.getItem(STORAGE_KEYS.language) || "en";
let pendingResumeFen = localStorage.getItem(STORAGE_KEYS.resumeFen) || "";
let state = createInitialState();
let engineReady = false;
let started = false;
let thinking = false;
let currentFen = INITIAL_FEN;
let startFen = INITIAL_FEN;
let moveRows = [];
let runToken = 0;
let statusKey = "statusLoading";
let statusArgs = {};
let statusIsError = false;

function createInitialState() {
  return {
    pits: {
      white: Array(9).fill(9),
      black: Array(9).fill(9)
    },
    kazans: { white: 0, black: 0 },
    tuzdyks: { white: -1, black: -1 },
    toPlay: "white",
    steps: 0,
    gameOver: false,
    winnerCode: -2,
    winner: "",
    legal: [0, 1, 2, 3, 4, 5, 6, 7, 8]
  };
}

function t(key, args = {}) {
  const table = I18N[lang] || I18N.en;
  const template = Object.prototype.hasOwnProperty.call(table, key)
    ? table[key]
    : Object.prototype.hasOwnProperty.call(I18N.en, key)
      ? I18N.en[key]
      : key;
  return template.replace(/\{(\w+)\}/g, (_, name) => String(args[name] ?? ""));
}

function errorKey(error) {
  const message = error && error.message ? error.message : "";
  if (message.includes("Engine Wasm not found")) return "wasmMissing";
  if (message.includes("fen_invalid")) return "fenInvalid";
  return "unknownEngineError";
}

function setStatus(key, args = {}, isError = false) {
  statusKey = key;
  statusArgs = args;
  statusIsError = isError;
  els.status.textContent = t(key, args);
  els.status.classList.toggle("error", isError);
}

function setError(error) {
  setStatus(errorKey(error), {}, true);
}

function applyTranslations() {
  document.documentElement.lang = lang;
  document.title = `${t("mainTitle")} - 9Q`;
  document.querySelectorAll("[data-i18n]").forEach((node) => {
    node.textContent = t(node.dataset.i18n);
  });
  document.querySelectorAll("[data-player-option]").forEach((option) => {
    option.textContent = t(PLAYER_KEYS[option.dataset.playerOption]);
  });
  if (els.analysisButton) els.analysisButton.textContent = t("analysisButton");
  if (els.downloadPgn) els.downloadPgn.textContent = t("downloadPgnButton");
  if (els.pgnInput) els.pgnInput.placeholder = t("pgnInput");
  els.setup.setAttribute("aria-label", t("setup"));
  els.setup.setAttribute("title", t("setup"));
  setStatus(statusKey, statusArgs, statusIsError);
}

function moveCountFromRows() {
  return moveRows.reduce((count, row) => count + (row.white ? 1 : 0) + (row.black ? 1 : 0), 0);
}

function playerFor(side) {
  return side === "white" ? els.whitePlayer.value : els.blackPlayer.value;
}

function playerName(side, locale = lang) {
  const value = playerFor(side);
  return I18N[locale][PLAYER_KEYS[value]] || value;
}

function isHuman(side) {
  return playerFor(side) === "human";
}

function isTimedPlayerValue(player) {
  return player === "minimax" || player === "dag4";
}

function isTimedPlayer(side) {
  return isTimedPlayerValue(playerFor(side));
}

function secondsFor(side) {
  if (!isTimedPlayer(side)) return 0;
  const input = side === "white" ? els.whiteTime : els.blackTime;
  const raw = Number(input.value);
  if (!Number.isFinite(raw)) return 0.1;
  return Math.min(30, Math.max(0.001, raw));
}

function sideLabel(side) {
  return t(side);
}

function resultText() {
  if (!state.gameOver) return "";
  if (state.winner === "white") return t("whiteWins");
  if (state.winner === "black") return t("blackWins");
  return t("draw");
}

function pgnResult() {
  if (!state.gameOver) return "*";
  if (state.winner === "white") return "1-0";
  if (state.winner === "black") return "0-1";
  return "1/2-1/2";
}

function renderStoneField(count, blocked) {
  if (blocked) return "";
  const visible = Math.min(count, 18);
  let html = "";
  for (let i = 0; i < visible; i++) {
    html += '<span class="stone"></span>';
  }
  return html;
}

function renderPit(side, pit, count, displayNumber, blocked) {
  const legal = state.legal.includes(pit);
  const canMove = started && !thinking && !state.gameOver && state.toPlay === side && isHuman(side) && legal && !blocked;
  const classes = ["pit", side, blocked ? "tuzdyk-cell" : "", canMove ? "legal-human" : ""].filter(Boolean).join(" ");
  const label = blocked
    ? t("tuzdykPitLabel", { side: sideLabel(side), pit: displayNumber })
    : t("pitLabel", { side: sideLabel(side), pit: displayNumber, stones: count });

  return `
    <button class="${classes}" type="button" data-side="${side}" data-pit="${pit}" ${canMove ? "" : "disabled"} aria-label="${label}">
      <span class="pit-label">${displayNumber}</span>
      <span class="stone-field" aria-hidden="true">${renderStoneField(count, blocked)}</span>
      <span class="pit-count">${blocked ? "" : count}</span>
    </button>
  `;
}

function renderBoard() {
  const blackPits = [];
  for (let pit = 8; pit >= 0; pit--) {
    blackPits.push(renderPit("black", pit, state.pits.black[pit], pit + 1, state.tuzdyks.white === pit));
  }

  const whitePits = [];
  for (let pit = 0; pit < 9; pit++) {
    whitePits.push(renderPit("white", pit, state.pits.white[pit], pit + 1, state.tuzdyks.black === pit));
  }

  els.board.innerHTML = `
    <div class="pit-row black-row">${blackPits.join("")}</div>
    <div class="pit-row white-row">${whitePits.join("")}</div>
  `;
}

function appendMove(move) {
  if (!move || !move.notation) return;
  if (move.side === "white") {
    moveRows.push({ white: move.notation, black: "" });
  } else if (moveRows.length === 0 || moveRows[moveRows.length - 1].black) {
    moveRows.push({ white: "", black: move.notation });
  } else {
    moveRows[moveRows.length - 1].black = move.notation;
  }
}

function renderLog() {
  els.moveLog.innerHTML = moveRows
    .map((row, index) => `
      <li class="move-row">
        <span class="move-number">${index + 1}.</span>
        <span class="move-code">${row.white || ""}</span>
        <span class="move-code">${row.black || ""}</span>
      </li>
    `)
    .join("");
  els.moveLog.scrollTop = els.moveLog.scrollHeight;
}

function renderSetupControls() {
  for (const [side, wrapper, input] of [
    ["white", els.whiteTimeControl, els.whiteTime],
    ["black", els.blackTimeControl, els.blackTime]
  ]) {
    const timeless = !isTimedPlayer(side);
    wrapper.classList.toggle("is-timeless", timeless);
    input.disabled = timeless;
  }
  els.start.disabled = !engineReady || thinking;
}

function renderStatusLine() {
  if (state.gameOver) {
    const result = resultText();
    els.turnLabel.textContent = result;
    els.gameEnd.textContent = `${t("gameOver")}: ${result}`;
    els.gameEnd.hidden = false;
  } else if (thinking) {
    els.turnLabel.textContent = t(state.toPlay === "white" ? "whiteThinking" : "blackThinking");
    els.gameEnd.hidden = true;
  } else {
    els.turnLabel.textContent = t(state.toPlay === "white" ? "whiteToMove" : "blackToMove");
    els.gameEnd.hidden = true;
  }
  els.gamePage.classList.toggle("game-over", state.gameOver);
}

function render() {
  els.whiteKazan.textContent = state.kazans.white;
  els.blackKazan.textContent = state.kazans.black;
  els.downloadPgn.disabled = !started;
  renderSetupControls();
  renderStatusLine();
  renderBoard();
  renderLog();
}

function showSetup() {
  els.setupPage.hidden = false;
  els.gamePage.hidden = true;
  document.body.classList.remove("is-playing");
}

function showGame() {
  els.setupPage.hidden = true;
  els.gamePage.hidden = false;
  document.body.classList.add("is-playing");
}

async function startGame() {
  const token = ++runToken;
  thinking = false;
  moveRows = [];
  const fen = (pendingResumeFen || els.fenInput.value).trim();
  pendingResumeFen = "";
  localStorage.removeItem(STORAGE_KEYS.resumeFen);
  const payload = await engine.send("setFen", { fen });
  state = payload.state;
  currentFen = payload.fen || fen || INITIAL_FEN;
  startFen = currentFen;
  started = true;
  setStatus("statusPlaying");
  showGame();
  render();
  continueBots(token);
}

async function loadInitialFen() {
  const payload = await engine.send("reset");
  state = payload.state;
  currentFen = payload.fen || INITIAL_FEN;
  startFen = currentFen;
  els.fenInput.value = currentFen;
  els.pgnInput.value = "";
  moveRows = [];
  started = false;
  thinking = false;
  runToken++;
  pendingResumeFen = "";
  localStorage.removeItem(STORAGE_KEYS.resumeFen);
  localStorage.removeItem(STORAGE_KEYS.pgn);
  localStorage.removeItem(STORAGE_KEYS.analysisMoveIndex);
  setStatus("statusReady");
  render();
}

function backToSetup() {
  runToken++;
  started = false;
  thinking = false;
  els.fenInput.value = currentFen || INITIAL_FEN;
  setStatus("statusReady");
  showSetup();
  render();
}

function goToAnalysis() {
  localStorage.setItem(STORAGE_KEYS.pgn, makePgn());
  localStorage.setItem(STORAGE_KEYS.analysisMoveIndex, String(Math.max(0, moveCountFromRows() - 1)));
  window.location.href = "/analysis.html";
}

async function playHumanMove(pit) {
  if (!state || !started || thinking || state.gameOver || !isHuman(state.toPlay)) return;
  const payload = await engine.send("humanMove", { pit });
  if (payload.accepted) {
    appendMove(payload.move);
    state = payload.state;
    currentFen = payload.fen || currentFen;
    render();
    continueBots(runToken);
  }
}

async function continueBots(token) {
  if (!state || !started || state.gameOver || token !== runToken) {
    render();
    return;
  }

  const side = state.toPlay;
  const player = playerFor(side);
  if (player === "human") {
    render();
    return;
  }

  thinking = true;
  render();
  await new Promise((resolve) => setTimeout(resolve, 20));
  if (token !== runToken) {
    thinking = false;
    render();
    return;
  }

  try {
    const payload = await engine.send("botMove", { player, seconds: secondsFor(side) });
    appendMove(payload.move);
    state = payload.state;
    currentFen = payload.fen || currentFen;
  } catch (error) {
    setError(error);
    started = false;
    showSetup();
  } finally {
    thinking = false;
    render();
  }

  if (token === runToken) {
    continueBots(token);
  }
}

function pgnDate() {
  const now = new Date();
  const yyyy = now.getFullYear();
  const mm = String(now.getMonth() + 1).padStart(2, "0");
  const dd = String(now.getDate()).padStart(2, "0");
  return `${yyyy}.${mm}.${dd}`;
}

function escapePgnTag(value) {
  return String(value).replace(/\\/g, "\\\\").replace(/"/g, '\\"');
}

function makePgn() {
  const result = pgnResult();
  const tags = [
    ["Event", "9Q Browser Game"],
    ["Site", "Local"],
    ["Date", pgnDate()],
    ["Variant", "Togyzkumalak"],
    ["Format", "9Q-PGN"],
    ["White", playerName("white", "en")],
    ["Black", playerName("black", "en")],
    ["FEN", startFen],
    ["Result", result]
  ];

  const header = tags.map(([key, value]) => `[${key} "${escapePgnTag(value)}"]`).join("\n");
  const moves = moveRows
    .map((row, index) => `${index + 1}. ${row.white || "..."}${row.black ? ` ${row.black}` : ""}`)
    .join(" ");
  return `${header}\n\n${moves}${moves && result !== "*" ? ` ${result}` : result !== "*" ? result : ""}\n`;
}

function downloadPgn() {
  const blob = new Blob([makePgn()], { type: "application/x-chess-pgn;charset=utf-8" });
  const url = URL.createObjectURL(blob);
  const stamp = new Date().toISOString().replace(/[-:]/g, "").slice(0, 15);
  const link = document.createElement("a");
  link.href = url;
  link.download = `9q-${stamp}.pgn`;
  document.body.appendChild(link);
  link.click();
  link.remove();
  URL.revokeObjectURL(url);
}

els.board.addEventListener("click", (event) => {
  const pitButton = event.target.closest(".pit");
  if (!pitButton || pitButton.disabled) return;
  playHumanMove(Number(pitButton.dataset.pit));
});

els.setupForm.addEventListener("submit", (event) => {
  event.preventDefault();
  if (!engineReady || thinking) return;
  startGame().catch((error) => {
    started = false;
    thinking = false;
    showSetup();
    setError(error);
    render();
  });
});

els.start.addEventListener("click", () => {
  if (!engineReady || thinking) return;
  startGame().catch((error) => {
    started = false;
    thinking = false;
    showSetup();
    setError(error);
    render();
  });
});

els.initialFen.addEventListener("click", () => {
  loadInitialFen().catch((error) => {
    setError(error);
  });
});

els.analyzePgn.addEventListener("click", () => {
  const pgn = els.pgnInput.value.trim();
  localStorage.setItem(STORAGE_KEYS.pgn, pgn);
  localStorage.setItem(STORAGE_KEYS.analysisMoveIndex, "0");
  window.location.href = "/analysis.html";
});

els.setup.addEventListener("click", backToSetup);
document.querySelector("#analysis-button")?.addEventListener("click", goToAnalysis);
els.downloadPgn.addEventListener("click", downloadPgn);

els.language.addEventListener("change", () => {
  lang = els.language.value;
  localStorage.setItem(STORAGE_KEYS.language, lang);
  applyTranslations();
  render();
});

for (const control of [els.whitePlayer, els.blackPlayer]) {
  control.addEventListener("change", render);
}

applyTranslations();
els.fenInput.value = INITIAL_FEN;
render();

engine
  .send("init")
  .then((payload) => {
    engineReady = true;
    if (pendingResumeFen) {
      return startGame();
    }
    state = payload.state;
    currentFen = payload.fen || INITIAL_FEN;
    startFen = currentFen;
    els.fenInput.value = currentFen;
    setStatus("statusReady");
    render();
  })
  .catch((error) => {
    engineReady = false;
    state = createInitialState();
    setError(error);
    render();
  });
