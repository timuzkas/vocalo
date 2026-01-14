#include "httplib.h"
#include "template.hpp"
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <thread>
#include <cstdlib>
#include <map>

// OS-specific includes
#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <pwd.h>
#endif

using namespace std;
using namespace std::chrono;
namespace fs = std::filesystem;

// ==========================================
// 1. STATS PAGE CSS
// ==========================================
const string STATS_CSS = R"CSS(
    :root { --bg: #0d0d0d; --fg: #e0e0e0; --acc: #4af626; --sub: #666; }
    [data-theme="light"] { --bg: #f5f5f5; --fg: #1a1a1a; --acc: #00aa00; --sub: #999; }
    [data-theme="dracula"] { --bg: #282a36; --fg: #f8f8f2; --acc: #bd93f9; --sub: #6272a4; }
    [data-theme="nord"] { --bg: #2e3440; --fg: #d8dee9; --acc: #88c0d0; --sub: #4c566a; }
    [data-theme="monokai"] { --bg: #272822; --fg: #f8f8f2; --acc: #e6db74; --sub: #75715e; }
    [data-theme="amoled"] { --bg: #000; --fg: #fff; --acc: #fff; --sub: #444; }

    body { background: var(--bg); color: var(--fg); font-family: 'JetBrains Mono', monospace; padding: 40px; transition: 0.3s; }
    h1 { margin-top: 0; }
    .nav a { color: var(--sub); text-decoration: none; font-size: 1.1rem; border: 1px solid var(--sub); padding: 8px 16px; border-radius: 4px; }
    .nav a:hover { color: var(--fg); border-color: var(--fg); }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin: 40px 0; }
    .card { background: rgba(128,128,128,0.05); padding: 25px; border-radius: 8px; border: 1px solid rgba(128,128,128,0.1); }
    .val { font-size: 2.5rem; font-weight: bold; color: var(--acc); margin-top: 10px; }
    .lbl { color: var(--sub); text-transform: uppercase; font-size: 0.8rem; letter-spacing: 1px; }
    table { width: 100%; border-collapse: collapse; margin-top: 40px; }
    th { text-align: left; color: var(--sub); padding: 10px; border-bottom: 1px solid var(--sub); }
    td { padding: 12px 10px; border-bottom: 1px solid rgba(128,128,128,0.1); }
    .chart-box { background: rgba(128,128,128,0.05); padding: 20px; border-radius: 8px; height: 300px; }
)CSS";

// ==========================================
const string INDEX_HTML = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>type/c++</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Archivo:wght@400;600&family=Fira+Code:wght@400;600&family=IBM+Plex+Mono:wght@400;600&family=JetBrains+Mono:wght@400;600&family=Roboto+Mono:wght@400;600&family=Source+Code+Pro:wght@400;600&family=Space+Mono:wght@400;700&family=Inter:wght@400;600&display=swap" rel="stylesheet">

    <style>
        :root {
            --bg: #0d0d0d; --main: #e0e0e0; --sub: #555555;
            --acc: #4af626; --err: #ff2a2a;
            --cursor-glow: 0 0 10px rgba(74, 246, 38, 0.4);
            --font-mono: 'JetBrains Mono', monospace;
            --font-ui: 'Inter', sans-serif;
            --ease-cam: cubic-bezier(0.25, 1, 0.5, 1);
        }
        [data-theme="light"] { --bg: #ffffff; --main: #1a1a1a; --sub: #9ca3af; --acc: #2563eb; --err: #dc2626; --cursor-glow: 0 0 5px rgba(37, 99, 235, 0.3); }
        [data-theme="dracula"] { --bg: #282a36; --main: #f8f8f2; --sub: #6272a4; --acc: #bd93f9; --err: #ff5555; --cursor-glow: 0 0 10px rgba(189, 147, 249, 0.4); }
        [data-theme="nord"] { --bg: #2e3440; --main: #d8dee9; --sub: #4c566a; --acc: #88c0d0; --err: #bf616a; --cursor-glow: 0 0 10px rgba(136, 192, 208, 0.4); }
        [data-theme="monokai"] { --bg: #272822; --main: #f8f8f2; --sub: #75715e; --acc: #e6db74; --err: #f92672; --cursor-glow: 0 0 10px rgba(230, 219, 116, 0.4); }
        [data-theme="amoled"] { --bg: #000000; --main: #ffffff; --sub: #333333; --acc: #ffffff; --err: #ff0000; --cursor-glow: 0 0 10px rgba(255, 255, 255, 0.5); }

        * { box-sizing: border-box; transition: background-color 0.3s, color 0.3s, border-color 0.3s; }
        body { background-color: var(--bg); color: var(--main); font-family: var(--font-mono); margin: 0; height: 100vh; display: flex; flex-direction: column; align-items: center; overflow: hidden; }

        .header { width: 100%; max-width: 1100px; padding: 30px; display: grid; grid-template-columns: 150px 1fr 150px; align-items: center; z-index: 20; transition: opacity 0.3s, transform 0.3s; }
        body.focus-mode .header { opacity: 0; transform: translateY(-20px); pointer-events: none; }

        .logo { font-weight: 700; font-size: 1.4rem; display: flex; align-items: center; gap: 8px; color: var(--main); letter-spacing: -1px; }
        .logo span { color: var(--acc); }

        .config-bar { justify-self: center; display: flex; gap: 20px; font-family: var(--font-ui); font-size: 0.85rem; background: rgba(127,127,127,0.1); padding: 8px 16px; border-radius: 8px; backdrop-filter: blur(5px); }
        .cfg-group { display: flex; gap: 5px; align-items: center; }
        .cfg-btn { background: transparent; border: none; color: var(--sub); cursor: pointer; padding: 4px 8px; border-radius: 4px; font-family: inherit; transition: 0.2s; }
        .cfg-btn:hover { color: var(--main); }
        .cfg-btn.active { color: var(--bg); background: var(--acc); font-weight: 600; }
        .divider { width: 1px; height: 16px; background: var(--sub); opacity: 0.3; margin: 0 5px; }

        .select-wrapper { position: relative; display: flex; align-items: center; }
        select.cfg-btn { appearance: none; -webkit-appearance: none; background-color: transparent; padding-right: 20px; cursor: pointer; }
        .select-arrow { position: absolute; right: 0; top: 50%; transform: translateY(-50%); pointer-events: none; opacity: 0.5; width: 10px; height: 10px; }
        select.cfg-btn option { background-color: var(--bg); color: var(--main); }

        .header-right { display: flex; justify-content: flex-end; gap: 15px; align-items: center; position: relative; }
        .icon-btn { color: var(--sub); text-decoration: none; display: flex; align-items: center; transition: color 0.2s; cursor: pointer; background: none; border: none; opacity: 0; }
        .header:hover .icon-btn, body:not(.focus-mode) .icon-btn { opacity: 1; }
        .icon-btn:hover, .icon-btn.active { color: var(--acc); }

        .game-wrapper { flex: 1; width: 100%; position: relative; display: flex; flex-direction: column; justify-content: center; align-items: center; }
        .hud { display: flex; gap: 40px; font-size: 1.5rem; color: var(--acc); margin-bottom: 10px; font-weight: 700; height: 30px; transition: opacity 0.3s; }
        .zen-mode .hud { opacity: 0; }
        .hud .sub-info { font-size: 1rem; color: var(--sub); font-weight: 400; align-self: flex-end; margin-bottom: 3px; }

        .viewport { position: relative; width: 100%; max-width: 1000px; height: 140px; overflow: hidden; mask-image: linear-gradient(to bottom, transparent, black 15%, black 85%, transparent); -webkit-mask-image: linear-gradient(to bottom, transparent, black 15%, black 85%, transparent); }
        .words-list { position: relative; display: flex; flex-wrap: wrap; width: 100%; font-size: 1.7rem; line-height: 2.6rem; color: var(--sub); will-change: transform; transition: transform 0.15s var(--ease-cam); }
        .word { margin-right: 14px; margin-bottom: 10px; position: relative; white-space: nowrap; }
        .char { position: relative; }
        .char.correct { color: var(--main); }
        .char.incorrect { color: var(--err); }
        .cursor { position: absolute; width: 3px; height: 2rem; background: var(--acc); border-radius: 2px; box-shadow: var(--cursor-glow); top: 0; left: 0; pointer-events: none; z-index: 5; transition: transform 0.1s cubic-bezier(0.2, 0, 0, 1); }
        .typing .cursor { animation: none; opacity: 1; }
        @keyframes breathe { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }
        .cursor { animation: breathe 1.5s infinite ease-in-out; }

        .zen-mode .viewport { max-width: 100%; height: 100vh; mask-image: none; -webkit-mask-image: none; display: flex; align-items: center; justify-content: center; }
        .zen-mode .words-list { position: absolute; left: 50%; width: auto; flex-wrap: nowrap; font-size: 2.5rem; line-height: 1; align-items: center; transition: transform 0.12s var(--ease-cam); }
        .zen-mode .word { margin-right: 30px; margin-bottom: 0; }
        .zen-mode .cursor { height: 3rem; width: 4px; transition: none; }

        .popover { position: absolute; top: 100%; right: 0; margin-top: 10px; width: 320px; background: var(--bg); border: 1px solid var(--sub); border-radius: 8px; padding: 20px; box-shadow: 0 10px 30px rgba(0,0,0,0.5); z-index: 200; opacity: 0; pointer-events: none; transform: translateY(-10px); transition: all 0.2s ease; }
        .popover.show { opacity: 1; pointer-events: all; transform: translateY(0); }
        .set-row { display: flex; flex-direction: column; gap: 8px; margin-bottom: 15px; font-family: var(--font-ui); }
        .set-head { display: flex; justify-content: space-between; align-items: center; }
        .set-lbl { color: var(--sub); font-size: 0.85rem; font-weight: 600; text-transform: uppercase; }
        .set-ctrl select, .set-ctrl button, .set-ctrl input { background: rgba(127,127,127,0.1); border: 1px solid transparent; width: 100%; color: var(--main); padding: 8px 12px; border-radius: 6px; cursor: pointer; font-family: inherit; font-size: 0.9rem; text-align: left; }
        .set-ctrl select:hover, .set-ctrl button:hover { border-color: var(--sub); }
        .set-ctrl button.active { background: var(--acc); color: var(--bg); font-weight: 600; text-align: center; }
        .custom-text-area { width: 100%; height: 100px; background: rgba(0,0,0,0.2); border: 1px solid var(--sub); color: var(--main); font-family: var(--font-mono); font-size: 0.8rem; padding: 10px; border-radius: 6px; resize: vertical; }
        .custom-text-area:focus { outline: none; border-color: var(--acc); }

        .result-overlay { position: fixed; inset: 0; background: rgba(0,0,0,0.9); backdrop-filter: blur(10px); z-index: 100; display: flex; flex-direction: column; justify-content: center; align-items: center; opacity: 0; pointer-events: none; transition: opacity 0.2s; }
        .result-overlay.show { opacity: 1; pointer-events: all; }
        .res-wpm { font-size: 8rem; color: var(--acc); font-weight: 700; line-height: 1; }
        .res-grid { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 50px; margin-top: 40px; text-align: center; }
        .res-val { font-size: 2rem; font-weight: 600; color: var(--main); }
        .res-lbl { font-size: 0.9rem; color: var(--sub); text-transform: uppercase; letter-spacing: 1px; margin-top: 5px; }

        .btn-group { margin-top: 60px; display: flex; gap: 20px; }
        .btn { background: transparent; border: 1px solid var(--sub); color: var(--main); padding: 12px 30px; border-radius: 6px; cursor: pointer; font-size: 1rem; display: flex; align-items: center; gap: 10px; transition: 0.2s; }
        .btn:hover { border-color: var(--acc); color: var(--acc); background: rgba(255,255,255,0.05); }
        .btn-danger:hover { border-color: var(--err); color: var(--err); }
        .btn-danger.confirm { background: var(--err); color: #000; border-color: var(--err); }

        .footer { padding: 20px; color: var(--sub); font-size: 0.8rem; display: flex; gap: 20px; justify-content: center; transition: opacity 0.3s; }
        body.focus-mode .footer { opacity: 0.3; }
        .key { background: rgba(127,127,127,0.2); padding: 2px 6px; border-radius: 4px; color: var(--main); font-family: var(--font-ui); margin-right: 5px; }
    </style>
</head>
<body>

    <div class="header">
        <div class="logo">type<span>/</span>c++</div>

        <div class="config-bar">
            <div class="cfg-group" id="modeParams">
                <button class="cfg-btn" onclick="setMode('time')" data-mode="time">Time</button>
                <button class="cfg-btn" onclick="setMode('word')" data-mode="word">Word</button>
                <button class="cfg-btn" onclick="setMode('zen')" data-mode="zen">Zen</button>
            </div>
            <div class="divider"></div>
            <div class="cfg-group" id="lengthParams"></div>
            <div class="divider"></div>
            <div class="select-wrapper">
                <select class="cfg-btn" id="themeSelect" onchange="setTheme(this.value)">
                    <option value="default">Terminal</option>
                    <option value="light">Light</option>
                    <option value="dracula">Dracula</option>
                    <option value="nord">Nord</option>
                    <option value="monokai">Monokai</option>
                    <option value="amoled">Amoled</option>
                </select>
                <svg class="select-arrow" viewBox="0 0 24 24" fill="currentColor"><path d="M7 10l5 5 5-5z"/></svg>
            </div>
        </div>

        <div class="header-right">
            <button class="icon-btn" id="settingsBtn" title="Settings">
                <svg width="20" height="20" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" d="M10.325 4.317c.426-1.756 2.924-1.756 3.35 0a1.724 1.724 0 002.573 1.066c1.543-.94 3.31.826 2.37 2.37a1.724 1.724 0 001.065 2.572c1.756.426 1.756 2.924 0 3.35a1.724 1.724 0 00-1.066 2.573c.94 1.543-.826 3.31-2.37 2.37a1.724 1.724 0 00-2.572 1.065c-.426 1.756-2.924 1.756-3.35 0a1.724 1.724 0 00-2.573-1.066c-1.543.94-3.31-.826-2.37-2.37a1.724 1.724 0 00-1.065-2.572c-1.756-.426-1.756-2.924 0-3.35a1.724 1.724 0 001.066-2.573c-.94-1.543.826-3.31 2.37-2.37.996.608 2.296.07 2.572-1.065z"></path><path stroke-linecap="round" stroke-linejoin="round" d="M15 12a3 3 0 11-6 0 3 3 0 016 0z"></path></svg>
            </button>
            <a href="/stats" class="icon-btn" title="Stats">
                <svg width="20" height="20" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><path d="M9 19v-6a2 2 0 00-2-2H5a2 2 0 00-2 2v6a2 2 0 002 2h2a2 2 0 002-2zm0 0V9a2 2 0 012-2h2a2 2 0 012 2v10m-6 0a2 2 0 002 2h2a2 2 0 002-2m0 0V5a2 2 0 012-2h2a2 2 0 012 2v14a2 2 0 01-2 2h-2a2 2 0 01-2-2z"></path></svg>
            </a>

            <div class="popover" id="settingsPopover">
                <div class="set-row">
                    <div class="set-lbl">Word Set</div>
                    <div class="set-ctrl">
                        <select id="wordSetSelect" onchange="setWordSet(this.value)">
                            <optgroup label="Built-in">
                                <option value="cpp">C++ / Programming</option>
                                <option value="english_200">English 200</option>
                                <option value="oxford_3000">Oxford 3k</option>
                                <option value="quotes">Famous Quotes</option>
                            </optgroup>
                            <optgroup label="Custom" id="customBanksGroup" style="display:none;">
                            </optgroup>
                            <option value="__custom__">+ Create New</option>
                        </select>
                    </div>
                </div>
                <div class="set-row" id="customBankEditor" style="display:none;">
                    <input type="text" id="bankName" class="custom-text-area" placeholder="Word bank name..." style="height:40px; margin-bottom:10px;">
                    <textarea id="customText" class="custom-text-area" placeholder="Paste your words here (space or newline separated)..." style="display:block;"></textarea>
                    <div style="display:flex; gap:10px; margin-top:10px;">
                        <button class="set-ctrl" onclick="saveCustomBank()" style="background:var(--acc); color:var(--bg); flex:1;">Save Bank</button>
                        <button class="set-ctrl" onclick="cancelCustomBank()" style="flex:1;">Cancel</button>
                        <button class="set-ctrl btn-danger" onclick="deleteCustomBank()" style="flex:0; padding:8px 16px;" id="deleteBankBtn">Delete</button>
                    </div>
                </div>
                <div class="set-row">
                    <div class="set-lbl">Font Family</div>
                    <div class="set-ctrl">
                        <select id="fontSelect" onchange="setFont(this.value)">
                            <option value="'JetBrains Mono', monospace">JetBrains Mono</option>
                            <option value="'Archivo', sans-serif">Archivo</option>
                            <option value="'Roboto Mono', monospace">Roboto Mono</option>
                            <option value="'Fira Code', monospace">Fira Code</option>
                            <option value="'Source Code Pro', monospace">Source Code Pro</option>
                            <option value="'Space Mono', monospace">Space Mono</option>
                            <option value="'IBM Plex Mono', monospace">IBM Plex Mono</option>
                        </select>
                    </div>
                </div>
                <div class="set-row">
                    <div class="set-head">
                        <div class="set-lbl">Sound</div>
                        <div class="set-ctrl" style="width: auto;">
                            <button id="soundToggle" onclick="toggleSound()" style="padding: 4px 10px; width: 60px;">Off</button>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <div class="game-wrapper">
        <div class="hud" id="hud">
            <div id="counter">60</div>
            <div class="sub-info"><span id="liveWpm">0</span> wpm</div>
        </div>

        <div class="viewport" id="viewport">
            <div class="words-list" id="wordList">
                <div class="cursor" id="cursor"></div>
            </div>
        </div>
    </div>

    <div class="footer">
        <div><span class="key">tab</span> restart</div>
        <div><span class="key">esc</span> pause</div>
        <div><span class="key">ctrl+bs</span> delete</div>
        <div><span class="key">shift+F</span> zen</div>
    </div>

    <div class="result-overlay" id="results">
        <div class="res-wpm" id="finalWpm">0</div>
        <div class="res-grid">
            <div><div class="res-val" id="finalAcc">0%</div><div class="res-lbl">accuracy</div></div>
            <div><div class="res-val" id="finalChars">0/0</div><div class="res-lbl">chars</div></div>
            <div><div class="res-val" id="finalMode">time</div><div class="res-lbl">mode</div></div>
        </div>
        <div class="btn-group">
            <button class="btn" onclick="resetGame()">Next Test (Tab)</button>
            <button class="btn btn-danger" id="delBtn" onclick="deleteStat()">Delete (2x)</button>
        </div>
    </div>

    <script>
        const config = {
            mode: localStorage.getItem('mode') || 'time',
            length: parseInt(localStorage.getItem('length')) || 60,
            theme: localStorage.getItem('theme') || 'default',
            font: localStorage.getItem('font') || "'JetBrains Mono', monospace",
            sound: localStorage.getItem('sound') === 'true',
            wordSet: localStorage.getItem('wordSet') || 'cpp'
        };

        let customBanks = JSON.parse(localStorage.getItem('customBanks') || '{}');
        let editingBank = null;

        const state = {
            words: [], wordEls: [], wordIdx: 0, charIdx: 0,
            isPlaying: false, startTime: 0, timer: null, timerVal: 0,
            stats: { correct: 0, incorrect: 0 }
        };

        const els = {
            list: document.getElementById('wordList'),
            cursor: document.getElementById('cursor'),
            counter: document.getElementById('counter'),
            wpm: document.getElementById('liveWpm'),
            hud: document.getElementById('hud'),
            results: document.getElementById('results'),
            settingsBtn: document.getElementById('settingsBtn'),
            popover: document.getElementById('settingsPopover')
        };

        const audioCtx = new (window.AudioContext || window.webkitAudioContext)();

        function setTheme(name) {
            document.documentElement.setAttribute('data-theme', name);
            localStorage.setItem('theme', name);
            document.getElementById('themeSelect').value = name;
        }

        function setFont(font) {
            document.documentElement.style.setProperty('--font-mono', font);
            localStorage.setItem('font', font);
            config.font = font;
            document.getElementById('fontSelect').value = font;
            requestAnimationFrame(() => updateCursor());
        }

        function toggleSound() {
            config.sound = !config.sound;
            localStorage.setItem('sound', config.sound);
            updateSettingsUI();
            if(config.sound && audioCtx.state === 'suspended') audioCtx.resume();
        }

        function setWordSet(val) {
            if (val === '__custom__') {
                editingBank = null;
                document.getElementById('customBankEditor').style.display = 'block';
                document.getElementById('bankName').value = '';
                document.getElementById('customText').value = '';
                document.getElementById('deleteBankBtn').style.display = 'none';
            } else if (val.startsWith('custom_')) {
                const bankName = val.substring(7);
                editingBank = bankName;
                document.getElementById('customBankEditor').style.display = 'block';
                document.getElementById('bankName').value = bankName;
                document.getElementById('customText').value = customBanks[bankName] || '';
                document.getElementById('deleteBankBtn').style.display = 'block';
            } else {
                document.getElementById('customBankEditor').style.display = 'none';
                config.wordSet = val;
                localStorage.setItem('wordSet', val);
                resetGame();
            }
        }

        function saveCustomBank() {
            const name = document.getElementById('bankName').value.trim();
            const text = document.getElementById('customText').value.trim();

            if (!name) {
                alert('Please enter a bank name');
                return;
            }
            if (!text) {
                alert('Please enter some words');
                return;
            }

            customBanks[name] = text;
            localStorage.setItem('customBanks', JSON.stringify(customBanks));

            fetch('/api/save-wordbank', {
                method: 'POST',
                body: JSON.stringify({ name, words: text })
            });

            config.wordSet = 'custom_' + name;
            localStorage.setItem('wordSet', config.wordSet);

            updateSettingsUI();
            document.getElementById('customBankEditor').style.display = 'none';
            resetGame();
        }

        function cancelCustomBank() {
            document.getElementById('customBankEditor').style.display = 'none';
            document.getElementById('wordSetSelect').value = config.wordSet;
        }

        function deleteCustomBank() {
            if (!editingBank) return;

            if (!confirm(`Delete word bank "${editingBank}"?`)) return;

            delete customBanks[editingBank];
            localStorage.setItem('customBanks', JSON.stringify(customBanks));

            fetch('/api/delete-wordbank', {
                method: 'DELETE',
                body: JSON.stringify({ name: editingBank })
            });

            config.wordSet = 'cpp';
            localStorage.setItem('wordSet', config.wordSet);

            document.getElementById('customBankEditor').style.display = 'none';
            updateSettingsUI();
            resetGame();
        }

        function updateSettingsUI() {
            const sBtn = document.getElementById('soundToggle');
            sBtn.textContent = config.sound ? "On" : "Off";
            sBtn.classList.toggle('active', config.sound);

            document.getElementById('fontSelect').value = config.font;

            const select = document.getElementById('wordSetSelect');
            const customGroup = document.getElementById('customBanksGroup');

            customGroup.innerHTML = '';
            const hasBanks = Object.keys(customBanks).length > 0;
            customGroup.style.display = hasBanks ? 'block' : 'none';

            for (const [name, _] of Object.entries(customBanks)) {
                const opt = document.createElement('option');
                opt.value = 'custom_' + name;
                opt.textContent = name;
                customGroup.appendChild(opt);
            }

            select.value = config.wordSet;
        }

        els.settingsBtn.addEventListener('click', (e) => {
            e.stopPropagation();
            els.popover.classList.toggle('show');
            els.settingsBtn.classList.toggle('active');
        });

        document.addEventListener('click', (e) => {
            if(!els.popover.contains(e.target) && e.target !== els.settingsBtn) {
                els.popover.classList.remove('show');
                els.settingsBtn.classList.remove('active');
            }
        });

        function setMode(mode) {
            config.mode = mode;
            localStorage.setItem('mode', mode);
            updateConfigUI();
            resetGame();
        }

        function setLength(val) {
            config.length = val;
            localStorage.setItem('length', val);
            updateConfigUI();
            resetGame();
        }

        function updateConfigUI() {
            document.querySelectorAll('#modeParams .cfg-btn').forEach(b => {
                b.classList.toggle('active', b.dataset.mode === config.mode);
            });
            const lenContainer = document.getElementById('lengthParams');
            lenContainer.innerHTML = '';
            let options = [];
            if(config.mode === 'time') options = [15, 30, 60, 120];
            else if(config.mode === 'word') options = [10, 25, 50, 100];

            if(options.length > 0) {
                options.forEach(opt => {
                    const btn = document.createElement('button');
                    btn.className = `cfg-btn ${config.length === opt ? 'active' : ''}`;
                    btn.textContent = opt;
                    btn.onclick = () => setLength(opt);
                    lenContainer.appendChild(btn);
                });
            } else {
                lenContainer.innerHTML = '<span style="color:var(--sub);font-size:0.8em">∞</span>';
            }
        }

        function shuffle(array) {
            for (let i = array.length - 1; i > 0; i--) {
                const j = Math.floor(Math.random() * (i + 1));
                [array[i], array[j]] = [array[j], array[i]];
            }
            return array;
        }

        async function fetchWords(count) {
            if(config.wordSet.startsWith('custom_')) {
                const bankName = config.wordSet.substring(7);
                const raw = customBanks[bankName] || '';
                if(!raw.trim()) return ["please", "add", "words", "to", "custom", "bank"];
                let w = raw.split(/\s+/).filter(x => x.length > 0);
                let res = [];
                while(res.length < count) {
                    let chunk = [...w];
                    shuffle(chunk);
                    res = res.concat(chunk);
                }
                return res.slice(0, count);
            }
            try {
                const response = await fetch(`/api/words?count=${count}&bank=${config.wordSet}`);
                return await response.json();
            }
            catch { return ["error", "offline", "retry"]; }
        }

        async function initGame() {
            if(config.mode === 'zen') document.body.classList.add('zen-mode');
            else document.body.classList.remove('zen-mode');

            const count = (config.mode === 'word') ? config.length + 5 : 50;
            state.words = await fetchWords(count);
            renderWords();

            state.wordIdx = 0; state.charIdx = 0;
            state.stats = { correct: 0, incorrect: 0 };
            state.isPlaying = false;
            clearInterval(state.timer);

            els.results.classList.remove('show');
            document.body.classList.remove('focus-mode');
            els.list.classList.remove('typing');

            if(config.mode === 'zen') els.list.style.transform = `translateX(0px)`;
            else els.list.style.transform = `translateY(55px)`;

            if(config.mode === 'time') els.counter.textContent = config.length;
            else if(config.mode === 'word') els.counter.textContent = "0 / " + config.length;
            else els.counter.textContent = "";
            els.wpm.textContent = 0;

            requestAnimationFrame(() => updateCursor());
        }

        function renderWords() {
            els.list.innerHTML = '';
            els.list.appendChild(els.cursor);
            state.wordEls = state.words.map(w => {
                const d = document.createElement('div');
                d.className = 'word';
                w.split('').forEach(c => {
                    const s = document.createElement('span');
                    s.className = 'char';
                    s.textContent = c;
                    d.appendChild(s);
                });
                els.list.appendChild(d);
                return d;
            });
        }

        function startGame() {
            if(state.isPlaying) return;
            state.isPlaying = true;
            state.startTime = Date.now();
            els.list.classList.add('typing');
            document.body.classList.add('focus-mode');
            els.popover.classList.remove('show');

            if(config.mode === 'time') {
                state.timerVal = config.length;
                state.timer = setInterval(() => {
                    state.timerVal--;
                    els.counter.textContent = state.timerVal;
                    if(state.timerVal <= 0) endGame();
                    updateStats();
                }, 1000);
            } else {
                state.timer = setInterval(updateStats, 1000);
            }
        }

        function playSound(freq, type) {
            if(!config.sound) return;
            const osc = audioCtx.createOscillator();
            const gain = audioCtx.createGain();
            osc.frequency.value = freq;
            osc.type = type;
            gain.gain.setValueAtTime(0.05, audioCtx.currentTime);
            gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + 0.05);
            osc.connect(gain);
            gain.connect(audioCtx.destination);
            osc.start();
            osc.stop(audioCtx.currentTime + 0.05);
        }

        function updateStats() {
            const elapsed = (Date.now() - state.startTime) / 1000 / 60;
            if(elapsed > 0) els.wpm.textContent = Math.round((state.stats.correct/5) / elapsed);
        }

        function endGame() {
            state.isPlaying = false;
            clearInterval(state.timer);
            document.body.classList.remove('focus-mode');

            const elapsed = (Date.now() - state.startTime) / 1000 / 60;
            const wpm = Math.round((state.stats.correct/5) / elapsed) || 0;
            const total = state.stats.correct + state.stats.incorrect;
            const acc = total ? Math.round((state.stats.correct / total) * 100) : 0;

            document.getElementById('finalWpm').textContent = wpm;
            document.getElementById('finalAcc').textContent = acc + '%';
            document.getElementById('finalChars').textContent = `${state.stats.correct}/${state.stats.incorrect}`;
            document.getElementById('finalMode').textContent = config.mode;

            els.results.classList.add('show');

            if(total > 5) {
                fetch('/api/save-stats', {
                    method: 'POST',
                    body: JSON.stringify({ wpm, accuracy: acc, words: state.wordIdx, mode: config.mode })
                });
            }
        }

        function updateCursor() {
            const curWord = state.wordEls[state.wordIdx];
            if(!curWord) return;

            const chars = curWord.children;
            const target = (state.charIdx < chars.length) ? chars[state.charIdx] : chars[chars.length-1];

            const wordLeft = curWord.offsetLeft;
            const charLeft = target.offsetLeft + (state.charIdx >= chars.length ? target.offsetWidth : 0);
            const totalLeft = wordLeft + charLeft;

            const wordTop = curWord.offsetTop;
            const charTop = target.offsetTop;
            const totalTop = wordTop + charTop;

            if(config.mode === 'zen') {
                els.cursor.style.transform = `translate(${totalLeft}px, 0)`;
                els.list.style.transform = `translateX(-${totalLeft}px)`;
            } else {
                els.cursor.style.transform = `translate(${totalLeft}px, ${totalTop + 5}px)`;
                els.list.style.transform = `translateY(${55 - wordTop}px)`;
            }
        }

        async function checkInfiniteScroll() {
            if(state.wordIdx > state.words.length - 15) {
                const newWords = await fetchWords(25);
                state.words = state.words.concat(newWords);
                newWords.forEach(w => {
                    const d = document.createElement('div');
                    d.className = 'word';
                    w.split('').forEach(c => {
                        const s = document.createElement('span');
                        s.className = 'char';
                        s.textContent = c;
                        d.appendChild(s);
                    });
                    els.list.appendChild(d);
                    state.wordEls.push(d);
                });
            }
        }

        document.addEventListener('keydown', e => {
            if(e.key === 'Tab') { e.preventDefault(); resetGame(); return; }
            if(e.key === 'Escape') { if(state.isPlaying) endGame(); return; }
            if(e.key === 'F' && e.shiftKey) { setMode(config.mode==='zen'?'time':'zen'); return; }

            if(els.results.classList.contains('show') || els.popover.classList.contains('show')) {
                if(e.key === 'Escape') els.popover.classList.remove('show');
                if(e.ctrlKey && e.key === 'Backspace' && els.results.classList.contains('show')) deleteStat();
                return;
            }

            if(!state.isPlaying && e.key.length===1 && !e.ctrlKey && !e.metaKey) startGame();
            if(!state.isPlaying) return;

            const curWord = state.words[state.wordIdx];
            const curEl = state.wordEls[state.wordIdx];

            if(e.key === ' ') {
                e.preventDefault();
                for(let i=state.charIdx; i<curWord.length; i++) {
                    curEl.children[i].classList.add('incorrect');
                    state.stats.incorrect++;
                }
                state.wordIdx++;
                state.charIdx = 0;
                playSound(600, 'sine');

                if(config.mode === 'word' && state.wordIdx >= config.length) endGame();
                checkInfiniteScroll();
            }
            else if(e.key === 'Backspace') {
                if(state.charIdx > 0) {
                    state.charIdx--;
                    const char = curEl.children[state.charIdx];
                    if(char.classList.contains('correct')) state.stats.correct--;
                    if(char.classList.contains('incorrect')) state.stats.incorrect--;
                    char.className = 'char';
                }
            }
            else if(e.key.length === 1 && !e.ctrlKey && !e.metaKey) {
                e.preventDefault();
                if(state.charIdx < curWord.length) {
                    const charEl = curEl.children[state.charIdx];
                    if(e.key === curWord[state.charIdx]) {
                        charEl.classList.add('correct');
                        state.stats.correct++;
                        playSound(800, 'sine');
                    } else {
                        charEl.classList.add('incorrect');
                        state.stats.incorrect++;
                        playSound(150, 'sawtooth');
                    }
                    state.charIdx++;
                } else state.stats.incorrect++;
            }
            updateCursor();
        });

        function resetGame() { initGame(); }

        let delClick = 0;
        function deleteStat() {
            delClick++;
            const btn = document.getElementById('delBtn');
            if(delClick===1) {
                btn.classList.add('confirm');
                btn.innerHTML = "Confirm?";
                setTimeout(() => { delClick=0; btn.classList.remove('confirm'); btn.innerHTML = 'Delete (2x)'; }, 2000);
            } else {
                fetch('/api/delete-last-stat', {method:'DELETE'}).then(()=>resetGame());
            }
        }

        // Init
        setTheme(config.theme);
        setFont(config.font);
        updateConfigUI();
        updateSettingsUI();
        initGame();

    </script>
</body>
</html>
)HTML";

// ==========================================

fs::path get_data_directory() {
    fs::path app_dir;
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
        app_dir = fs::path(path) / "type_cpp";
    } else {
        app_dir = fs::current_path();
    }
#elif __APPLE__
    const char* home = getenv("HOME");
    if (home) {
        app_dir = fs::path(home) / "Library" / "Application Support" / "type_cpp";
    }
#else
    const char* xdg_data = getenv("XDG_DATA_HOME");
    if (xdg_data) {
        app_dir = fs::path(xdg_data) / "type_cpp";
    } else {
        const char* home = getenv("HOME");
        if (home) {
            app_dir = fs::path(home) / ".local" / "share" / "type_cpp";
        } else {
            app_dir = fs::current_path();
        }
    }
#endif
    if (!fs::exists(app_dir)) fs::create_directories(app_dir);
    return app_dir;
}

void open_browser(const string& url) {
#ifdef _WIN32
    string cmd = "start " + url;
#elif __APPLE__
    string cmd = "open " + url;
#else
    string cmd = "xdg-open " + url;
#endif
    system(cmd.c_str());
}

// ==========================================

map<string, vector<string>> WORD_BANKS = {
    {"cpp", {
        "algorithm", "function", "variable", "compile", "interface", "syntax", "runtime", "pointer",
        "stack", "heap", "memory", "binary", "recursive", "iteration", "thread", "mutex", "server",
        "client", "protocol", "request", "response", "header", "payload", "database", "encryption",
        "decryption", "optimization", "abstraction", "python", "javascript", "assembly", "compiler",
        "debugger", "framework", "library", "package", "module", "import", "export", "async", "await",
        "promise", "callback", "event", "listener", "array", "object", "class", "method", "inheritance",
        "polymorphism", "void", "null", "undefined", "integer", "float", "double", "char", "struct",
        "union", "enum", "const", "static", "virtual", "override", "lambda", "template", "namespace",
        "cout", "cin", "vector", "map", "string", "auto", "bool", "catch", "throw", "try", "delete", "new"
    }},
    {"english_200", {
        "the", "be", "to", "of", "and", "a", "in", "that", "have", "I", "it", "for", "not", "on", "with",
        "he", "as", "you", "do", "at", "this", "but", "his", "by", "from", "they", "we", "say", "her", "she",
        "or", "an", "will", "my", "one", "all", "would", "there", "their", "what", "so", "up", "out", "if",
        "about", "who", "get", "which", "go", "me", "when", "make", "can", "like", "time", "no", "just",
        "him", "know", "take", "people", "into", "year", "your", "good", "some", "could", "them", "see",
        "other", "than", "then", "now", "look", "only", "come", "its", "over", "think", "also", "back",
        "after", "use", "two", "how", "our", "work", "first", "well", "way", "even", "new", "want", "because",
        "any", "these", "give", "day", "most", "us", "is", "was", "are", "been", "has", "had", "were", "said",
        "did", "having", "may", "should", "am", "being", "been", "might", "must", "shall", "can", "could",
        "would", "will", "may", "ought", "dare", "need", "used", "let", "help", "make", "see", "hear", "feel",
        "watch", "notice", "observe", "find", "keep", "leave", "start", "begin", "continue", "stop", "finish",
        "end", "seem", "appear", "become", "remain", "stay", "turn", "grow", "fall", "rise", "stand", "sit",
        "lie", "walk", "run", "jump", "fly", "swim", "drive", "ride", "eat", "drink", "sleep", "wake", "live",
        "die", "kill", "save", "love", "hate", "like", "want", "need", "hope", "wish", "expect", "believe",
        "think", "know", "understand", "remember", "forget", "learn", "teach", "study", "read", "write",
        "speak", "talk", "say", "tell", "ask", "answer", "call", "name", "mean", "explain", "describe"
    }},
    {"oxford_3000", {
        "a", "abandon", "abandoned", "ability", "able", "about", "above", "abroad", "absence", "absent",
        "absolute", "absolutely", "absorb", "abuse", "academic", "accent", "accept", "acceptable", "access", "accident",
        "accidental", "accidentally", "accommodation", "accompany", "according to", "account", "account for", "accurate", "accurately", "accuse",
        "achieve", "achievement", "acid", "acknowledge", "a couple", "acquire", "across", "act", "action", "active",
        "actively", "activity", "actor", "actress", "actual", "actually", "ad", "adapt", "add", "addition",
        "additional", "add on", "address", "add up", "add up to", "adequate", "adequately", "adjust", "admiration", "admire",
        "admit", "adopt", "adult", "advance", "advanced", "advantage", "adventure", "advert", "advertise", "advertisement",
        "advertising", "advice", "advise", "affair", "affect", "affection", "afford", "afraid", "after", "afternoon",
        "afterwards", "again", "against", "age", "aged", "agency", "agent", "aggressive", "ago", "agree",
        "agreement", "ahead", "aid", "aim", "air", "aircraft", "airport", "alarm", "alarmed", "alarming",
        "alcohol", "alcoholic", "alive", "all", "allied", "allow", "allow for", "all right", "ally", "almost",
        "alone", "along", "alongside", "aloud", "alphabet", "alphabetical", "alphabetically", "already", "also", "alter",
        "alternative", "alternatively", "although", "altogether", "always", "a.m.", "amaze", "amazed", "amazing", "ambition",
        "ambulance", "among", "amount", "amount to", "amuse", "amused", "amusing", "analyse", "analysis", "ancient",
        "and", "anger", "angle", "angrily", "angry", "animal", "ankle", "anniversary", "announce", "annoy",
        "annoyed", "annoying", "annual", "annually", "another", "answer", "anti-", "anticipate", "anxiety", "anxious",
        "anxiously", "any", "anybody", "anyone", "anything", "anyway", "anywhere", "apart", "apart from", "apartment",
        "apologize", "apparent", "apparently", "appeal", "appear", "appearance", "apple", "application", "apply", "appoint",
        "appointment", "appreciate", "approach", "appropriate", "approval", "approve", "approving", "approximate", "approximately", "April",
        "area", "argue", "argument", "arise", "arm", "armed", "arms", "army", "around", "arrange",
        "arrangement", "arrest", "arrival", "arrive", "arrive at", "arrow", "art", "article", "artificial", "artificially",
        "artist", "artistic", "artistically", "as", "ashamed", "aside", "aside from", "ask", "asleep", "aspect",
        "assist", "assistance", "assistant", "associate", "associated", "association", "assume", "assure", "at", "atmosphere",
        "atom", "attach", "attached", "attack", "attempt", "attempted", "attend", "attend to", "attention", "attitude",
        "attorney", "attract", "attraction", "attractive", "audience", "August", "aunt", "author", "authority", "automatic",
        "automatically", "autumn", "available", "average", "avoid", "awake", "award", "aware", "away", "awful",
        "awfully", "awkward", "awkwardly", "baby", "back", "background", "back up", "backward", "backwards", "bacteria",
        "bad", "badly", "bad-tempered", "bag", "baggage", "bake", "balance", "ball", "ban", "band",
        "bandage", "bank", "bar", "bargain", "barrier", "base", "based", "base on", "basic", "basically",
        "basis", "bath", "bathroom", "battery", "battle", "bay", "be", "beach", "beak", "bear",
        "beard", "beat", "beat up", "beautiful", "beautifully", "beauty", "because", "because of", "become", "bed",
        "bedroom", "beef", "beer", "before", "begin", "beginning", "behalf", "behave", "behaviour", "behind",
        "be left over", "belief", "believe", "believe in", "bell", "belong", "belong to", "below", "belt",
        "bend", "beneath", "benefit", "bent", "beside", "best", "bet", "better", "betting", "between",
        "beyond", "bicycle", "bid 1", "big", "bike", "bill", "billion", "bin", "biology", "bird",
        "birth", "birthday", "biscuit", "bit", "bite", "bitter", "bitterly", "black", "blade", "blame",
        "blank", "blind", "block", "blonde", "blood", "blow", "blow out", "blow up", "blue", "board",
        "boat", "body", "boil", "bomb", "bone", "book", "boot", "border", "bore", "bored", "boring",
        "born", "borrow", "boss", "both", "bother", "bottle", "bottom", "bound", "bowl", "box",
        "boy", "boyfriend", "brain", "branch", "brand", "brave", "bread", "break", "break down", "breakfast",
        "break in", "break into", "break off", "break out", "break up", "breast", "breath", "breathe", "breathe in",
        "breathe out", "breathing", "breed", "brick", "bridge", "brief", "briefly", "bright", "brightly", "brilliant",
        "bring", "bring back", "bring down", "bring forward", "bring out", "bring up", "broad", "broadcast", "broadly",
        "broken", "brother", "brown", "brush", "bubble", "budget", "build", "building", "build up", "bullet",
        "bunch", "burn", "burn down", "burnt", "burst", "burst into", "burst out", "bury", "bus",
        "bush", "business", "businessman", "busy", "but", "butter", "button", "buy", "buyer", "by",
        "bye", "cabinet", "cable", "cake", "calculate", "calculation", "call", "call back", "called", "call for",
        "call off", "call up", "calm", "calm down", "calmly", "camera", "camp", "campaign", "camping",
        "can 1", "can 2", "cancel", "cancer", "candidate", "candy", "cannot", "cap", "capable", "capacity",
        "capital", "captain", "capture", "car", "card", "cardboard", "care", "career", "care for", "careful",
        "carefully", "careless", "carelessly", "carpet", "carrot", "carry", "carry on", "carry out", "case", "cash",
        "cast", "castle", "cat", "catch", "catch up", "category", "cause", "CD", "cease", "ceiling",
        "celebrate", "celebration", "cell", "cell phone", "cent", "centimetre", "central", "centre", "century", "ceremony",
        "certain", "certainly", "certificate", "chain", "chair", "chairman", "chairwoman", "challenge", "chamber", "chance",
        "change", "change round", "channel", "chapter", "character", "characteristic", "charge", "charity", "chart", "chase",
        "chase away", "chat", "cheap", "cheaply", "cheat", "cheat of", "check", "check in", "check into", "check on",
        "check out", "check over", "check up on", "cheek", "cheerful", "cheerfully", "cheese", "chemical", "chemist",
        "chemistry", "cheque", "chest", "chew", "chicken", "chief", "child", "chin", "chip", "chocolate",
        "choice", "choose", "chop", "chop down", "chop off", "church", "cigarette", "cinema", "circle", "circumstance",
        "citizen", "city", "civil", "claim", "clap", "class", "classic", "classroom", "clean", "clean up",
        "clear", "clearly", "clear out", "clear up", "clerk", "clever", "click", "client", "climate", "climb",
        "climbing", "clock", "close 1", "close 2", "closed", "closely", "closet", "cloth", "clothes", "clothing",
        "cloud", "club", "coach", "coal", "coast", "coat", "code", "coffee", "coin", "cold", "coldly",
        "collapse", "colleague", "collect", "collection", "college", "colour", "coloured", "column", "combination", "combine",
        "come", "come across", "come down", "comedy", "come from", "come in", "come off", "come on", "come out",
        "come round", "come to", "come up", "come up to", "comfort", "comfortable", "comfortably", "command", "comment", "commercial",
        "commission", "commit", "commitment", "committee", "common", "commonly", "communicate", "communication", "community", "company",
        "compare", "comparison", "compete", "competition", "competitive", "complain", "complaint", "complete", "completely", "complex",
        "complicate", "complicated", "computer", "concentrate", "concentrate on", "concentration", "concept", "concern", "concerned", "concerning",
        "concert", "conclude", "conclusion", "concrete", "condition", "conduct", "conference", "confidence", "confident", "confidently",
        "confine", "confined", "confirm", "conflict", "confront", "confuse", "confused", "confusing", "confusion", "congratulate",
        "congratulation", "congress", "connect", "connected", "connection", "conscious", "consequence", "conservative", "consider", "considerable",
        "considerably", "consideration", "consist", "consist of", "constant", "constantly", "construct", "construction", "consult", "consumer",
        "contact", "contain", "container", "contemporary", "content 1", "contest", "context", "continent", "continue", "continuous",
        "continuously", "contract", "contrast", "contrasting", "contribute", "contribution", "control", "controlled", "convenient", "convention",
        "conventional", "conversation", "convert", "convince", "cook", "cooker", "cookie", "cooking", "cool", "cool down",
        "cope", "copy", "core", "corner", "correct", "correctly", "cost", "cottage", "cotton", "cough", "coughing",
        "could", "council", "count", "counter", "count on", "country", "countryside", "county", "couple", "courage",
        "course", "court", "cousin", "cover", "covered", "covering", "cover up", "cow", "crack", "cracked",
        "craft", "crash", "crazy", "cream", "create", "creature", "credit", "credit card", "crime", "criminal",
        "crisis", "crisp", "criterion", "critical", "criticism", "criticize", "crop", "cross", "cross out", "crowd",
        "crowded", "crown", "crucial", "cruel", "crush", "cry", "cry out", "cultural", "culture", "cup",
        "cupboard", "curb", "cure", "curious", "curiously", "curl", "curl up", "curly", "current", "currently",
        "curtain", "curve", "curved", "custom", "customer", "customs", "cut", "cut back", "cut down", "cut off",
        "cut out", "cut up", "cycle", "cycling", "dad", "daily", "damage", "damp", "dance", "dancer",
        "dancing", "danger", "dangerous", "dare", "dark", "data", "date", "date back", "daughter", "day",
        "dead", "deaf", "deal", "deal in", "deal with", "dear", "death", "debate", "debt", "decade",
        "decay", "December", "decide", "decide on", "decision", "declare", "decline", "decorate", "decoration", "decorative",
        "decrease", "deep", "deeply", "defeat", "defence", "defend", "define", "definite", "definitely", "definition",
        "degree", "delay", "deliberate", "deliberately", "delicate", "delight", "delighted", "deliver", "delivery", "demand",
        "demonstrate", "dentist", "deny", "department", "departure", "depend", "depend on", "deposit", "depress", "depressed",
        "depressing", "depth", "derive", "derive from", "describe", "description", "desert", "deserted", "deserve", "design",
        "desire", "desk", "desperate", "desperately", "despite", "destroy", "destruction", "detail", "detailed", "determination",
        "determine", "determined", "develop", "development", "device", "devote", "devoted", "devote to", "diagram", "diamond",
        "diary", "dictionary", "die", "die away", "die out", "diet", "difference", "different", "differently", "difficult",
        "difficulty", "dig", "digital", "dinner", "direct", "direction", "directly", "director", "dirt", "dirty",
        "disabled", "disadvantage", "disagree", "disagreement", "disagree with doing", "disappear", "disappoint", "disappointed", "disappointing", "disappointment",
        "disapproval", "disapprove", "disapproving", "disaster", "disc", "discipline", "discount", "discover", "discovery", "discuss",
        "discussion", "disease", "disgust", "disgusted", "disgusting", "dish", "dishonest", "dishonestly", "disk", "dislike",
        "dismiss", "display", "dissolve", "distance", "distinguish", "distribute", "distribution", "district", "disturb", "disturbing",
        "divide", "division", "divorce", "divorced", "do 1", "doctor", "document", "dog", "dollar", "domestic",
        "dominate", "door", "dot", "double", "doubt", "do up", "do with", "do without", "down", "downstairs",
        "downward", "downwards", "dozen", "draft", "drag", "drama", "dramatic", "dramatically", "draw", "drawer",
        "drawing", "dream", "dress", "dressed", "dress up", "drink", "drive", "drive away", "drive off", "driver",
        "driving", "drop", "drop out", "drug", "drugstore", "drum", "drunk", "dry", "dry off", "dry up",
        "due", "dull", "dump", "during", "dust", "duty", "DVD", "dying", "each", "each other",
        "ear", "early", "earn", "earth", "ease", "easily", "east", "eastern", "easy", "eat",
        "eat out", "eat up", "economic", "economy", "edge", "edition", "editor", "educate", "educated", "education",
        "effect", "effective", "effectively", "efficient", "efficiently", "effort", "e.g.", "egg", "eight", "eighteen",
        "eighteenth", "eighth", "eightieth", "eighty", "either", "elbow", "elderly", "elect", "election", "electric",
        "electrical", "electricity", "electronic", "elegant", "element", "elevator", "eleven", "eleventh", "else", "elsewhere",
        "email", "embarrass", "embarrassed", "embarrassing", "embarrassment", "emerge", "emergency", "emotion", "emotional", "emotionally",
        "emphasis", "emphasize", "empire", "employ", "employee", "employer", "employment", "empty", "enable", "encounter",
        "encourage", "encouragement", "end", "end in", "ending", "end up", "enemy", "energy", "engage", "engaged",
        "engine", "engineer", "engineering", "enjoy", "enjoyable", "enjoyment", "enormous", "enough", "enquiry", "ensure",
        "enter", "entertain", "entertainer", "entertaining", "entertainment", "enthusiasm", "enthusiastic", "enthusiastically", "entire", "entirely",
        "entitle", "entrance 1", "entry", "envelope", "environment", "environmental", "equal", "equally", "equipment", "equivalent",
        "error", "escape", "especially", "essay", "essential", "essentially", "establish", "estate", "estimate", "etc.",
        "euro", "even", "evening", "event", "eventually", "ever", "every", "everybody", "everyone", "everything",
        "everywhere", "evidence", "evil", "ex-", "exact", "exactly", "exaggerate", "exaggerated", "exam", "examination",
        "examine", "example", "excellent", "except", "exception", "exchange", "excite", "excited", "excitement", "exciting",
        "exclude", "excluding", "excuse", "executive", "exercise", "exhibit", "exhibition", "exist", "existence", "exit",
        "expand", "expect", "expectation", "expected", "expense", "expensive", "experience", "experienced", "experiment", "expert",
        "explain", "explanation", "explode", "explore", "explosion", "export", "expose", "express", "expression", "extend",
        "extension", "extensive", "extent", "extra", "extraordinary", "extreme", "extremely", "eye", "face", "face up to",
        "facility", "fact", "factor", "factory", "fail", "failure", "faint", "faintly", "fair", "fairly",
        "faith", "faithful", "faithfully", "fall", "false", "fame", "familiar", "family", "famous", "fan",
        "fancy", "far", "farm", "farmer", "farming", "farther", "farthest", "fashion", "fashionable", "fast",
        "fasten", "fat", "father", "faucet", "fault", "favour", "favourite", "fear", "feather", "feature",
        "February", "federal", "fee", "feed", "feel", "feeling", "fellow", "female", "fence", "festival",
        "fetch", "fever", "few", "field", "fifteen", "fifteenth", "fifth", "fiftieth", "fifty", "fight",
        "fighting", "figure", "figure out", "file", "fill", "fill in", "fill out", "fill up", "film",
        "final", "finally", "finance", "financial", "find", "find out", "fine", "finely", "finger",
        "finish", "finished", "finish off", "fire", "firm", "firmly", "first", "fish", "fishing", "fit",
        "fit in", "five", "fix", "fixed", "flag", "flame", "flash", "flat", "flavour", "flesh",
        "flight", "float", "flood", "flooded", "flooding", "floor", "flour", "flow", "flower", "flu",
        "fly", "flying", "focus", "fold", "folding", "follow", "following", "follow up", "food", "foot",
        "football", "for", "force", "forecast", "foreign", "forest", "forever", "forget", "forgive", "fork",
        "form", "formal", "formally", "former", "formerly", "formula", "fortieth", "fortune", "forty", "forward",
        "found", "foundation", "four", "fourteen", "fourteenth", "fourth", "frame", "free", "freedom", "freely",
        "freeze", "frequent", "frequently", "fresh", "freshly", "Friday", "fridge", "friend", "friendly", "friendship",
        "frighten", "frighten away/off", "frightened", "frightening", "from", "front", "frozen", "fruit", "fry",
        "fuel", "full", "fully", "fun", "function", "function as", "fund", "fundamental", "funeral", "funny",
        "fur", "furniture", "further", "future", "gain", "gallon", "gamble", "gambling", "game", "gap",
        "garage", "garbage", "garden", "gas", "gasoline", "gate", "gather", "gear", "general", "generally",
        "generate", "generation", "generous", "generously", "gentle", "gentleman", "gently", "genuine", "genuinely",
        "geography", "get", "get away", "get away with", "get back", "get by", "get in", "get into",
        "get off", "get on", "get on with", "get out of", "get over", "get round", "get round to", "get through",
        "get up", "giant", "gift", "girl", "girlfriend", "give", "give away", "give back", "give in",
        "give off", "give out", "give up", "glad", "glass", "global", "glove", "glue", "go",
        "go ahead", "goal", "go away", "go back", "go back to", "go by", "god", "go down",
        "go into", "gold", "good", "goodbye", "goods", "go off", "go on", "go on doing", "go out",
        "go out with", "go over", "go round", "go through", "go through with", "go to", "go up", "govern",
        "government", "governor", "go with", "go without", "grab", "grade", "gradual", "gradually", "grain", "gram",
        "grammar", "grand", "grandchild", "granddaughter", "grandfather", "grandmother", "grandparent", "grandson", "grant", "grass",
        "grateful", "grave 1", "gravely", "great", "greatly", "green", "grey", "grocery", "ground", "group",
        "grow", "growth", "grow up", "guarantee", "guard", "guess", "guest", "guide", "guilty", "gun",
        "guy", "habit", "hair", "hairdresser", "half", "hall", "hammer", "hand", "hand back", "hand down",
        "hand in", "handle", "hand out", "hand over", "hand round", "hang", "hang about", "hang about with", "hang around",
        "hang around with", "hang on", "hang on to", "hang up", "happen", "happen to", "happily", "happiness", "happy",
        "hard", "hardly", "harm", "harmful", "harmless", "hat", "hate", "hatred", "have", "have back",
        "have on", "have to", "he", "head", "headache", "heal", "health", "healthy", "hear", "hear from",
        "hearing", "hear of", "heart", "heat", "heating", "heat up", "heaven", "heavily", "heavy", "heel",
        "height", "hell", "hello", "help", "helpful", "help out", "hence", "her", "here", "hero",
        "hers", "herself", "hesitate", "hi", "hide", "high", "highlight", "highly", "highway", "hill",
        "him", "himself", "hip", "hire", "hire out", "his", "historical", "history", "hit", "hobby",
        "hold", "hold back", "hold on", "hold on to", "hold out", "hold up", "hole", "holiday", "hollow",
        "holy", "home", "homework", "honest", "honestly", "honour", "hook", "hope", "horizontal", "horn",
        "horror", "horse", "hospital", "host", "hot", "hotel", "hour", "house", "household", "housing",
        "how", "however", "huge", "human", "humorous", "humour", "hundred", "hundredth", "hungry", "hunt",
        "hunting", "hurry", "hurry up", "hurt", "husband", "I", "ice", "ice cream", "idea", "ideal",
        "identify", "identify with", "identity", "i.e.", "if", "ignore", "ill", "illegal", "illegally", "illness",
        "illustrate", "image", "imaginary", "imagination", "imagine", "immediate", "immediately", "immoral", "impact", "impatient",
        "implication", "imply", "import", "importance", "important", "importantly", "impose", "impossible", "impress", "impressed",
        "impression", "impressive", "improve", "improvement", "in", "inability", "inch", "incident", "include", "including",
        "income", "increase", "increasingly", "indeed", "independence", "independent", "independently", "index", "indicate", "indication",
        "indirect", "indirectly", "individual", "indoor", "indoors", "industrial", "industry", "inevitable", "inevitably", "infect",
        "infected", "infection", "infectious", "influence", "inform", "informal", "information", "ingredient", "initial", "initially",
        "initiative", "injure", "injured", "injury", "ink", "inner", "innocent", "insect", "insert", "inside",
        "insist", "insist on", "insist on doing", "install", "instance", "instead", "instead of", "institute", "institution", "instruction",
        "instrument", "insult", "insulting", "insurance", "intelligence", "intelligent", "intend", "intended", "intention", "interest",
        "interested", "interesting", "interior", "internal", "international", "Internet", "interpret", "interpretation", "interrupt", "interruption",
        "interval", "interview", "into", "introduce", "introduction", "invent", "invention", "invest", "investigate", "investigation",
        "investment", "invitation", "invite", "involve", "involved", "involvement", "iron", "irritate", "irritated", "irritating",
        "island", "issue", "it", "item", "its", "itself", "jacket", "jam", "January", "jealous",
        "jeans", "jelly", "jewellery", "job", "join", "join in", "joint", "jointly", "joke", "journalist",
        "journey", "joy", "judge", "judgement", "juice", "July", "jump", "June", "junior", "just",
        "justice", "justified", "justify", "keen", "keep", "keep out", "keep out of", "keep up", "keep up with",
        "key", "keyboard", "kick", "kid", "kill", "killing", "kilogram", "kilometre", "kind", "kindly",
        "kindness", "king", "kiss", "kitchen", "knee", "knife", "knit", "knitted", "knitting", "knock",
        "knock down", "knock out", "knot", "know", "knowledge", "lab", "label", "laboratory", "labour", "lack",
        "lacking", "lady", "lake", "lamp", "land", "landscape", "lane", "language", "large", "largely",
        "last 1", "late", "later", "latest", "latter", "laugh", "laugh at", "launch", "law",
        "lawyer", "lay", "layer", "lazy", "lead 1", "leader", "leading 1", "leaf", "league", "lean",
        "learn", "least", "leather", "leave", "leave out", "lecture", "left", "leg", "legal", "legally",
        "lemon", "lend", "length", "less", "lesson", "let", "let down", "let off", "letter", "level",
        "library", "licence", "license", "lid", "lie 1", "lie 2", "lie around", "lie down", "life",
        "lift", "light", "lightly", "like", "likely", "limit", "limited", "limit to", "line", "link",
        "lip", "liquid", "list", "listen", "literature", "litre", "little", "live 1", "live 2", "lively",
        "live on", "live through", "live together", "living", "load", "loan", "local", "locally", "locate",
        "located", "location", "lock", "lock up", "logic", "logical", "lonely", "long", "look", "look after",
        "look at", "look down on", "look forward to", "look into", "look on", "look on with", "look out",
        "look out for", "look round", "look through", "look up", "look up to", "loose", "loosely", "lord",
        "lorry", "lose", "loss", "lost", "lot", "loud", "loudly", "love", "lovely", "lover",
        "low", "loyal", "luck", "lucky", "luggage", "lump", "lunch", "lung", "machine", "machinery",
        "mad", "magazine", "magic", "mail", "main", "mainly", "maintain", "major", "majority", "make",
        "make into", "make up", "make-up", "make up for", "male", "mall", "man", "manage", "management",
        "manager", "manner", "manufacture", "manufacturer", "manufacturing", "many", "map", "march", "March", "mark",
        "market", "marketing", "marriage", "married", "marry", "mass", "massive", "master", "match", "matching",
        "match up", "mate", "material", "mathematics", "matter", "maximum", "may", "May", "maybe", "mayor",
        "me", "meal", "mean", "meaning", "means", "meanwhile", "measure", "measurement", "meat", "media",
        "medical", "medicine", "medium", "meet", "meeting", "meet up", "meet with", "melt", "member", "membership",
        "memory", "mental", "mentally", "mention", "menu", "mere", "merely", "mess", "message", "metal",
        "method", "metre", "mid-", "midday", "middle", "midnight", "might", "mild", "mile", "military",
        "milk", "milligram", "millimetre", "million", "millionth", "mind", "mine", "mineral", "minimum", "minister",
        "ministry", "minor", "minority", "minute 1", "mirror", "miss", "missing", "miss out", "mistake", "mistake for",
        "mistaken", "mix", "mixed", "mixture", "mix up", "mobile", "mobile phone", "model", "modern", "mom",
        "moment", "Monday", "money", "monitor", "month", "mood", "moon", "moral", "morally", "more",
        "moreover", "morning", "most", "mostly", "mother", "motion", "motor", "motorbike", "motorcycle", "mount",
        "mountain", "mouse", "mouth", "move", "move in", "movement", "move out", "move over", "movie",
        "movie theater", "moving", "Mr", "Mrs", "Ms", "much", "mud", "multiply", "mum", "murder",
        "muscle", "museum", "music", "musical", "musician", "must", "my", "myself", "mysterious", "mystery",
        "nail", "naked", "name", "narrow", "nation", "national", "natural", "naturally", "nature", "navy",
        "near", "nearby", "nearly", "neat", "neatly", "necessarily", "necessary", "neck", "need", "needle",
        "negative", "neighbour", "neighbourhood", "neither", "nephew", "nerve", "nervous", "nervously", "nest", "net",
        "network", "never", "nevertheless", "new", "newly", "news", "newspaper", "next", "next to", "nice",
        "nicely", "niece", "night", "nine", "nineteen", "nineteenth", "ninetieth", "ninety", "ninth", "no",
        "nobody", "noise", "noisily", "noisy", "non-", "none", "nonsense", "no one", "nor", "normal",
        "normally", "north", "northern", "nose", "not", "note", "note down", "nothing", "notice", "noticeable",
        "novel", "November", "now", "nowhere", "nuclear", "number", "nurse", "nut", "obey", "object",
        "objective", "observation", "observe", "obtain", "obvious", "obviously", "occasion", "occasionally", "occupied", "occupy",
        "occur", "occur to", "ocean", "o’clock", "October", "odd", "oddly", "of", "off", "offence",
        "offend", "offense", "offensive", "offer", "office", "officer", "official", "officially", "often", "oh",
        "oil", "OK", "old", "old-fashioned", "on", "once", "one", "one another", "onion", "online",
        "only", "onto", "open", "opening", "openly", "open up", "operate", "operation", "opinion", "opponent",
        "opportunity", "oppose", "opposed", "opposing", "opposite", "opposition", "option", "or", "orange", "order",
        "ordinary", "organ", "organization", "organize", "organized", "origin", "original", "originally", "other", "otherwise",
        "ought to", "our", "ours", "ourselves", "out", "outdoor", "outdoors", "outer", "outline", "output",
        "outside", "outstanding", "oven", "over", "overall", "overcome", "owe", "own", "owner", "own up",
        "pace 1", "pack", "package", "packaging", "packet", "pack up", "page", "pain", "painful", "paint",
        "painter", "painting", "pair", "palace", "pale", "pan 1", "panel", "pants", "paper", "parallel",
        "parent", "park", "parliament", "part", "particular", "particularly", "partly", "partner", "partnership", "party",
        "pass", "passage", "pass away", "pass by", "passenger", "passing", "pass on", "pass out", "passport", "pass round",
        "pass through", "past", "path", "patience", "patient", "pattern", "pause", "pay", "pay back", "payment",
        "pay out", "pay up", "peace", "peaceful", "peak", "pen", "pencil", "penny", "pension 1", "people",
        "pepper", "per", "per cent", "perfect", "perfectly", "perform", "performance", "performer", "perhaps", "period",
        "permanent", "permanently", "permission", "permit", "person", "personal", "personality", "personally", "persuade", "pet",
        "petrol", "phase", "philosophy", "phone", "photo", "photocopy", "photograph", "photographer", "photography", "phrase",
        "physical", "physically", "physics", "piano", "pick", "pick up", "picture", "piece", "pig",
        "pile", "pile up", "pill", "pilot", "pin", "pink", "pint", "pipe", "pitch", "pity",
        "place", "plain", "plan", "plane", "planet", "planning", "plant", "plastic", "plate", "platform",
        "play", "play about", "player", "play with", "pleasant", "pleasantly", "please", "pleased", "pleasing", "pleasure",
        "plenty", "plot", "plug", "plug in", "plus 1", "p.m.", "pocket", "poem", "poetry", "point",
        "pointed", "point out", "poison", "poisonous", "pole", "police", "policy", "polish", "polite", "politely",
        "political", "politically", "politician", "politics", "pollution", "pool", "poor", "pop", "popular", "population",
        "port", "pose", "position", "positive", "possess", "possession", "possibility", "possible", "possibly", "post",
        "post office", "pot", "potato", "potential", "potentially", "pound", "pour", "powder", "power", "powerful",
        "practical", "practically", "practice", "practise", "praise", "pray", "prayer", "precise", "precisely", "predict",
        "prefer", "preference", "pregnant", "premises", "preparation", "prepare", "prepared", "presence", "present", "presentation",
        "preserve", "president", "press", "pressure", "presumably", "pretend", "pretty", "prevent", "previous", "previously",
        "price", "pride", "priest", "primarily", "primary", "prime minister", "prince", "princess", "principle", "print",
        "printer", "printing", "print off", "prior", "priority", "prison", "prisoner", "private", "privately", "prize",
        "probable", "probably", "problem", "procedure", "proceed", "process 1", "produce", "producer", "product", "production",
        "profession", "professional", "professor", "profit", "program", "programme", "progress", "project", "promise", "promote",
        "promotion", "prompt", "promptly", "pronounce", "pronunciation", "proof", "proper", "properly", "property", "proportion",
        "proposal", "propose", "prospect", "protect", "protection", "protest", "proud", "proudly", "prove", "provide",
        "provided", "providing", "pub", "public", "publication", "publicity", "publicly", "publish", "publishing", "pull",
        "pull apart", "pull down", "pull in", "pull off", "pull out", "pull over", "pull through", "pull together", "pull up",
        "punch", "punish", "punishment", "pupil", "purchase", "pure", "purely", "purple", "purpose", "pursue",
        "push", "push about", "push forward", "put", "put away", "put back", "put down", "put forward", "put in",
        "put off", "put on", "put out", "put through", "put together", "put up", "put up with", "qualification", "qualified",
        "qualify", "quality", "quantity", "quarter", "queen", "question", "quick", "quickly", "quiet", "quietly",
        "quit", "quite", "quote", "race", "racing", "radio", "rail", "railroad", "railway", "rain",
        "raise", "range", "rank", "rapid", "rapidly", "rare", "rarely", "rate", "rather", "raw",
        "re-", "reach", "react", "reaction", "read", "reader", "reading", "read out", "read over", "ready",
        "real", "realistic", "reality", "realize", "really", "rear", "reason", "reasonable", "reasonably", "recall",
        "receipt", "receive", "recent", "recently", "reception", "reckon", "reckon on", "recognition", "recognize", "recommend",
        "record", "recording", "recover", "red", "reduce", "reduction", "refer", "reference", "refer to", "reflect",
        "reform", "refrigerator", "refusal", "refuse 1", "regard", "regarding", "region", "regional", "register", "regret",
        "regular", "regularly", "regulation", "reject", "relate", "related", "relate to", "relation", "relationship", "relative",
        "relatively", "relax", "relaxed", "relaxing", "release", "relevant", "relief", "religion", "religious", "rely",
        "rely on", "remain", "remaining", "remains", "remark", "remarkable", "remarkably", "remember", "remind", "remind of",
        "remote", "removal", "remove", "rent", "rented", "repair", "repeat", "repeated", "repeatedly", "replace",
        "reply", "report", "represent", "representative", "reproduce", "reputation", "request", "require", "requirement", "rescue",
        "research", "reservation", "reserve", "resident", "resist", "resistance", "resolve", "resort", "resort to", "resource",
        "respect", "respond", "response", "responsibility", "responsible", "rest", "restaurant", "restore", "restrict", "restricted",
        "restriction", "result", "result in", "retain", "retire", "retired", "retirement", "return", "reveal", "reverse",
        "review", "revise", "revision", "revolution", "reward", "rhythm", "rice", "rich", "rid", "ride",
        "rider", "ridiculous", "riding", "right", "rightly", "ring 1", "ring 2", "ring back", "rise",
        "risk", "rival", "river", "road", "rob", "rock", "role", "roll", "romantic", "roof",
        "room", "root", "rope", "rough", "roughly", "round", "rounded", "route", "routine", "row 1",
        "royal", "rub", "rubber", "rubbish", "rude", "rudely", "ruin", "ruined", "rule", "rule out",
        "ruler", "rumour", "run", "run after", "run away", "runner", "running", "run out", "run over",
        "run through", "rural", "rush", "sack", "sad", "sadly", "sadness", "safe", "safely", "safety",
        "sail", "sailing", "sailor", "salad", "salary", "sale", "salt", "salty", "same", "sample",
        "sand", "satisfaction", "satisfied", "satisfy", "satisfying", "Saturday", "sauce", "save", "saving", "say",
        "scale", "scare", "scared", "scare off", "scene", "schedule", "scheme", "school", "science", "scientific",
        "scientist", "scissors", "score", "scratch", "scream", "screen", "screw", "sea", "seal", "seal off",
        "search", "season", "seat", "second 1", "secondary", "secret", "secretary", "secretly", "section", "sector",
        "secure", "security", "see", "see about", "seed", "seek", "seem", "see to", "select", "selection",
        "self", "self-", "sell", "sell off", "sell out", "senate", "senator", "send", "send for", "send off",
        "senior", "sense", "sensible", "sensitive", "sentence", "separate", "separated", "separately", "separation", "September",
        "series", "serious", "seriously", "servant", "serve", "service", "session", "set", "set off", "set out",
        "settle", "settle down", "set up", "seven", "seventeen", "seventh", "seventieth", "seventy", "several", "severe",
        "severely", "sew", "sewing", "sex", "sexual", "sexually", "shade", "shadow", "shake", "shall",
        "shallow", "shame", "shape", "shaped", "share", "sharp", "sharply", "shave", "she", "sheep",
        "sheet", "shelf", "shell", "shelter", "shift", "shine", "shiny", "ship", "shirt", "shock",
        "shocked", "shocking", "shoe", "shoot", "shoot down", "shooting", "shop", "shopping", "short", "shortly",
        "shot", "should", "shoulder", "shout", "show", "shower", "show off", "show round", "show up", "shut",
        "shut down", "shut in", "shut out", "shut up", "shy", "sick", "side", "sideways", "sight",
        "sign", "signal", "signature", "significant", "significantly", "silence", "silent", "silk", "silly", "silver",
        "similar", "similarly", "simple", "simply", "since", "sincere", "sincerely", "sing", "singer", "singing",
        "single", "sink", "sir", "sister", "sit", "sit down", "site", "situation", "six", "sixteen",
        "sixth", "sixtieth", "sixty", "size", "skilful", "skilfully", "skill", "skilled", "skin", "skirt",
        "sky", "sleep", "sleeve", "slice", "slide", "slight", "slightly", "slip", "slope", "slow",
        "slowly", "small", "smart", "smash", "smell", "smile", "smoke", "smoking", "smooth", "smoothly",
        "snake", "snow", "so", "soap", "social", "socially", "society", "sock", "soft", "softly",
        "software", "soil", "soldier", "solid", "solution", "solve", "some", "somebody", "somehow", "someone",
        "something", "sometimes", "somewhat", "somewhere", "son", "song", "soon", "sore", "sorry", "sort",
        "sort out", "soul", "sound", "soup", "sour", "source", "south", "southern", "space", "spare",
        "speak", "speaker", "speak out", "speak up", "special", "specialist", "specially", "specific", "specifically",
        "speech", "speed", "speed up", "spell", "spelling", "spend", "spice", "spicy", "spider", "spin",
        "spirit", "spiritual", "spite", "split", "split up", "spoil", "spoken", "spoon", "sport", "spot",
        "spray", "spread", "spread out", "spring", "square", "squeeze", "stable", "staff", "stage", "stair",
        "stamp", "stand", "standard", "stand back", "stand by", "stand for", "stand out", "stand up", "stand up for",
        "star", "stare", "start", "start off", "start out", "start up", "state", "statement", "station",
        "statue", "status", "stay", "stay away", "stay out of", "steadily", "steady", "steal", "steam",
        "steel", "steep", "steeply", "steer", "step", "stick", "stick out", "stick to", "stick up", "sticky",
        "stiff", "stiffly", "still", "sting", "stir", "stock", "stomach", "stone", "stop", "store",
        "storm", "story", "stove", "straight", "strain", "strange", "strangely", "stranger", "strategy", "stream",
        "street", "strength", "stress", "stressed", "stretch", "strict", "strictly", "strike", "striking", "string",
        "strip", "stripe", "striped", "stroke", "strong", "strongly", "structure", "struggle", "student", "studio",
        "study", "stuff", "stupid", "style", "subject", "substance", "substantial", "substantially", "substitute", "succeed",
        "success", "successful", "successfully", "such", "suck", "sudden", "suddenly", "suffer", "suffering", "sufficient",
        "sufficiently", "sugar", "suggest", "suggestion", "suit", "suitable", "suitcase", "suited", "sum", "summary",
        "summer", "sum up", "sun", "Sunday", "superior", "supermarket", "supply", "support", "supporter", "suppose",
        "sure", "surely", "surface", "surname", "surprise", "surprised", "surprising", "surprisingly", "surround", "surrounding",
        "surroundings", "survey", "survive", "suspect", "suspicion", "suspicious", "swallow", "swear", "swearing", "sweat",
        "sweater", "sweep", "sweet", "swell", "swelling", "swim", "swimming", "swimming pool", "swing", "switch",
        "switch off", "swollen", "symbol", "sympathetic", "sympathy", "system", "table", "tablet", "tackle",
        "tail", "take", "take away", "take back", "take down", "take in", "take off", "take on", "take over",
        "take up", "talk", "tall", "tank", "tap", "tape", "target", "task", "taste", "tax",
        "taxi", "tea", "teach", "teacher", "teaching", "team", "tear 1", "tear 2", "tear up", "technical",
        "technique", "technology", "telephone", "television", "tell", "tell off", "temperature", "temporarily", "temporary",
        "ten", "tend", "tendency", "tension", "tent", "tenth", "term", "terrible", "terribly", "test",
        "text", "than", "thank", "thanks", "thank you", "that", "the", "theatre", "their", "theirs",
        "them", "theme", "themselves", "then", "theory", "there", "therefore", "they", "thick", "thickly",
        "thickness", "thief", "thin", "thing", "think", "think about", "thinking", "think of", "think of as",
        "think over", "think up", "third", "thirsty", "thirteen", "thirteenth", "thirtieth", "thirty", "this", "thorough",
        "thoroughly", "though", "thought", "thousand", "thousandth", "thread", "threat", "threaten", "threatening", "three",
        "throat", "through", "throughout", "throw", "throw away", "throw out", "thumb", "Thursday", "thus", "ticket",
        "tidy", "tie", "tie up", "tight", "tightly", "till", "time", "timetable", "tin", "tiny",
        "tip", "tip over", "tire", "tired", "tire out", "tiring", "title", "to", "today", "toe",
        "together", "toilet", "tomato", "tomorrow", "ton", "tone", "tongue", "tonight", "tonne", "too",
        "tool", "tooth", "top", "topic", "total", "totally", "touch", "tough", "tour", "tourist",
        "towards", "towel", "tower", "town", "toy", "trace", "track", "trade", "trading", "tradition",
        "traditional", "traditionally", "traffic", "train", "training", "transfer", "transform", "translate", "translation", "transparent",
        "transport", "transportation", "trap", "travel", "traveller", "treat", "treatment", "tree", "trend", "trial",
        "triangle", "trick", "trip", "tropical", "trouble", "trousers", "truck", "true", "truly", "trust",
        "truth", "try", "try on", "try out", "tube", "Tuesday", "tune", "tunnel", "turn", "turn back",
        "turn down", "turn into", "turn off", "turn on", "turn out", "turn over", "turn round", "turn to",
        "turn up", "TV", "twelfth", "twelve", "twentieth", "twenty", "twice", "twin", "twist", "twisted",
        "two", "type", "typical", "typically", "tyre", "the unemployed", "the unexpected", "ugly", "ultimate", "ultimately",
        "umbrella", "unable", "unacceptable", "uncertain", "uncle", "uncomfortable", "unconscious", "uncontrolled", "under", "underground",
        "underneath", "understand", "understanding", "underwater", "underwear", "undo", "unemployed", "unemployment", "unexpected", "unexpectedly",
        "unfair", "unfairly", "unfortunate", "unfortunately", "unfriendly", "unhappy", "uniform", "unimportant", "union", "unique",
        "unit", "unite", "united", "universe", "university", "unkind", "unknown", "unless", "unlike", "unlikely",
        "unload", "unlucky", "unnecessary", "unpleasant", "unreasonable", "unsteady", "unsuccessful", "untidy", "until", "unusual",
        "unusually", "unwilling", "unwillingly", "up", "upon", "upper", "upset", "upsetting", "upside down", "upstairs",
        "upward", "upwards", "urban", "urge", "urgent", "us", "use", "used 1", "used 2", "used to",
        "useful", "useless", "user", "use up", "usual", "usually", "vacation", "valid", "valley", "valuable",
        "value", "van", "variation", "varied", "variety", "various", "vary", "vast", "vegetable", "vehicle",
        "venture", "version", "vertical", "very", "via", "victim", "victory", "video", "view", "village",
        "violence", "violent", "violently", "virtually", "virus", "visible", "vision", "visit", "visitor", "vital",
        "vocabulary", "voice", "volume", "vote", "wage", "waist", "wait", "waiter", "wake", "walk",
        "walking", "walk out", "walk up", "wall", "wallet", "wander", "want", "war", "warm", "warmth",
        "warm up", "warn", "warning", "wash", "wash away", "washing", "wash off", "wash out", "wash up", "waste",
        "watch", "watch out", "watch out for", "water", "wave", "way", "we", "weak", "weakness",
        "wealth", "weapon", "wear", "wear away", "wear off", "wear out", "weather", "web", "website",
        "wedding", "Wednesday", "week", "weekend", "weekly", "weigh", "weight", "welcome", "well", "well known",
        "west", "western", "wet", "what", "whatever", "wheel", "when", "whenever", "where", "whereas",
        "wherever", "whether", "which", "while", "whisper", "whistle", "white", "who", "whoever", "whole",
        "whom", "whose", "why", "wide", "widely", "width", "wife", "wild", "wildly", "will",
        "willing", "willingly", "willingness", "win", "wind 1", "wind 2", "window", "wine", "wing",
        "winner", "winning", "winter", "wire", "wise", "wish", "with", "withdraw", "within", "without",
        "witness", "woman", "wonder", "wonderful", "wood", "wooden", "wool", "word", "work", "worker",
        "working", "work out", "world", "worried", "worry", "worrying", "worse", "worship", "worst", "worth",
        "would", "wound 1", "wounded", "wrap", "wrapping", "wrist", "write", "write back", "write down", "writer",
        "writing", "written", "wrong", "wrongly", "yard", "yawn", "yeah", "year", "yellow", "yes",
        "yesterday", "yet", "you", "young", "your", "yours", "yourself", "youth", "zero", "zone"
    }},
    {"quotes", {
        "To", "be", "or", "not", "to", "be", "that", "is", "the", "question", "Whether", "tis", "nobler",
        "All", "that", "glitters", "is", "not", "gold", "Ask", "not", "what", "your", "country", "can", "do",
        "for", "you", "The", "only", "thing", "we", "have", "to", "fear", "is", "fear", "itself", "I", "think",
        "therefore", "I", "am", "Knowledge", "is", "power", "Time", "is", "money", "Practice", "makes", "perfect",
        "Where", "there", "is", "a", "will", "there", "is", "a", "way", "Actions", "speak", "louder", "than", "words"
    }}
};

map<string, vector<string>> custom_word_banks;

struct GameStats {
    long long timestamp;
    int wpm;
    int accuracy;
    int words;
    string mode;
};

vector<GameStats> stats_history;

fs::path get_stats_file_path() {
    return get_data_directory() / "stats.txt";
}

fs::path get_wordbanks_file_path() {
    return get_data_directory() / "custom_wordbanks.txt";
}

void load_stats() {
    stats_history.clear();
    ifstream file(get_stats_file_path());
    if (!file.is_open()) return;
    string line;
    while (getline(file, line)) {
        if(line.empty()) continue;
        istringstream iss(line);
        GameStats stat;
        if(iss >> stat.timestamp >> stat.wpm >> stat.accuracy >> stat.words) {
            if (!(iss >> stat.mode)) stat.mode = "time";
            stats_history.push_back(stat);
        }
    }
}

void load_custom_wordbanks() {
    custom_word_banks.clear();
    ifstream file(get_wordbanks_file_path());
    if (!file.is_open()) return;

    string line;
    string current_bank_name;
    vector<string> current_words;

    while (getline(file, line)) {
        if (line.empty()) continue;

        if (line[0] == '[' && line[line.length()-1] == ']') {
            if (!current_bank_name.empty() && !current_words.empty()) {
                custom_word_banks[current_bank_name] = current_words;
            }
            current_bank_name = line.substr(1, line.length()-2);
            current_words.clear();
        } else {
            istringstream iss(line);
            string word;
            while (iss >> word) {
                if (!word.empty()) current_words.push_back(word);
            }
        }
    }

    if (!current_bank_name.empty() && !current_words.empty()) {
        custom_word_banks[current_bank_name] = current_words;
    }
}

void save_custom_wordbank(const string& name, const vector<string>& words) {
    custom_word_banks[name] = words;

    ofstream file(get_wordbanks_file_path(), ios::trunc);
    if (file.is_open()) {
        for (const auto& [bank_name, bank_words] : custom_word_banks) {
            file << "[" << bank_name << "]\n";
            for (size_t i = 0; i < bank_words.size(); i++) {
                file << bank_words[i];
                if ((i + 1) % 10 == 0 || i == bank_words.size() - 1) {
                    file << "\n";
                } else {
                    file << " ";
                }
            }
            file << "\n";
        }
    }
}

void delete_custom_wordbank(const string& name) {
    custom_word_banks.erase(name);

    ofstream file(get_wordbanks_file_path(), ios::trunc);
    if (file.is_open()) {
        for (const auto& [bank_name, bank_words] : custom_word_banks) {
            file << "[" << bank_name << "]\n";
            for (size_t i = 0; i < bank_words.size(); i++) {
                file << bank_words[i];
                if ((i + 1) % 10 == 0 || i == bank_words.size() - 1) {
                    file << "\n";
                } else {
                    file << " ";
                }
            }
            file << "\n";
        }
    }
}

void save_stat(const GameStats& stat) {
    stats_history.push_back(stat);
    ofstream file(get_stats_file_path(), ios::app);
    if (file.is_open()) {
        file << stat.timestamp << " " << stat.wpm << " " << stat.accuracy << " " << stat.words << " " << stat.mode << "\n";
    }
}

void rewrite_stats_file() {
    ofstream file(get_stats_file_path(), ios::trunc);
    if (file.is_open()) {
        for (const auto& stat : stats_history) {
            file << stat.timestamp << " " << stat.wpm << " " << stat.accuracy << " " << stat.words << " " << stat.mode << "\n";
        }
    }
}

string format_timestamp(long long timestamp_ms) {
    time_t timestamp = timestamp_ms / 1000;
    struct tm* timeinfo = localtime(&timestamp);
    char buffer[256];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", timeinfo);
    return string(buffer);
}

// ==========================================
// MAIN
// ==========================================
int main() {
    fs::path dataDir = get_data_directory();
    cout << "Data Directory: " << dataDir << endl;
    load_stats();
    load_custom_wordbanks();

    httplib::Server svr;

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(INDEX_HTML, "text/html");
    });

    svr.Get("/stats", [](const httplib::Request&, httplib::Response& res) {
        Template tmpl = Template::fromString(R"(
<!DOCTYPE html>
<html>
<head>
    <title>Stats | type/c++</title>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.9.1/chart.min.js"></script>
    <style>)" + STATS_CSS + R"(</style>
</head>
<body>
    <div class="nav">
    <a href="/">&lt; Back to Game</a>
    </div>
    <div class="grid">
        <div class="card"><div class="lbl">Games Played</div><div class="val">{{ total_games }}</div></div>
        <div class="card"><div class="lbl">Avg WPM</div><div class="val">{{ avg_wpm }}</div></div>
        <div class="card"><div class="lbl">Highest WPM</div><div class="val">{{ best_wpm }}</div></div>
        <div class="card"><div class="lbl">Avg Accuracy</div><div class="val">{{ avg_accuracy }}%</div></div>
    </div>
    <div class="chart-box"><canvas id="wpmChart"></canvas></div>
    <table>
        <thead><tr><th>Date</th><th>Mode</th><th>WPM</th><th>Acc</th><th>Words</th></tr></thead>
        <tbody>
            <script>
            const history = {{ history_data | raw }};
            const wpmData = {{ wpm_data | raw }};
            const labels = {{ labels | raw }};

            document.write(history.map(r =>
                `<tr><td>${r.date}</td><td><span style="opacity:0.7; font-size:0.9em; border:1px solid #555; padding:2px 6px; border-radius:4px">${r.mode}</span></td><td>${r.wpm}</td><td>${r.accuracy}%</td><td>${r.words}</td></tr>`
            ).join(''));

            const theme = localStorage.getItem('theme') || 'default';
            document.documentElement.setAttribute('data-theme', theme);
            const color = theme.includes('light') ? '#00aa00' : '#4af626';

            new Chart(document.getElementById('wpmChart'), {
                type: 'line',
                data: {
                    labels: labels,
                    datasets: [{
                        label: 'WPM',
                        data: wpmData,
                        borderColor: color,
                        backgroundColor: color + '22',
                        tension: 0.4, fill: true, pointRadius: 3
                    }]
                },
                options: {
                    responsive: true, maintainAspectRatio: false,
                    plugins: { legend: { display: false } },
                    scales: {
                        y: { beginAtZero: true, grid: { color: 'rgba(128,128,128,0.1)' } },
                        x: { display: false }
                    }
                }
            });
            </script>
        </tbody>
    </table>
</body>
</html>
        )", "stats_page");

        int total = stats_history.size();
        int best = 0;
        long long sum_wpm = 0, sum_acc = 0;
        for(auto& s : stats_history) {
            if(s.wpm > best) best = s.wpm;
            sum_wpm += s.wpm;
            sum_acc += s.accuracy;
        }

        string wpm_j = "[", lbl_j = "[", hist_j = "[";
        int start = max(0, total - 30);
        for(int i = start; i < total; i++) {
            wpm_j += to_string(stats_history[i].wpm) + (i<total-1?",":"");
            lbl_j += "\"G" + to_string(i+1) + "\"" + (i<total-1?",":"");
        }
        wpm_j += "]"; lbl_j += "]";

        int h_start = max(0, total - 15);
        for(int i = total - 1; i >= h_start; i--) {
            hist_j += "{\"date\":\"" + format_timestamp(stats_history[i].timestamp) + "\",";
            hist_j += "\"mode\":\"" + stats_history[i].mode + "\",";
            hist_j += "\"wpm\":" + to_string(stats_history[i].wpm) + ",";
            hist_j += "\"accuracy\":" + to_string(stats_history[i].accuracy) + ",";
            hist_j += "\"words\":" + to_string(stats_history[i].words) + "}";
            if(i > h_start) hist_j += ",";
        }
        hist_j += "]";

        tmpl.set("total_games", to_string(total));
        tmpl.set("avg_wpm", total ? to_string(sum_wpm/total) : "0");
        tmpl.set("best_wpm", to_string(best));
        tmpl.set("avg_accuracy", total ? to_string(sum_acc/total) : "0");
        tmpl.set("history_data", hist_j);
        tmpl.set("wpm_data", wpm_j);
        tmpl.set("labels", lbl_j);

        res.set_content(tmpl.render(), "text/html");
    });

    svr.Get("/api/words", [](const httplib::Request& req, httplib::Response& res) {
        int count = 50;
        if(req.has_param("count")) count = stoi(req.get_param_value("count"));

        string bank_name = "cpp";
        if(req.has_param("bank")) bank_name = req.get_param_value("bank");

        vector<string> words;

        if (WORD_BANKS.find(bank_name) != WORD_BANKS.end()) {
            words = WORD_BANKS[bank_name];
        } else if (custom_word_banks.find(bank_name) != custom_word_banks.end()) {
            words = custom_word_banks[bank_name];
        } else {
            words = WORD_BANKS["cpp"];
        }

        random_device rd;
        mt19937 g(rd());
        shuffle(words.begin(), words.end(), g);

        string json = "[";
        for(int i=0; i<count; i++) {
            json += "\"" + words[i % words.size()] + "\"";
            if(i < count-1) json += ",";
        }
        json += "]";
        res.set_content(json, "application/json");
    });

    svr.Post("/api/save-wordbank", [](const httplib::Request& req, httplib::Response& res) {
        try {
            string b = req.body;
            auto getVal = [&](string key) -> string {
                size_t pos = b.find("\"" + key + "\":");
                if(pos == string::npos) return "";
                size_t start = b.find_first_not_of(" \t\n\r", pos + key.length() + 3);
                bool isString = (b[start] == '"');
                if(isString) start++;
                size_t end = b.find_first_of(isString ? "\"" : ",}", start);
                return b.substr(start, end - start);
            };

            string name = getVal("name");
            string words_str = getVal("words");

            if (!name.empty() && !words_str.empty()) {
                vector<string> words;
                istringstream iss(words_str);
                string word;
                while (iss >> word) {
                    if (!word.empty()) words.push_back(word);
                }

                if (!words.empty()) {
                    save_custom_wordbank(name, words);
                }
            }

            res.set_content("{}", "application/json");
        } catch(...) { res.status = 400; }
    });

    svr.Delete("/api/delete-wordbank", [](const httplib::Request& req, httplib::Response& res) {
        try {
            string b = req.body;
            auto getVal = [&](string key) -> string {
                size_t pos = b.find("\"" + key + "\":");
                if(pos == string::npos) return "";
                size_t start = b.find_first_not_of(" \t\n\r", pos + key.length() + 3);
                bool isString = (b[start] == '"');
                if(isString) start++;
                size_t end = b.find_first_of(isString ? "\"" : ",}", start);
                return b.substr(start, end - start);
            };

            string name = getVal("name");
            if (!name.empty()) {
                delete_custom_wordbank(name);
            }

            res.set_content("{}", "application/json");
        } catch(...) { res.status = 400; }
    });

    svr.Post("/api/save-stats", [](const httplib::Request& req, httplib::Response& res) {
        try {
            string b = req.body;
            GameStats s;
            s.mode = "time";
            auto getVal = [&](string key) -> string {
                size_t pos = b.find("\"" + key + "\":");
                if(pos == string::npos) return "";
                size_t start = b.find_first_not_of(" \t\n\r", pos + key.length() + 3);
                bool isString = (b[start] == '"');
                if(isString) start++;
                size_t end = b.find_first_of(isString ? "\"" : ",}", start);
                return b.substr(start, end - start);
            };
            string wpm = getVal("wpm"); if(!wpm.empty()) s.wpm = stoi(wpm);
            string acc = getVal("accuracy"); if(!acc.empty()) s.accuracy = stoi(acc);
            string w = getVal("words"); if(!w.empty()) s.words = stoi(w);
            string m = getVal("mode"); if(!m.empty()) s.mode = m;
            s.timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            save_stat(s);
            res.set_content("{}", "application/json");
        } catch(...) { res.status = 400; }
    });

    svr.Delete("/api/delete-last-stat", [](const httplib::Request&, httplib::Response& res) {
        if(!stats_history.empty()) {
            stats_history.pop_back();
            rewrite_stats_file();
            res.set_content("{}", "application/json");
        } else res.status = 400;
    });

    cout << "Starting server on http://localhost:8080..." << endl;
    std::thread([](){
        std::this_thread::sleep_for(std::chrono::seconds(1));
        open_browser("http://localhost:8080");
    }).detach();

    svr.listen("localhost", 8080);
}
