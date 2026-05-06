// ==UserScript==
// @name         DeepSeek 最后回复复制器 | DeepSeek Last Reply Copier
// @namespace    http://tampermonkey.net/
// @version      4.2.0
// @description  支持普通模式(API完美格式)与联网模式(DOM完美链接/表格/引用等)自由切换。一键复制或选择任意历史回复。
// @author       Gemini
// @license      Custom License
// @match        https://*.deepseek.com/a/chat/s/*
// @grant        GM_setClipboard
// ==/UserScript==

(function() {
    'use strict';

    let allMessages = [];
    let dataReady = false;
    let currentMode = localStorage.getItem('ds_copy_mode') || 'normal'; // 'normal' | 'web'

    // ===== CSS & Top Bar =====
    const style = document.createElement('style');
    style.textContent = `
        .ds-select-mode .ds-message {
            cursor: pointer !important;
            transition: outline 0.2s, background-color 0.2s;
            border-radius: 8px;
        }
        .ds-select-mode .ds-message:hover {
            outline: 2px dashed #007bff;
            background-color: rgba(0, 123, 255, 0.05);
        }
        .ds-select-top-bar {
            position: fixed; top: 0; left: 0; right: 0; height: 40px;
            background: #007bff; color: white; display: none; align-items: center;
            justify-content: center; z-index: 10001; font-size: 14px; font-weight: 600;
            box-shadow: 0 2px 8px rgba(0,0,0,0.15); font-family: system-ui, -apple-system, sans-serif;
        }
    `;
    document.head.appendChild(style);

    const topBar = document.createElement('div');
    topBar.className = 'ds-select-top-bar';
    topBar.innerText = '🎯 请点击要复制的回复 | 点击空白处或按 Esc 取消';
    document.body.appendChild(topBar);

    // ===== XHR 拦截 =====
    const hook = () => {
        const open = XMLHttpRequest.prototype.open;
        XMLHttpRequest.prototype.open = function(m, url) {
            if (url.includes('history_messages?chat_session_id')) {
                arguments[1] = url.replace(/&cache_version=\d+/, '') + '&v=' + Date.now();
            }
            this.addEventListener('load', () => {
                if (this.responseURL.includes('history_messages')) {
                    try {
                        const res = JSON.parse(this.responseText);
                        if (res.data?.biz_data?.chat_messages?.length > 0) {
                            allMessages = res.data.biz_data.chat_messages;
                            dataReady = true;
                            updateButtonState();
                        }
                    } catch(e) {}
                }
            });
            open.apply(this, arguments);
        };
    };
    hook();

    // ===== 模式 1: 普通模式 (API 提取) =====
    function getMdFromAPI(apiMsg) {
        if (!apiMsg) return null;
        let body = '';
        (apiMsg.fragments || []).forEach(f => {
            if (f.type === 'RESPONSE' || f.type === 'REQUEST') {
                body += (f.content || '');
            }
        });
        body = body.trim();
        body = body.replace(/\[(?:reference|citation|ref):\d+\]/gi, '');
        return body || null;
    }

    // ===== 模式 2: 联网模式 (DOM 提取) =====
    function domToMarkdown(node) {
        if (node.nodeType === Node.TEXT_NODE) return node.textContent;
        if (node.nodeType !== Node.ELEMENT_NODE) return '';

        const tag = node.tagName.toLowerCase();
        
        // 忽略无用元素
        if (node.classList.contains('_2ed5dee')) return '';
        if (node.style.opacity === '0') return '';

        // 处理引用链接
        if (tag === 'a' && node.querySelector('.ds-markdown-cite')) {
            const href = node.getAttribute('href');
            const citeSpan = node.querySelector('.ds-markdown-cite');
            let num = '';
            citeSpan.querySelectorAll('span').forEach(s => {
                if (s.style.opacity !== '0' && s.textContent.trim()) num = s.textContent.trim();
            });
            if (!num) num = citeSpan.textContent.trim();
            if (href && num) return ` [[${num}](${href})]`;
            return '';
        }

        // 处理图片 (自闭合标签，无需递归子节点)
        if (tag === 'img') {
            const alt = node.getAttribute('alt') || '';
            const src = node.getAttribute('src') || '';
            return src ? `![${alt}](${src})` : '';
        }
        
        // 处理水平分割线 (自闭合标签)
        if (tag === 'hr') {
            return '---\n\n';
        }

        // 递归处理子节点
        let content = '';
        for (const child of node.childNodes) content += domToMarkdown(child);

        // 处理表格
        if (tag === 'table') {
            const rows = [];
            for (const tr of node.querySelectorAll('tr')) {
                const cells = [];
                for (const cell of tr.querySelectorAll('th, td')) {
                    cells.push(domToMarkdown(cell).replace(/\n/g, ' ').trim());
                }
                rows.push(cells);
            }
            if (rows.length === 0) return '';
            let md = '| ' + rows[0].join(' | ') + ' |\n';
            md += '| ' + rows[0].map(() => '---').join(' | ') + ' |\n';
            for (let i = 1; i < rows.length; i++) {
                md += '| ' + rows[i].join(' | ') + ' |\n';
            }
            return md + '\n';
        }

        // 处理引用块
        if (tag === 'blockquote') {
            const lines = content.trim().replace(/\n{2,}/g, '\n').split('\n');
            return lines.map(line => '> ' + line).join('\n') + '\n\n';
        }

        // 标题
        if (tag === 'h1') return '# ' + content.trim() + '\n\n';
        if (tag === 'h2') return '## ' + content.trim() + '\n\n';
        if (tag === 'h3') return '### ' + content.trim() + '\n\n';
        if (tag === 'h4') return '#### ' + content.trim() + '\n\n';
        if (tag === 'h5') return '##### ' + content.trim() + '\n\n';
        if (tag === 'h6') return '###### ' + content.trim() + '\n\n';

        // 段落
        if (tag === 'p' && node.classList.contains('ds-markdown-paragraph')) return content.trim() + '\n\n';

        // 文本格式
        if (tag === 'strong' || tag === 'b') return '**' + content + '**';
        if (tag === 'em' || tag === 'i') return '*' + content + '*';
        if (tag === 'del' || tag === 's' || tag === 'strike') return '~~' + content + '~~';

        // 列表
        if (tag === 'li') return '- ' + content.trim() + '\n';
        if (tag === 'ul' || tag === 'ol') return content + '\n';

        // 代码
        if (tag === 'code') return '`' + content + '`';
        if (tag === 'pre') return '```\n' + content + '\n```\n';

        // 换行
        if (tag === 'br') return '\n';

        return content;
    }

    function getMdFromDOM(msgElement) {
        if (!msgElement) return null;
        const markdownDoms = msgElement.querySelectorAll('.ds-markdown');
        if (markdownDoms.length > 0) {
            let result = '';
            for (const md of markdownDoms) {
                if (md.closest('.ds-think-content') || md.closest('.e4c3fd02') || md.closest('._60aa7fb')) continue;
                result += domToMarkdown(md);
            }
            return result.trim() || null;
        }
        return msgElement.textContent.trim() || null;
    }

    // ===== 指纹匹配 (普通模式选择时使用) =====
    function normalizeText(t) {
        return t.replace(/[\s\n]+/g, ' ').replace(/\[(?:reference|citation|ref):\d+\]/gi, '').trim();
    }

    function getMessageFingerprint(msgElement) {
        const markdownDoms = msgElement.querySelectorAll('.ds-markdown');
        for (const md of markdownDoms) {
            if (md.closest('.ds-think-content') || md.closest('.e4c3fd02') || md.closest('._60aa7fb')) continue;
            const text = md.textContent.trim();
            if (text) return normalizeText(text).substring(0, 60);
        }
        return normalizeText(msgElement.textContent).substring(0, 60);
    }

    function findMatchingMessage(msgElement) {
        if (!dataReady || allMessages.length === 0) return null;
        const fp = getMessageFingerprint(msgElement);
        if (!fp) return null;
        const fpShort = fp.substring(0, 40);
        for (const msg of allMessages) {
            let body = '';
            (msg.fragments || []).forEach(f => { body += (f.content || ''); });
            body = normalizeText(body);
            if (body && body.includes(fpShort)) return msg;
        }
        return null;
    }

    // ===== 主入口 =====
    function getReplyMd(msgElement) {
        if (!msgElement) return null;
        if (currentMode === 'normal') {
            if (!dataReady) return null;
            const apiMsg = findMatchingMessage(msgElement);
            return getMdFromAPI(apiMsg);
        } else {
            return getMdFromDOM(msgElement);
        }
    }

    function getLastReplyMd() {
        if (currentMode === 'normal') {
            if (!dataReady) return null;
            const assistantMsgs = allMessages.filter(m => m.role !== 'USER');
            if (assistantMsgs.length === 0) return null;
            return getMdFromAPI(assistantMsgs[assistantMsgs.length - 1]);
        } else {
            const messages = document.querySelectorAll('.ds-message');
            if (messages.length === 0) return null;
            return getMdFromDOM(messages[messages.length - 1]);
        }
    }

    // ===== 交互逻辑 =====
    function copyText(text, btn) {
        if (!text) { alert("暂无数据！请等待对话加载完成，或切换到联网模式。"); return; }
        if (typeof GM_setClipboard !== 'undefined') {
            GM_setClipboard(text, 'text');
            showSuccess(btn);
        } else {
            navigator.clipboard.writeText(text).then(() => showSuccess(btn)).catch(err => alert("复制失败: " + err));
        }
    }

    function toggleMode() {
        currentMode = currentMode === 'normal' ? 'web' : 'normal';
        localStorage.setItem('ds_copy_mode', currentMode);
        updateModeUI();
    }

    function enterSelectionMode() {
        if (currentMode === 'normal' && !dataReady) { alert("暂无数据！请等待对话加载完成，或切换到联网模式。"); return; }
        document.body.classList.add('ds-select-mode');
        topBar.style.display = 'flex';
    }

    function exitSelectionMode() {
        document.body.classList.remove('ds-select-mode');
        topBar.style.display = 'none';
    }

    document.addEventListener('click', (e) => {
        if (!document.body.classList.contains('ds-select-mode')) return;
        if (e.target.closest('#ds_copy_root') || e.target.closest('.ds-select-top-bar')) return;
        const msg = e.target.closest('.ds-message');
        if (msg) {
            e.stopPropagation();
            e.preventDefault();
            const md = getReplyMd(msg);
            if (md) {
                copyText(md, null);
                topBar.innerText = '✅ 复制成功！';
                setTimeout(() => { exitSelectionMode(); topBar.innerText = '🎯 请点击要复制的回复 | 点击空白处或按 Esc 取消'; }, 1000);
            } else {
                alert("该消息暂无数据，请等待加载或切换模式。");
            }
        } else {
            exitSelectionMode();
        }
    }, true);

    document.addEventListener('keydown', (e) => {
        if (e.key === 'Escape') exitSelectionMode();
    });

    // ===== UI 渲染 =====
    function updateButtonState() {
        const btn = document.getElementById('ds_copy_last_btn');
        if (btn && currentMode === 'normal') {
            btn.style.backgroundColor = dataReady ? '#007bff' : '#6c757d';
            btn.style.cursor = dataReady ? 'pointer' : 'not-allowed';
            btn.title = dataReady ? '点击复制最后回复' : '等待数据加载...';
        }
    }

    function updateModeUI() {
        const modeBtn = document.getElementById('ds_mode_btn');
        const copyBtn = document.getElementById('ds_copy_last_btn');
        if (!modeBtn || !copyBtn) return;

        if (currentMode === 'normal') {
            modeBtn.innerText = '📄 普通';
            modeBtn.style.backgroundColor = '#007bff';
            modeBtn.title = '当前: 普通模式 (API完美格式，无链接)\n点击切换到联网模式';
            copyBtn.style.backgroundColor = dataReady ? '#007bff' : '#6c757d';
            copyBtn.style.cursor = dataReady ? 'pointer' : 'not-allowed';
        } else {
            modeBtn.innerText = '🌐 联网';
            modeBtn.style.backgroundColor = '#28a745';
            modeBtn.title = '当前: 联网模式 (DOM完美链接/表格/引用等)\n点击切换到普通模式';
            copyBtn.style.backgroundColor = '#28a745';
            copyBtn.style.cursor = 'pointer';
        }
    }

    function createUI() {
        if (document.getElementById('ds_copy_root')) return;
        const root = document.createElement('div');
        root.id = 'ds_copy_root';
        Object.assign(root.style, {
            position: 'fixed', bottom: '40px', right: '40px', zIndex: '10000',
            display: 'flex', flexDirection: 'column', gap: '8px',
            opacity: '0.5', transition: 'opacity 0.3s'
        });

        const baseBtnStyle = {
            padding: '10px 16px', border: 'none', borderRadius: '8px',
            color: 'white', cursor: 'pointer',
            fontSize: '13px', fontWeight: '600', fontFamily: 'system-ui, -apple-system, sans-serif',
            boxShadow: '0 4px 12px rgba(0,0,0,0.15)', transition: 'all 0.2s',
            display: 'flex', alignItems: 'center', justifyContent: 'center', whiteSpace: 'nowrap',
            width: '100%', boxSizing: 'border-box'
        };

        const modeBtn = document.createElement('button');
        modeBtn.id = 'ds_mode_btn';
        Object.assign(modeBtn.style, baseBtnStyle);
        modeBtn.onclick = toggleMode;

        const copyLastBtn = document.createElement('button');
        copyLastBtn.id = 'ds_copy_last_btn';
        copyLastBtn.innerText = '📋 复制最后回复';
        Object.assign(copyLastBtn.style, baseBtnStyle);
        copyLastBtn.onclick = () => copyText(getLastReplyMd(), copyLastBtn);

        const selectBtn = document.createElement('button');
        selectBtn.id = 'ds_select_btn';
        selectBtn.innerText = '🎯 选择回复复制';
        Object.assign(selectBtn.style, baseBtnStyle);
        selectBtn.style.backgroundColor = '#6c757d';
        selectBtn.onmouseenter = () => selectBtn.style.backgroundColor = '#5a6268';
        selectBtn.onmouseleave = () => selectBtn.style.backgroundColor = '#6c757d';
        selectBtn.onclick = enterSelectionMode;

        root.onmouseenter = () => root.style.opacity = '1';
        root.onmouseleave = () => root.style.opacity = '0.5';

        root.appendChild(modeBtn);
        root.appendChild(copyLastBtn);
        root.appendChild(selectBtn);
        document.body.appendChild(root);

        updateModeUI();
        updateButtonState();
    }

    function showSuccess(btn) {
        if (!btn) return;
        const originalBg = btn.style.backgroundColor;
        const originalText = btn.innerText;
        btn.style.backgroundColor = '#ffc107';
        btn.style.color = '#000';
        btn.innerText = '✅ 已复制！';
        setTimeout(() => {
            btn.style.backgroundColor = originalBg;
            btn.style.color = 'white';
            btn.innerText = originalText;
        }, 2000);
    }

    window.addEventListener('load', () => { createUI(); setInterval(createUI, 3000); });
})();