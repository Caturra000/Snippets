// ==UserScript==
// @name         Arena.ai 终极体验优化增强版 (增量渲染不弹条版)
// @namespace    http://tampermonkey.net/
// @version      9.0
// @description  增量渲染解决侧边栏滚动重置问题、磁吸抽屉、无缝毛玻璃物理吸顶、精确置顶
// @author       AI Assistant
// @match        *://*.arena.ai/*
// @match        *://chat.lmsys.org/*
// @grant        GM_addStyle
// ==/UserScript==

(function() {
    'use strict';

    // ==========================================
    // 1. 注入自定义 CSS (加入滚动隔离防冒泡)
    // ==========================================
    GM_addStyle(`
        /* ---------------- 对话框间距 (解决跳转被遮挡) ---------------- */
        .group.flex.flex-col {
            scroll-margin-top: 80px !important;
        }

        /* ---------------- Minimap 磁吸边缘抽屉样式 ---------------- */
        #tm-arena-minimap {
            position: fixed;
            right: 0;
            top: 100px;
            bottom: 50px; /* 固定上下边界，确保内容区可以完美溢出滚动 */
            background: rgba(250, 250, 250, 0.75);
            backdrop-filter: blur(16px);
            -webkit-backdrop-filter: blur(16px);
            border: 1px solid rgba(128, 128, 128, 0.2);
            border-right: none;
            border-radius: 16px 0 0 16px;
            z-index: 9999;
            box-shadow: -4px 4px 16px rgba(0,0,0,0.1);
            color: #333;
            display: flex;
            flex-direction: row;

            /* 抽屉收纳动画 */
            transform: translateX(calc(100% - 36px));
            transition: transform 0.4s cubic-bezier(0.16, 1, 0.3, 1), box-shadow 0.4s ease;
        }

        html.dark #tm-arena-minimap {
            background: rgba(30, 30, 30, 0.75);
            color: #eee;
        }

        #tm-arena-minimap:hover {
            transform: translateX(0);
            box-shadow: -12px 12px 32px rgba(0,0,0,0.15);
        }

        /* 左侧把手区域 */
        #tm-minimap-handle {
            width: 36px;
            flex-shrink: 0;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            border-right: 1px solid rgba(128, 128, 128, 0.1);
            cursor: pointer;
        }
        #tm-minimap-handle span {
            writing-mode: vertical-rl;
            margin-top: 12px;
            font-size: 12px;
            font-weight: bold;
            letter-spacing: 4px;
            color: rgba(128, 128, 128, 0.8);
        }

        /* 右侧实际内容区域 */
        #tm-minimap-content {
            width: 240px;
            padding: 12px;
            height: 100%;
            overflow-y: auto;
            overscroll-behavior: contain; /* 核心：防止在侧边栏滚动时带动底层网页滚动 */
        }

        #tm-minimap-content h3 {
            margin: 0 0 10px 0;
            font-size: 14px;
            font-weight: bold;
            opacity: 0.8;
            border-bottom: 1px solid rgba(128, 128, 128, 0.2);
            padding-bottom: 6px;
        }

        /* 美化滚动条 */
        #tm-minimap-content::-webkit-scrollbar { width: 6px; }
        #tm-minimap-content::-webkit-scrollbar-thumb {
            background: rgba(128, 128, 128, 0.3);
            border-radius: 3px;
        }

        /* Minimap 条目样式 */
        .tm-minimap-item {
            padding: 8px;
            margin-bottom: 6px;
            font-size: 12px;
            cursor: pointer;
            border-radius: 6px;
            background: rgba(128, 128, 128, 0.08);
            transition: all 0.2s ease;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
            display: flex;
            align-items: center;
        }
        .tm-minimap-item.is-user { border-left: 3px solid #10a37f; }
        .tm-minimap-item.is-ai { border-left: 3px solid #8b5cf6; }
        .tm-minimap-item:hover {
            background: rgba(128, 128, 128, 0.2);
            transform: translateX(-2px);
        }

        /* ---------------- 用户提问：悬浮复制按钮 ---------------- */
        .tm-user-copy-btn {
            position: absolute;
            left: -20px;
            top: 0;
            background: #10a37f;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 6px 12px;
            font-size: 12px;
            font-weight: bold;
            cursor: pointer;
            opacity: 0.2;
            transition: opacity 0.2s ease, background 0.2s ease;
            box-shadow: 0 2px 8px rgba(0,0,0,0.2);
            display: inline-flex;
            align-items: center;
            gap: 4px;
            z-index: 50;
            will-change: transform;
        }
        .group.self-end:hover .tm-user-copy-btn { opacity: 1; }
        .tm-user-copy-btn:hover { background: #0d8a6a !important; opacity: 1 !important; }

        /* ---------------- 代码框增强 (无缝毛玻璃) ---------------- */
        .tm-code-header-enhanced {
            position: relative;
            z-index: 40 !important;
            will-change: transform;
            transition: box-shadow 0.2s ease, background-color 0.2s ease;
            background-color: var(--surface-primary, #ffffff);
        }
        html.dark .tm-code-header-enhanced { background-color: var(--surface-primary, #1e1e1e); }

        .tm-code-header-enhanced.is-sticking {
            background-color: rgba(250, 250, 250, 0.8) !important;
            backdrop-filter: blur(12px);
            -webkit-backdrop-filter: blur(12px);
            box-shadow: 0 -1px 0 rgba(250, 250, 250, 0.8), 0 4px 12px rgba(0,0,0,0.12);
        }
        html.dark .tm-code-header-enhanced.is-sticking {
            background-color: rgba(30, 30, 30, 0.8) !important;
            box-shadow: 0 -1px 0 rgba(30, 30, 30, 0.8), 0 4px 12px rgba(0,0,0,0.4);
        }

        .tm-code-actions-container { display: flex; gap: 6px; margin-left: auto; }
        .tm-code-action-btn {
            background: transparent; color: inherit; border: 1px solid rgba(128, 128, 128, 0.3);
            border-radius: 4px; padding: 3px 8px; font-size: 11px; cursor: pointer; opacity: 0.8;
            transition: all 0.2s; display: flex; align-items: center; gap: 4px; font-family: monospace;
        }
        .tm-code-action-btn:hover { opacity: 1; background: rgba(128, 128, 128, 0.15); }
    `);

    // ==========================================
    // 2. 纯享版物理吸顶引擎
    // ==========================================
    function runStickyEngine() {
        requestAnimationFrame(runStickyEngine);

        const scrollViewport = document.querySelector('[data-radix-scroll-area-viewport]') || window;
        const viewportTop = scrollViewport === window ? 0 : scrollViewport.getBoundingClientRect().top;

        // 1. 处理所有代码框头部
        const codeBlocks = document.querySelectorAll('div[data-code-block="true"].tm-code-processed');
        codeBlocks.forEach(block => {
            const header = block.querySelector('.tm-code-header-enhanced');
            const codeContainer = block.querySelector('.code-block_container__lbMX4') || block.querySelector('pre')?.parentElement;
            if (!header || !codeContainer) return;

            if (codeContainer.style.display === 'none') {
                header.style.transform = 'translateY(0px)';
                header.classList.remove('is-sticking');
                return;
            }

            const blockRect = block.getBoundingClientRect();

            if (blockRect.top < viewportTop && blockRect.bottom > viewportTop + header.offsetHeight) {
                const offset = (viewportTop - blockRect.top) - 1; // 1px密封防穿透
                const maxOffset = blockRect.height - header.offsetHeight;
                header.style.transform = `translateY(${Math.min(offset, maxOffset)}px)`;
                if (!header.classList.contains('is-sticking')) header.classList.add('is-sticking');
            } else {
                header.style.transform = 'translateY(0px)';
                if (header.classList.contains('is-sticking')) header.classList.remove('is-sticking');
            }
        });

        // 2. 处理提问复制按钮
        const userMessages = document.querySelectorAll('.group.self-end.tm-copy-processed');
        userMessages.forEach(msgBox => {
            const copyBtn = msgBox.querySelector('.tm-user-copy-btn');
            if (!copyBtn) return;
            const msgRect = msgBox.getBoundingClientRect();
            const targetTop = viewportTop + 10;
            if (msgRect.top < targetTop && msgRect.bottom > targetTop + copyBtn.offsetHeight) {
                const offset = targetTop - msgRect.top;
                const maxOffset = msgRect.height - copyBtn.offsetHeight - 10;
                copyBtn.style.transform = `translateY(${Math.min(offset, maxOffset)}px)`;
            } else {
                copyBtn.style.transform = 'translateY(0px)';
            }
        });
    }

    // ==========================================
    // 3. Minimap 增量渲染逻辑 (彻底解决滚不下去的问题)
    // ==========================================
    let updateMinimapTimeout = null;
    function updateMinimap() {
        if (updateMinimapTimeout) clearTimeout(updateMinimapTimeout);
        updateMinimapTimeout = setTimeout(() => {

            if (!document.getElementById('tm-arena-minimap')) {
                const minimap = document.createElement('div');
                minimap.id = 'tm-arena-minimap';
                minimap.innerHTML = `
                    <div id="tm-minimap-handle">🗺️<span>导航</span></div>
                    <div id="tm-minimap-content">
                        <div id="tm-minimap-list"></div>
                    </div>
                `;
                document.body.appendChild(minimap);
            }

            const listContainer = document.getElementById('tm-minimap-list');
            const ol = document.querySelector('ol');
            if (!ol) return;

            let messageNodes = Array.from(ol.querySelectorAll(':scope > div')).filter(node => node.querySelector('.prose'));
            if (messageNodes.length === 0) {
                document.getElementById('tm-arena-minimap').style.display = 'none';
                return;
            }
            document.getElementById('tm-arena-minimap').style.display = 'flex';

            messageNodes.reverse();

            // 【核心修复】：如果对话条数发生变化，才重构 DOM 树（创建空白节点）；
            // 否则绝对不破坏原有 DOM，从而保护原有的 scrollTop 滚动位置！
            if (listContainer.children.length !== messageNodes.length) {
                listContainer.innerHTML = '';
                messageNodes.forEach(() => {
                    const item = document.createElement('div');
                    listContainer.appendChild(item);
                });
            }

            // 增量式更新文字和属性
            messageNodes.forEach((node, index) => {
                node.classList.add('group', 'flex', 'flex-col');

                const item = listContainer.children[index];
                if (!item) return;

                const isUser = node.querySelector('.self-end') !== null;
                const prose = node.querySelector('.prose');
                let text = prose ? prose.innerText.trim().replace(/\n/g, ' ') : '';

                let title = isUser ? `我: ${text.substring(0, 15)}...` : `bot: ${text.substring(0, 15)}...`;
                const className = `tm-minimap-item ${isUser ? 'is-user' : 'is-ai'}`;

                // 仅当文字或样式改变时才赋值（极致性能优化）
                if (item.innerText !== title) {
                    item.innerText = title;
                    item.title = text;
                }
                if (item.className !== className) {
                    item.className = className;
                }

                // 确保点击事件生效
                item.onclick = () => {
                    node.scrollIntoView({ behavior: 'smooth', block: 'start' });
                    node.style.transition = 'filter 0.3s, background-color 0.3s';
                    node.style.filter = 'brightness(1.3)';
                    node.style.backgroundColor = 'rgba(128,128,128,0.1)';
                    setTimeout(() => { node.style.filter = 'none'; node.style.backgroundColor = ''; }, 800);
                };
            });
        }, 100); // 防抖时间减至 100ms，让更新更加实时
    }

    // ==========================================
    // 4. DOM 元素注入 (代码工具栏 & 复制按钮)
    // ==========================================
    function injectElements() {
        const codeBlocks = document.querySelectorAll('div[data-code-block="true"]:not(.tm-code-processed)');
        codeBlocks.forEach(block => {
            block.classList.add('tm-code-processed');
            block.style.overflow = 'visible';

            const header = block.querySelector('.border-b');
            if (!header) return;

            header.classList.add('tm-code-header-enhanced');

            const langNode = header.querySelector('.text-text-secondary.text-sm.font-medium');
            const lang = langNode ? langNode.innerText.trim().toLowerCase() : 'txt';

            const actionsDiv = document.createElement('div');
            actionsDiv.className = 'tm-code-actions-container';

            const foldBtn = createActionBtn('➖ 折叠', () => {
                const preContainer = block.querySelector('.code-block_container__lbMX4') || block.querySelector('pre')?.parentElement;
                if(!preContainer) return;
                const isHidden = preContainer.style.display === 'none';
                preContainer.style.display = isHidden ? '' : 'none';
                foldBtn.innerText = isHidden ? '➖ 折叠' : '➕ 展开';
                if(!isHidden) {
                    header.style.transform = 'translateY(0px)';
                    header.classList.remove('is-sticking');
                }
            });

            const copyBtn = createActionBtn('📋 复制', () => {
                const currentPre = block.querySelector('pre');
                const text = currentPre ? currentPre.textContent : '';
                navigator.clipboard.writeText(text).then(() => {
                    const originHtml = copyBtn.innerHTML;
                    copyBtn.innerText = '✅ 成功';
                    copyBtn.style.color = '#10a37f';
                    copyBtn.style.borderColor = '#10a37f';
                    setTimeout(() => { copyBtn.innerHTML = originHtml; copyBtn.style.color = ''; copyBtn.style.borderColor = ''; }, 2000);
                });
            });

            const downloadBtn = createActionBtn('💾 下载', () => {
                const currentPre = block.querySelector('pre');
                const text = currentPre ? currentPre.textContent : '';
                const extMap = { 'bash':'sh', 'python':'py', 'javascript':'js', 'typescript':'ts', 'html':'html', 'css':'css', 'json':'json', 'java':'java', 'c++':'cpp', 'sql':'sql' };
                const ext = extMap[lang] || 'txt';
                const blob = new Blob([text], { type: 'text/plain;charset=utf-8' });
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = `code_${Date.now()}.${ext}`;
                a.click();
                URL.revokeObjectURL(url);
            });

            actionsDiv.appendChild(foldBtn);
            actionsDiv.appendChild(copyBtn);
            actionsDiv.appendChild(downloadBtn);
            header.appendChild(actionsDiv);
        });

        const userMessages = document.querySelectorAll('.group.self-end:not(.tm-copy-processed)');
        userMessages.forEach(msgBox => {
            msgBox.classList.add('tm-copy-processed');
            msgBox.style.position = 'relative';

            const copyBtn = document.createElement('button');
            copyBtn.className = 'tm-user-copy-btn';
            copyBtn.innerHTML = '📋';
            copyBtn.title = "复制提问内容";

            copyBtn.onclick = (e) => {
                e.stopPropagation();
                const currentProse = msgBox.querySelector('.prose');
                const text = currentProse ? currentProse.innerText : '';
                navigator.clipboard.writeText(text).then(() => {
                    const originalHtml = copyBtn.innerHTML;
                    copyBtn.innerHTML = '✅';
                    copyBtn.style.background = '#059669';
                    setTimeout(() => { copyBtn.innerHTML = originalHtml; copyBtn.style.background = '#10a37f'; }, 2000);
                });
            };
            msgBox.appendChild(copyBtn);
        });
    }

    function createActionBtn(text, onClick) {
        const btn = document.createElement('button');
        btn.className = 'tm-code-action-btn';
        btn.innerText = text;
        btn.onclick = (e) => { e.preventDefault(); e.stopPropagation(); onClick(); };
        return btn;
    }

    // ==========================================
    // 5. 启动入口
    // ==========================================
    const observer = new MutationObserver(() => {
        injectElements();
        updateMinimap();
    });

    function init() {
        injectElements();
        updateMinimap();
        observer.observe(document.body, { childList: true, subtree: true });
        requestAnimationFrame(runStickyEngine);
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }
})();