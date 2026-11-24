// Phone side of RO Diary.
//
// The phone holds the full history and the settings. The watch only gets what
// it needs to run the survey: reminder settings and the custom questions.
// Everything stays in this app's localStorage; nothing is sent to a server.
//
// Written in ES5 on purpose: PebbleKit JS runs on old JavaScript engines on
// some phones.

var configHtml = require('./generated/config_html');

var SETTINGS_KEY = 'settings';
var ENTRIES_KEY = 'entries';
var DIRTY_KEY = 'configDirty';

var MAX_CUSTOM = 24;
var MAX_LABEL_BYTES = 63;
var FIRST_CUSTOM_ID = 10;
var MAX_SEND_ATTEMPTS = 3;

function load(key, fallback) {
  try {
    var raw = localStorage.getItem(key);
    return raw ? JSON.parse(raw) : fallback;
  } catch (e) {
    return fallback;
  }
}

function save(key, value) {
  localStorage.setItem(key, JSON.stringify(value));
}

function loadSettings() {
  var settings = load(SETTINGS_KEY, {});
  if (typeof settings.reminderEnabled !== 'boolean') settings.reminderEnabled = true;
  if (typeof settings.hour !== 'number') settings.hour = 21;
  if (typeof settings.minute !== 'number') settings.minute = 0;
  if (typeof settings.snooze !== 'number') settings.snooze = 15;
  if (typeof settings.weekStart !== 'number') settings.weekStart = 1;
  if (typeof settings.nextId !== 'number') settings.nextId = FIRST_CUSTOM_ID;
  if (!(settings.questions instanceof Array)) settings.questions = [];
  return settings;
}

function clampInt(value, min, max, fallback) {
  var n = parseInt(value, 10);
  if (isNaN(n)) return fallback;
  return Math.min(max, Math.max(min, n));
}

// Cuts a label so it fits the watch's buffer without splitting a character.
function fitLabel(label) {
  var out = '';
  var bytes = 0;
  for (var i = 0; i < label.length; i++) {
    var code = label.charCodeAt(i);
    var ch = label.charAt(i);
    var size;
    if (code >= 0xd800 && code <= 0xdbff && i + 1 < label.length) {
      ch += label.charAt(++i);
      size = 4;
    } else {
      size = code < 0x80 ? 1 : code < 0x800 ? 2 : 3;
    }
    if (bytes + size > MAX_LABEL_BYTES) break;
    out += ch;
    bytes += size;
  }
  return out;
}

// ------------------------------------------------------------ sending to watch

var queue = [];
var sending = false;

function pump() {
  if (sending || !queue.length) return;
  sending = true;
  var item = queue[0];
  Pebble.sendAppMessage(item.message, function () {
    sending = false;
    queue.shift();
    if (item.done) item.done();
    pump();
  }, function () {
    sending = false;
    item.attempts++;
    if (item.attempts >= MAX_SEND_ATTEMPTS) {
      // Give up for now. Unsent settings stay flagged and go out on the next launch.
      queue = [];
      return;
    }
    setTimeout(pump, 500);
  });
}

function send(message, done) {
  queue.push({ message: message, attempts: 0, done: done });
  pump();
}

function activeQuestions(settings) {
  var active = [];
  for (var i = 0; i < settings.questions.length; i++) {
    if (!settings.questions[i].archived) active.push(settings.questions[i]);
  }
  return active.slice(0, MAX_CUSTOM);
}

function syncConfig() {
  var settings = loadSettings();
  var questions = activeQuestions(settings);
  save(DIRTY_KEY, true);

  send({
    CFG_BEGIN: questions.length,
    CFG_ENABLED: settings.reminderEnabled ? 1 : 0,
    CFG_HOUR: settings.hour,
    CFG_MINUTE: settings.minute,
    CFG_SNOOZE: settings.snooze
  });
  for (var i = 0; i < questions.length; i++) {
    send({
      Q_INDEX: i,
      Q_ID: questions[i].id,
      Q_TYPE: questions[i].type,
      Q_SECTION: questions[i].section,
      Q_LABEL: fitLabel(questions[i].label)
    });
  }
  send({ CFG_END: questions.length }, function () {
    save(DIRTY_KEY, false);
  });
}

