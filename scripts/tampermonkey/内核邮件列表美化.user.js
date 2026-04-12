// ==UserScript==
// @name         Lore LKML Beautifier (内核邮件列表美化)
// @namespace    http://tampermonkey.net/
// @version      1.7
// @description  Beautify lore.kernel.org: constrained widths, collapsible diffs, text/code mode toggle, structured headers.
// @author       AI Assistant
// @match        https://lore.kernel.org/*
// @icon         https://www.kernel.org/theme/images/logos/favicon.png
// @grant        GM_addStyle
// ==/UserScript==

(function() {
    'use strict';

    const css = `
        :root {
            --bg-color: #f4f5f7;
            --card-bg: #ffffff;
            --text-color: #24292f;
            --muted-text: #57606a;
            --border-color: #d0d7de;
            --link-color: #0969da;
            --quote-color: #656d76;
            --quote-border: #d0d7de;
            --diff-add-bg: #e6ffec;
            --diff-add-text: #1a7f37;
            --diff-del-bg: #ffebe9;
            --diff-del-text: #cf222e;
            --diff-hunk-bg: #ddf4ff;
            --diff-hunk-text: #0969da;
            --diff-details-bg: #f6f8fa;
            --card-max-width: 900px;
            --tag-bg: #f6f8fa;
        }

        @media (prefers-color-scheme: dark) {
            :root {
                --bg-color: #0d1117;
                --card-bg: #161b22;
                --text-color: #c9d1d9;
                --muted-text: #8b949e;
                --border-color: #30363d;
                --link-color: #58a6ff;
                --quote-color: #8b949e;
                --quote-border: #30363d;
                --diff-add-bg: rgba(46, 160, 67, 0.15);
                --diff-add-text: #3fb950;
                --diff-del-bg: rgba(248, 81, 73, 0.15);
                --diff-del-text: #f85149;
                --diff-hunk-bg: rgba(56, 139, 253, 0.15);
                --diff-hunk-text: #58a6ff;
                --diff-details-bg: #0d1117;
                --tag-bg: #21262d;
            }
        }

        * { box-sizing: border-box !important; }

        body {
            background-color: var(--bg-color) !important;
            color: var(--text-color) !important;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif !important;
            margin: 0 !important;
            padding: 20px 16px !important;
            line-height: 1.6;
            max-width: 100vw !important;
            overflow-x: hidden !important;
        }

        a { color: var(--link-color) !important; text-decoration: none; }
        a:hover { text-decoration: underline; }

        form, .lkml-sys-block {
            background: var(--card-bg);
            border: 1px solid var(--border-color);
            border-radius: 8px;
            padding: 15px 20px;
            margin-bottom: 20px;
            margin-left: auto;
            margin-right: auto;
            box-shadow: 0 1px 3px rgba(0,0,0,0.05);
            max-width: var(--card-max-width);
        }

        .lkml-email-card {
            background: var(--card-bg);
            border: 1px solid var(--border-color);
            border-radius: 8px;
            padding: 24px;
            margin-bottom: 24px;
            margin-left: auto;
            margin-right: auto;
            box-shadow: 0 2px 6px rgba(0,0,0,0.04);
            width: 100% !important;
            max-width: var(--card-max-width) !important;
            overflow-x: hidden !important;
        }

        .lkml-email-text {
            font-family: "JetBrains Mono", "Fira Code", Consolas, "Courier New", monospace !important;
            font-size: 14px !important;
            margin: 0;
            color: var(--text-color);
            width: 100% !important;
            white-space: pre-wrap !important;
            word-wrap: break-word !important;
            word-break: break-all !important;
            overflow-wrap: anywhere !important;
        }

        .lkml-email-text.lkml-text-mode {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif !important;
            font-size: 15px !important;
            line-height: 1.75 !important;
            word-break: normal !important;
        }

        span.q {
            color: var(--quote-color) !important;
            border-left: 4px solid var(--quote-border);
            padding-left: 12px;
            display: inline-block;
            margin-top: 4px;
            margin-bottom: 4px;
        }

        /* 线程导航 */
        .lkml-thread-nav {
            border: 1px solid var(--border-color);
            border-radius: 6px;
            margin-bottom: 16px;
            background: var(--diff-details-bg);
        }
        .lkml-thread-nav > summary {
            padding: 8px 14px;
            cursor: pointer;
            font-size: 14px;
            user-select: none;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
            border-radius: 6px;
        }
        .lkml-thread-nav[open] > summary {
            border-bottom: 1px solid var(--border-color);
            border-bottom-left-radius: 0;
            border-bottom-right-radius: 0;
        }
        .lkml-nav-content {
            font-family: "JetBrains Mono", "Fira Code", Consolas, monospace;
            font-size: 13px;
            padding: 12px 14px;
            margin: 0;
            max-height: 300px;
            overflow-y: auto;
            white-space: pre-wrap;
            word-break: break-all;
        }

        /* 邮件头 */
        .lkml-email-headers {
            padding: 12px 0;
            margin-bottom: 12px;
            border-bottom: 1px solid var(--border-color);
        }
        .lkml-header-row {
            display: flex;
            align-items: baseline;
            padding: 3px 0;
            font-size: 14px;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
        }
        .lkml-header-label {
            font-weight: 600;
            color: var(--muted-text);
            min-width: 56px;
            flex-shrink: 0;
            margin-right: 12px;
        }
        .lkml-header-value {
            color: var(--text-color);
            flex: 1;
            line-height: 1.5;
        }
        .lkml-header-from {
            font-weight: 600;
        }
        .lkml-header-date {
            color: var(--muted-text);
            margin-left: 8px;
            font-size: 13px;
        }
        .lkml-header-tag {
            display: inline-block;
            background: var(--tag-bg);
            border: 1px solid var(--border-color);
            border-radius: 4px;
            padding: 1px 8px;
            margin: 2px 3px 2px 0;
            font-size: 13px;
            font-family: "JetBrains Mono", "Fira Code", Consolas, monospace;
        }

        /* Diff 折叠区域 */
        .lkml-diff-details {
            margin: 16px 0;
            border: 1px solid var(--border-color);
            border-radius: 6px;
            background: var(--diff-details-bg);
            width: 100% !important;
            max-width: 100% !important;
            overflow-x: hidden !important;
        }
        .lkml-diff-details > summary {
            padding: 10px 14px;
            cursor: pointer;
            background: var(--card-bg);
            border-radius: 6px;
            font-family: -apple-system, sans-serif;
            font-size: 14px;
            user-select: none;
            display: flex;
            align-items: center;
            overflow: hidden;
        }
        .lkml-diff-details[open] > summary {
            border-bottom-left-radius: 0;
            border-bottom-right-radius: 0;
            border-bottom: 1px solid var(--border-color);
        }
        .diff-filename {
            font-weight: 600;
            margin-left: 6px;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
            flex-shrink: 1;
        }
        .diff-toggle-hint {
            font-size: 0.85em;
            color: var(--muted-text);
            margin-left: 12px;
            flex-shrink: 0;
        }
        .lkml-diff-content {
            font-family: "JetBrains Mono", "Fira Code", Consolas, "Courier New", monospace !important;
            font-size: 14px !important;
            margin: 0;
            padding: 12px 0;
            width: 100% !important;
            max-width: 100% !important;
            overflow-x: auto !important;
            white-space: pre !important;
        }
        .lkml-diff-content span {
            display: block !important;
            min-width: 100% !important;
            width: max-content !important;
            padding: 0 14px !important;
        }
        span.add { background-color: var(--diff-add-bg) !important; color: var(--diff-add-text) !important; }
        span.del { background-color: var(--diff-del-bg) !important; color: var(--diff-del-text) !important; }
        span.hunk { background-color: var(--diff-hunk-bg) !important; color: var(--diff-hunk-text) !important; padding-top: 4px !important; padding-bottom: 4px !important; }
        span.head { color: var(--muted-text) !important; font-weight: bold; }

        hr { display: none !important; }
        .lkml-email-text b { color: var(--text-color); font-size: 1.05em; }

        /* 切换按钮 */
        .lkml-toggle-btn {
            position: fixed;
            bottom: 20px;
            right: 20px;
            z-index: 9999;
            padding: 8px 16px;
            border-radius: 6px;
            border: 1px solid var(--border-color);
            background: var(--card-bg);
            color: var(--text-color);
            cursor: pointer;
            font-size: 14px;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
            box-shadow: 0 2px 8px rgba(0,0,0,0.12);
            transition: opacity 0.2s, transform 0.2s;
            user-select: none;
        }
        .lkml-toggle-btn:hover {
            transform: translateY(-1px);
            box-shadow: 0 4px 12px rgba(0,0,0,0.15);
        }
    `;

    GM_addStyle(css);

    function escapeHtml(str) {
        return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    }

    // ==========================================
    // 2. DOM 结构改造
    // ==========================================
    const preBlocks = document.querySelectorAll('pre');

    preBlocks.forEach(pre => {
        const isEmailBlock = pre.querySelector('a[id^="m"]');
        if (!isEmailBlock) {
            pre.classList.add('lkml-sys-block');
            return;
        }

        const card = document.createElement('div');
        card.className = 'lkml-email-card';

        let currentTextPre = document.createElement('pre');
        currentTextPre.className = 'lkml-email-text';
        card.appendChild(currentTextPre);

        let currentDetails = null;
        let diffContent = null;
        const nodes = Array.from(pre.childNodes);

        for (let i = 0; i < nodes.length; i++) {
            const node = nodes[i];

            if (node.nodeType === Node.ELEMENT_NODE && node.classList.contains('head')) {
                let filename = "Diff / Patch";
                const match = node.textContent.match(/diff --git a\/(.*?)\s+b\//);
                if (match) filename = match[1];

                currentDetails = document.createElement('details');
                currentDetails.className = 'lkml-diff-details';

                const summary = document.createElement('summary');
                summary.innerHTML = `📄 <span class="diff-filename">${filename}</span> <span class="diff-toggle-hint">(点击展开/折叠)</span>`;
                currentDetails.appendChild(summary);

                diffContent = document.createElement('pre');
                diffContent.className = 'lkml-diff-content';
                currentDetails.appendChild(diffContent);

                card.appendChild(currentDetails);
                diffContent.appendChild(node);

                currentTextPre = document.createElement('pre');
                currentTextPre.className = 'lkml-email-text';
            } else if (currentDetails) {
                if (node.nodeType === Node.TEXT_NODE && /(?:^|\n)--\s*\n/.test(node.textContent)) {
                    currentDetails = null;
                    diffContent = null;
                    card.appendChild(currentTextPre);
                    currentTextPre.appendChild(node);
                } else {
                    diffContent.appendChild(node);
                }
            } else {
                currentTextPre.appendChild(node);
            }
        }

        if (currentTextPre.childNodes.length > 0 && currentTextPre.parentNode !== card) {
            card.appendChild(currentTextPre);
        } else if (currentTextPre.childNodes.length === 0 && currentTextPre.parentNode === card) {
            card.removeChild(currentTextPre);
        }

        pre.parentNode.replaceChild(card, pre);

        // 提取线程导航和邮件头
        processEmailCard(card);
    });

    // ==========================================
    // 3. 提取线程导航 + 结构化邮件头
    // ==========================================
    function processEmailCard(card) {
        const textPres = card.querySelectorAll('.lkml-email-text');
        if (textPres.length === 0) return;

        const firstPre = textPres[0];
        const html = firstPre.innerHTML;

        // 匹配行首的 From: 且后面含 @（邮件头特征，排除正文中的 "From"）
        const fromMatch = html.match(/(^|\n)From:\s*\S.*@/);
        if (!fromMatch) return;

        const fromIdx = fromMatch.index + (fromMatch[0].startsWith('\n') ? 1 : 0);

        // 头部结束：From: 之后第一个 \n\n
        const headerEnd = html.indexOf('\n\n', fromIdx);
        if (headerEnd === -1) return;

        // From: 所在行的行首
        let navEnd = html.lastIndexOf('\n', fromIdx - 1);
        if (navEnd === -1) navEnd = 0;
        else navEnd++;

        const navHtml = html.substring(0, navEnd);
        const headerHtml = html.substring(fromIdx, headerEnd);
        const bodyHtml = html.substring(headerEnd + 2);

        // 正文放回 firstPre
        firstPre.innerHTML = bodyHtml;
        if (!bodyHtml.trim()) firstPre.remove();

        // --- 线程导航（可折叠） ---
        if (navHtml.trim()) {
            const navDetails = document.createElement('details');
            navDetails.className = 'lkml-thread-nav';
            const navSummary = document.createElement('summary');
            navSummary.textContent = '线程导航';
            navDetails.appendChild(navSummary);
            const navContent = document.createElement('pre');
            navContent.className = 'lkml-nav-content';
            navContent.innerHTML = navHtml;
            navDetails.appendChild(navContent);
            card.insertBefore(navDetails, card.firstChild);
        }

        // --- 结构化邮件头 ---
        const headerDiv = document.createElement('div');
        headerDiv.className = 'lkml-email-headers';

        const headerText = headerHtml.replace(/<[^>]+>/g, '');
        const headerLines = headerText.split('\n');

        headerLines.forEach(line => {
            const colonIdx = line.indexOf(':');
            if (colonIdx === -1) return;
            const key = line.substring(0, colonIdx).trim();
            const value = line.substring(colonIdx + 1).trim();
            if (!key || !value) return;

            const row = document.createElement('div');
            row.className = 'lkml-header-row';

            const label = document.createElement('span');
            label.className = 'lkml-header-label';
            label.textContent = key + ':';

            const val = document.createElement('span');
            val.className = 'lkml-header-value';

            if (key === 'From') {
                const fm = value.match(/^(.+?)\s*@\s*(.+?)(?:\s*\(.*\))?$/);
                if (fm) {
                    val.innerHTML = `<span class="lkml-header-from">${escapeHtml(fm[1].trim())}</span><span class="lkml-header-date">${escapeHtml(fm[2].trim())}</span>`;
                } else {
                    val.textContent = value;
                }
            } else if (key === 'To' || key === 'Cc') {
                value.split(/,\s*/).forEach(item => {
                    if (!item.trim()) return;
                    const tag = document.createElement('span');
                    tag.className = 'lkml-header-tag';
                    tag.textContent = item.trim();
                    val.appendChild(tag);
                });
            } else {
                val.textContent = value;
            }

            row.appendChild(label);
            row.appendChild(val);
            headerDiv.appendChild(row);
        });

        card.insertBefore(headerDiv, card.querySelector('.lkml-email-text, .lkml-diff-details'));
    }

    // ==========================================
    // 4. 文本模式 / 代码模式 切换
    // ==========================================
    const toggleBtn = document.createElement('button');
    toggleBtn.className = 'lkml-toggle-btn';
    toggleBtn.textContent = '文本模式';
    document.body.appendChild(toggleBtn);

    let isTextMode = localStorage.getItem('lkml-text-mode') === 'true';
    const originalHTML = new Map();

    function mergeSoftLineBreaks(text) {
        const paragraphs = text.split(/\n{2,}/);
        return paragraphs
            .map(p => p.split('\n').map(line => line.trimEnd()).join(' '))
            .join('\n\n');
    }

    function switchToTextMode() {
        document.querySelectorAll('.lkml-email-text').forEach(el => {
            if (!originalHTML.has(el)) {
                originalHTML.set(el, el.innerHTML);
            }
            const walker = document.createTreeWalker(el, NodeFilter.SHOW_TEXT);
            const textNodes = [];
            while (walker.nextNode()) textNodes.push(walker.currentNode);
            textNodes.forEach(node => {
                node.textContent = mergeSoftLineBreaks(node.textContent);
            });
            el.classList.add('lkml-text-mode');
        });
    }

    function switchToCodeMode() {
        originalHTML.forEach((html, el) => {
            if (el.parentNode) {
                el.innerHTML = html;
                el.classList.remove('lkml-text-mode');
            }
        });
        originalHTML.clear();
    }

    if (isTextMode) {
        toggleBtn.textContent = '代码模式';
        switchToTextMode();
    }

    toggleBtn.addEventListener('click', () => {
        isTextMode = !isTextMode;
        localStorage.setItem('lkml-text-mode', isTextMode);
        if (isTextMode) {
            toggleBtn.textContent = '代码模式';
            switchToTextMode();
        } else {
            toggleBtn.textContent = '文本模式';
            switchToCodeMode();
        }
    });

})();