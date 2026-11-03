// TinySci settings page. Runs on the phone: the gear next to TinySci
// in the Pebble app opens it. The choices are kept on the phone and sent to
// the watch, which saves its own copy (they work without the phone).

var KEY = 'solvercalc-settings';
var DEFAULTS = { BtnUp: 0, BtnUpHold: 6, BtnDown: 3, BtnDownHold: 5, BtnSelect: 2, BtnSelectHold: 3,
                 Graph: 0, Buzz: 1, Backlight: 0, Format: 0, Digits: 4 };

// Button actions, in the order the watch numbers them (A_* in main.c).
var ACTIONS = ['Next page', 'Previous page', '= (equals / solve)', 'AC (clear)', 'DEL (delete)',
               'Formula view', 'History', 'Solver keys', 'Highlight previous key', 'Highlight next key',
               'Press highlighted key', 'Ans', 'Nothing'];
var BUTTONS = [['BtnUp', 'Up'], ['BtnUpHold', 'Up, held'], ['BtnDown', 'Down'], ['BtnDownHold', 'Down, held'],
               ['BtnSelect', 'Select'], ['BtnSelectHold', 'Select, held']];

// The button section: one list of actions per button and hold.
function buttons(s) {
  var h = '<div class="s"><div class="t">Buttons</div><div class="h">What each button does on the calculator. ' +
          'Back always deletes, and holding Back exits.</div>';
  for (var b = 0; b < BUTTONS.length; b++) {
    h += '<div class="b"><span>' + BUTTONS[b][1] + '</span><select data-k="' + BUTTONS[b][0] + '">';
    for (var a = 0; a < ACTIONS.length; a++)
      h += '<option value="' + a + '"' + (a === s[BUTTONS[b][0]] ? ' selected' : '') + '>' + ACTIONS[a] + '</option>';
    h += '</select></div>';
  }
  return h + '<button id="def" class="lnk">Reset buttons to default</button></div>';
}

function load() {
  var s = {};
  for (var k in DEFAULTS) s[k] = DEFAULTS[k];
  try {
    var saved = JSON.parse(localStorage.getItem(KEY) || '{}');
    for (var j in saved) if (j in DEFAULTS) s[j] = saved[j];
  } catch (e) {}
  return s;
}

// One setting: a title, a line of help, and a row of choices.
function row(key, title, help, choices, value) {
  var h = '<div class="s"><div class="t">' + title + '</div><div class="h">' + help + '</div><div class="c">';
  for (var i = 0; i < choices.length; i++)
    h += '<button data-k="' + key + '" data-v="' + i + '"' + (i === value ? ' class="on"' : '') + '>' +
         choices[i] + '</button>';
  return h + '</div></div>';
}

function page(s) {
  return '<!DOCTYPE html><html><head><meta charset="utf-8">' +
    '<meta name="viewport" content="width=device-width,initial-scale=1">' +
    '<title>TinySci</title><style>' +
    'body{margin:0;padding:16px;background:#101828;color:#fff;font:16px -apple-system,Roboto,sans-serif}' +
    'h1{font-size:22px;margin:4px 0 2px}p.sub{color:#9aa4b8;margin:0 0 16px;font-size:14px}' +
    '.s{background:#1d2840;border-radius:12px;padding:12px 14px;margin-bottom:12px}' +
    '.t{font-weight:600}.h{color:#9aa4b8;font-size:13px;margin:2px 0 10px}' +
    '.c{display:flex;gap:6px}.c button{flex:1;padding:10px 4px;border:0;border-radius:8px;' +
    'background:#2b3958;color:#dfe6f5;font-size:14px}.c button.on{background:#ffaa00;color:#000;font-weight:600}' +
    '.b{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:6px 0}' +
    '.b span{color:#dfe6f5;white-space:nowrap}.b select{flex:0 1 62%;padding:9px 8px;border-radius:8px;' +
    'border:0;background:#2b3958;color:#fff;font-size:14px}' +
    '.lnk{background:none;border:0;color:#7fb3ff;font-size:14px;padding:8px 0 0}' +
    'label{display:flex;align-items:center;gap:10px;color:#ffb0b0}input{width:20px;height:20px}' +
    '#save{width:100%;padding:14px;border:0;border-radius:12px;background:#00aa55;color:#fff;' +
    'font-size:17px;font-weight:600;margin-top:4px}' +
    '</style></head><body>' +
    '<h1>TinySci</h1><p class="sub">Settings are saved on your watch.</p>' +
    buttons(s) +
    row('Format', 'Number format', 'How answers are shown. Norm: up to 10 digits. Sci: 1.234e5. ' +
        'Eng: powers of 1000 (123.4e3). Fix: a set number of decimals. The MODE key on the watch steps through them too.',
        ['Norm', 'Sci', 'Eng', 'Fix'], s.Format) +
    '<div class="s"><div class="b"><span>Digits for Sci, Eng, Fix</span><select data-k="Digits">' +
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9].map(function (d) {
      return '<option value="' + d + '"' + (d === s.Digits ? ' selected' : '') + '>' + d + '</option>';
    }).join('') + '</select></div></div>' +
    row('Graph', 'Solver graph', 'Graph both sides of the equation on the solver sheet.', ['Off', 'On'], s.Graph) +
    row('Buzz', 'Key buzz', 'A short buzz when a key registers.', ['Off', 'Light', 'Strong'], s.Buzz) +
    row('Backlight', 'Backlight', 'Watch: as the watch is set. On press: lights up with each key. ' +
        'Always: stays on while the calculator is open.', ['Watch', 'On press', 'Always'], s.Backlight) +
    '<div class="s"><label><input type="checkbox" id="clr">Clear the answer history</label></div>' +
    '<button id="save">Save</button>' +
    '<script>' +
    'var s=' + JSON.stringify(s) + ';' +
    'document.querySelectorAll(".c button").forEach(function(b){b.onclick=function(){' +
    'var k=b.getAttribute("data-k");s[k]=+b.getAttribute("data-v");' +
    'document.querySelectorAll("[data-k="+k+"]").forEach(function(o){o.className=o===b?"on":""});};});' +
    'document.querySelectorAll("select").forEach(function(e){e.onchange=function(){s[e.getAttribute("data-k")]=+e.value;};});' +
    'var D=' + JSON.stringify(DEFAULTS) + ';' +
    'document.getElementById("def").onclick=function(){document.querySelectorAll("select[data-k^=Btn]").forEach(function(e){' +
    'var k=e.getAttribute("data-k");s[k]=D[k];e.value=D[k];});};' +
    'document.getElementById("save").onclick=function(){' +
    'var out={};for(var k in s)out[k]=s[k];out.ClearHistory=document.getElementById("clr").checked?1:0;' +
    'location.href="pebblejs://close#"+encodeURIComponent(JSON.stringify(out));};' +
    '</script></body></html>';
}

Pebble.addEventListener('ready', function () {});

Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL('data:text/html;charset=utf-8,' + encodeURIComponent(page(load())));
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;                 // closed without saving
  var got;
  try { got = JSON.parse(decodeURIComponent(e.response)); } catch (err) { return; }
  var s = load(), msg = {};
  for (var k in DEFAULTS) {
    if (k in got) s[k] = got[k] | 0;
    msg[k] = s[k];
  }
  msg.ClearHistory = got.ClearHistory ? 1 : 0;
  localStorage.setItem(KEY, JSON.stringify(s));
  Pebble.sendAppMessage(msg, function () {}, function () {
    Pebble.showSimpleNotificationOnPebble('TinySci', 'Settings not sent. Open the app on your watch and save again.');
  });
});