// ------------------------------------------------------ entries from the watch

function storeEntry(date, bytes) {
  var answers = {};
  // Three bytes per answer: question id (low, high), value.
  for (var i = 0; i + 2 < bytes.length; i += 3) {
    answers[bytes[i] | (bytes[i + 1] << 8)] = bytes[i + 2];
  }
  var entries = load(ENTRIES_KEY, {});
  entries[date] = answers;
  save(ENTRIES_KEY, entries);
}

Pebble.addEventListener('appmessage', function (e) {
  var payload = e.payload || {};
  if (payload.ENTRY_DATE !== undefined) {
    storeEntry(payload.ENTRY_DATE, payload.ENTRY_DATA || []);
    send({ ENTRY_ACK: payload.ENTRY_DATE });
  }
});

Pebble.addEventListener('ready', function () {
  if (load(DIRTY_KEY, true)) {
    syncConfig();
  }
  // Tells the watch it can upload entries it has been holding on to.
  send({ READY: 1 });
});

// --------------------------------------------------------------- settings page

Pebble.addEventListener('showConfiguration', function () {
  var state = { settings: loadSettings(), entries: load(ENTRIES_KEY, {}) };
  // "<" is escaped so the data can never close the page's <script> tag.
  var json = JSON.stringify(state).replace(/</g, '\\u003c');
  var html = configHtml.replace('"__STATE__"', function () { return json; });
  Pebble.openURL('data:text/html;charset=utf-8,' + encodeURIComponent(html));
});

function applyConfig(result) {
  var settings = loadSettings();
  settings.reminderEnabled = !!result.reminderEnabled;
  settings.hour = clampInt(result.hour, 0, 23, settings.hour);
  settings.minute = clampInt(result.minute, 0, 59, settings.minute);
  settings.snooze = clampInt(result.snooze, 1, 120, settings.snooze);
  settings.weekStart = clampInt(result.weekStart, 0, 6, settings.weekStart);

  var previous = {};
  var i;
  for (i = 0; i < settings.questions.length; i++) {
    previous[settings.questions[i].id] = settings.questions[i];
  }

  var questions = [];
  var kept = {};
  var incoming = result.questions instanceof Array ? result.questions : [];
  for (i = 0; i < incoming.length && questions.length < MAX_CUSTOM; i++) {
    var label = String(incoming[i].label || '').replace(/\s+/g, ' ').replace(/^ | $/g, '');
    if (!label) continue;
    var existing = previous[incoming[i].id];
    if (existing && !kept[existing.id]) {
      // The answer type of an existing question never changes, so old answers stay meaningful.
      kept[existing.id] = true;
      questions.push({
        id: existing.id,
        section: incoming[i].section === 2 ? 2 : 1,
        type: existing.type,
        label: label
      });
    } else {
      questions.push({
        id: settings.nextId++,
        section: incoming[i].section === 2 ? 2 : 1,
        type: incoming[i].type === 1 ? 1 : 0,
        label: label
      });
    }
  }

  // Removed questions are kept as archived so past answers still export with a name.
  for (i = 0; i < settings.questions.length; i++) {
    var old = settings.questions[i];
    if (!kept[old.id]) {
      questions.push({ id: old.id, section: old.section, type: old.type, label: old.label, archived: true });
    }
  }

  settings.questions = questions;
  save(SETTINGS_KEY, settings);
  syncConfig();
}

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response || e.response === 'CANCELLED') return;
  var result;
  try {
    result = JSON.parse(decodeURIComponent(e.response));
  } catch (err) {
    try {
      result = JSON.parse(e.response);
    } catch (err2) {
      return;
    }
  }
  if (result && typeof result === 'object') {
    applyConfig(result);
  }
});
