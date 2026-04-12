// ==UserScript==
// @name         DeepSeek 最后回复复制器 | DeepSeek Last Reply Copier
// @namespace    http://tampermonkey.net/
// @version      1.0.0
// @description  在右下角提供一个按钮，一键复制 DeepSeek 最后一个回复的 Markdown 文本（不包含思维过程）。Provides a button to copy the last DeepSeek reply in Markdown (excluding thinking).
// @author       Gemini
// @license      Custom License
// @match        https://*.deepseek.com/a/chat/s/*
// @grant        GM_setClipboard
// ==/UserScript==

(function() {
    'use strict';

    let state = {
        targetResponse: null
    };

    // 提取最后一次回复的 Markdown（不包含思考过程）
    function getLastReplyMd() {
        if (!state.targetResponse) return null;
        try {
            const data = JSON.parse(state.targetResponse);
            const bizData = data.data.biz_data;
            const messages = bizData.chat_messages;

            // 过滤出所有助手的回复
            const assistantMessages = messages.filter(msg => msg.role !== 'USER');
            if (assistantMessages.length === 0) return null;

            // 获取最后一次回复
            const lastMsg = assistantMessages[assistantMessages.length - 1];
            let body = "";
            let citations = {};

            (lastMsg.fragments || []).forEach(f => {
                // 仅保留 RESPONSE 或 REQUEST 类型，丢弃 THINK (思维过程)
                if (f.type === 'RESPONSE' || f.type === 'REQUEST') {
                    body += (f.content || "");
                } else if (f.type === 'SEARCH' && f.results) {
                    f.results.forEach(r => {
                        if (r.cite_index) citations[r.cite_index] = r.url;
                    });
                }
            });

            // 格式化引用
            body = body.replace(/\[citation:(\d+)\]/g, (m, id) => citations[id] ? ` [[${id}]](${citations[id]})` : m);
            // 格式化数学公式
            body = body.replace(/\$\$(.*?)\$\$/gs, (m, f) => f.includes('\n') ? `\n$$\n${f.trim()}\n$$\n` : `$$${f}$$`);

            return body.trim();
        } catch (e) {
            console.error("Parse failed: " + e.message);
            return null;
        }
    }

    // 创建右下角 UI
    function createUI() {
        if (document.getElementById('ds_copy_root')) { updateStatus(); return; }

        const root = document.createElement('div');
        root.id = 'ds_copy_root';
        Object.assign(root.style, {
            position: 'fixed',
            bottom: '40px',   // 固定在右下角
            right: '40px',
            zIndex: '10000',
            display: 'flex',
            opacity: '0.5',
            transition: 'opacity 0.3s'
        });

        const copyBtn = document.createElement('button');
        copyBtn.id = 'ds_copy_btn';
        copyBtn.innerText = '📋 复制最后回复';
        Object.assign(copyBtn.style, {
            padding: '12px 20px',
            border: 'none',
            borderRadius: '8px',
            backgroundColor: '#007bff',
            color: 'white',
            cursor: 'pointer',
            fontSize: '14px',
            fontWeight: '600',
            fontFamily: 'system-ui, -apple-system, sans-serif',
            boxShadow: '0 4px 12px rgba(0,0,0,0.15)',
            transition: 'all 0.2s',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center'
        });

        // 绑定点击事件
        copyBtn.onclick = () => {
            const mdText = getLastReplyMd();
            if (mdText) {
                // 优先使用油猴自带的剪贴板 API，更加稳定
                if (typeof GM_setClipboard !== 'undefined') {
                    GM_setClipboard(mdText, 'text');
                    showSuccess(copyBtn);
                } else {
                    navigator.clipboard.writeText(mdText).then(() => {
                        showSuccess(copyBtn);
                    }).catch(err => alert("复制失败: " + err));
                }
            } else {
                alert("暂无数据！请等待对话加载完成。");
            }
        };

        // 鼠标悬停动画
        root.onmouseenter = () => root.style.opacity = '1';
        root.onmouseleave = () => root.style.opacity = '0.5';

        root.appendChild(copyBtn);
        document.body.appendChild(root);
        updateStatus();
    }

    // 成功复制后的按钮状态变化
    function showSuccess(btn) {
        const originalBg = btn.style.backgroundColor;
        const originalText = btn.innerText;
        btn.style.backgroundColor = '#28a745';
        btn.innerText = '✅ 已复制！';
        setTimeout(() => {
            btn.style.backgroundColor = originalBg;
            btn.innerText = originalText;
        }, 2000);
    }

    // 更新按钮可用状态
    function updateStatus() {
        let hasData = false;
        try {
            if (state.targetResponse) {
                const data = JSON.parse(state.targetResponse);
                hasData = data.data.biz_data.chat_messages.filter(m => m.role !== 'USER').length > 0;
            }
        } catch (e) {}

        const btn = document.getElementById('ds_copy_btn');
        if (btn) {
            btn.style.backgroundColor = hasData ? '#007bff' : '#6c757d';
            btn.title = hasData ? '点击复制最后回复' : '等待数据加载...';
            btn.style.cursor = hasData ? 'pointer' : 'not-allowed';
        }
    }

    // 拦截 XHR 请求，获取后台数据
    const hook = () => {
        const open = XMLHttpRequest.prototype.open;
        XMLHttpRequest.prototype.open = function(m, url) {
            if (url.includes('history_messages?chat_session_id')) {
                arguments[1] = url.replace(/&cache_version=\d+/, '') + '&v=' + Date.now();
            }
            this.addEventListener('load', () => {
                if (this.responseURL.includes('history_messages')) {
                    const res = JSON.parse(this.responseText);
                    if (res.data?.biz_data?.chat_messages?.length > 0) {
                        state.targetResponse = this.responseText;
                        updateStatus();
                    }
                }
            });
            open.apply(this, arguments);
        };
    };

    hook();
    window.addEventListener('load', () => {
        createUI();
        setInterval(createUI, 2000);
    });
})();