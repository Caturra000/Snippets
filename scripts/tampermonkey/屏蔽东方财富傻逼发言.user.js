// ==UserScript==
// @name         屏蔽东方财富傻逼发言
// @namespace    http://tampermonkey.net/
// @version      1.0
// @description  屏蔽东方财富股吧特定用户的发言，支持行情页、期货页及独立股吧页，带管理面板
// @author       Caturra
// @match        *://*.eastmoney.com/*
// @grant        GM_setValue
// @grant        GM_getValue
// @grant        GM_registerMenuCommand
// @grant        GM_addStyle
// ==/UserScript==

(function() {
    'use strict';

    // --- 样式设置 ---
    const css = `
        .em-block-panel {
            position: fixed;
            top: 50px;
            right: 20px;
            width: 300px;
            background: #fff;
            border: 1px solid #ccc;
            box-shadow: 0 2px 10px rgba(0,0,0,0.2);
            z-index: 999999;
            padding: 15px;
            border-radius: 8px;
            font-size: 14px;
            display: none;
            color: #333;
            font-family: sans-serif;
        }
        .em-block-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 10px;
            border-bottom: 1px solid #eee;
            padding-bottom: 5px;
        }
        .em-block-title { font-weight: bold; font-size: 16px; }
        .em-close-btn { cursor: pointer; color: #999; font-size: 18px; }
        .em-close-btn:hover { color: #f00; }
        .em-input-group { display: flex; gap: 5px; margin-bottom: 10px; }
        .em-input { flex: 1; padding: 5px; border: 1px solid #ddd; border-radius: 4px; }
        .em-add-btn { padding: 5px 10px; background: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer; }
        .em-add-btn:hover { background: #0056b3; }
        .em-list { max-height: 300px; overflow-y: auto; list-style: none; padding: 0; margin: 0; border: 1px solid #eee; }
        .em-list-item { display: flex; justify-content: space-between; padding: 5px 8px; border-bottom: 1px solid #f5f5f5; }
        .em-list-item:last-child { border-bottom: none; }
        .em-del-btn { color: #dc3545; cursor: pointer; margin-left: 10px; }
        .em-del-btn:hover { text-decoration: underline; }
        /* 快捷屏蔽按钮样式 */
        .em-quick-block {
            font-size: 12px;
            color: #fff;
            background-color: #dc3545;
            cursor: pointer;
            margin-left: 8px;
            border-radius: 3px;
            padding: 1px 4px;
            display: inline-block;
            line-height: 14px;
            font-weight: normal;
            vertical-align: middle;
        }
        .em-quick-block:hover { background-color: #a71d2a; }
    `;
    GM_addStyle(css);

    // --- 状态管理 ---
    let blockedUsers = GM_getValue('em_blocked_users', []);

    function saveUsers() {
        GM_setValue('em_blocked_users', blockedUsers);
    }

    function addUser(username) {
        if (!username) return;
        username = username.trim();
        if (!blockedUsers.includes(username)) {
            blockedUsers.push(username);
            saveUsers();
            renderList();
            scanAndBlock();
        }
    }

    function removeUser(username) {
        blockedUsers = blockedUsers.filter(u => u !== username);
        saveUsers();
        renderList();
        scanAndBlock();
    }

    // --- UI 构建 ---
    const panel = document.createElement('div');
    panel.className = 'em-block-panel';
    panel.innerHTML = `
        <div class="em-block-header">
            <span class="em-block-title">股吧屏蔽管理</span>
            <span class="em-close-btn">×</span>
        </div>
        <div class="em-input-group">
            <input type="text" class="em-input" placeholder="输入用户名...">
            <button class="em-add-btn">添加</button>
        </div>
        <ul class="em-list"></ul>
    `;
    document.body.appendChild(panel);

    const ul = panel.querySelector('.em-list');
    const input = panel.querySelector('.em-input');
    const addBtn = panel.querySelector('.em-add-btn');
    const closeBtn = panel.querySelector('.em-close-btn');

    function renderList() {
        ul.innerHTML = '';
        if (blockedUsers.length === 0) {
            ul.innerHTML = '<li style="padding:10px; text-align:center; color:#999;">暂无屏蔽用户</li>';
            return;
        }
        blockedUsers.forEach(user => {
            const li = document.createElement('li');
            li.className = 'em-list-item';
            li.innerHTML = `<span>${user}</span><span class="em-del-btn" data-user="${user}">移除</span>`;
            ul.appendChild(li);
        });
        ul.querySelectorAll('.em-del-btn').forEach(btn => {
            btn.onclick = (e) => removeUser(e.target.getAttribute('data-user'));
        });
    }

    addBtn.onclick = () => { addUser(input.value); input.value = ''; };
    closeBtn.onclick = () => { panel.style.display = 'none'; };
    input.addEventListener('keypress', (e) => { if (e.key === 'Enter') { addUser(input.value); input.value = ''; } });

    GM_registerMenuCommand("管理屏蔽名单", () => {
        panel.style.display = 'block';
        renderList();
    });

    // --- 核心逻辑：处理单行数据 ---
    function processRow(row, username, btnContainer) {
        if (!username) return;

        // 1. 检查屏蔽状态
        if (blockedUsers.includes(username)) {
            row.style.display = 'none';
        } else {
            row.style.display = ''; // 恢复显示

            // 2. 添加屏蔽按钮 (避免重复添加)
            if (btnContainer && !btnContainer.querySelector('.em-quick-block')) {
                const blockBtn = document.createElement('span');
                blockBtn.className = 'em-quick-block';
                blockBtn.innerText = '屏蔽';
                blockBtn.title = `屏蔽用户：${username}`;
                blockBtn.onclick = (e) => {
                    e.preventDefault();
                    e.stopPropagation();
                    if(confirm(`确定要屏蔽【${username}】吗？`)) {
                        addUser(username);
                    }
                };
                btnContainer.appendChild(blockBtn);
            }
        }
    }

    // --- 扫描器：适配不同页面结构 ---
    function scanAndBlock() {
        // === 场景1：传统股吧 (tr.articlerow) ===
        // 特征：data-username 在 tr 上
        const type1Rows = document.querySelectorAll('tr.articlerow');
        type1Rows.forEach(row => {
            const username = row.getAttribute('data-username');
            const authorTd = row.querySelector('.gblisttd4'); // 名字所在的列
            processRow(row, username, authorTd);
        });

        // === 场景2：期货/新版股吧 (tr.listitem) ===
        // 特征：没有 data-username，名字在 .author .nametext 里
        const type2Rows = document.querySelectorAll('tr.listitem');
        type2Rows.forEach(row => {
            // 查找名字元素
            const nameEl = row.querySelector('.author .nametext');
            if (nameEl) {
                const username = nameEl.innerText.trim();
                // 按钮放在 .author 容器里，名字后面
                const authorDiv = row.querySelector('.author');
                processRow(row, username, authorDiv);
            }
        });
    }

    // --- 监听动态加载 (MutationObserver) ---
    const observer = new MutationObserver((mutations) => {
        // 简单节流，或者直接执行（DOM操作开销此时可以接受）
        scanAndBlock();
    });

    observer.observe(document.body, { childList: true, subtree: true });

    // 初始化
    renderList();
    setTimeout(scanAndBlock, 500); // 延迟一点以防页面初始化未完成

})();