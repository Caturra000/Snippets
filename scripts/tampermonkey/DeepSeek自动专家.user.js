// ==UserScript==
// @name         DeepSeek 自动切换专家模式和关闭联网模式
// @namespace    http://tampermonkey.net/
// @version      1.0
// @description  在 chat.deepseek.com 自动开启专家模式并关闭联网模式
// @author       MiMo
// @match        https://chat.deepseek.com/*
// @grant        none
// @run-at       document-idle
// ==/UserScript==

(function () {
    'use strict';

    // 模拟真实点击
    function simulateClick(element) {
        element.dispatchEvent(new MouseEvent('mousedown', { bubbles: true, cancelable: true }));
        element.dispatchEvent(new MouseEvent('mouseup', { bubbles: true, cancelable: true }));
        element.dispatchEvent(new MouseEvent('click', { bubbles: true, cancelable: true }));
    }

    // 查找包含指定文本的按钮
    function findButtonByText(texts) {
        const allElements = document.querySelectorAll('div[role="button"], div[class*="button"], div[class*="toggle"]');
        for (const el of allElements) {
            const text = el.textContent.trim();
            if (texts.some(t => text.includes(t))) {
                return el;
            }
        }
        return null;
    }

    // 查找专家模式按钮（通过 svg 图标或父元素）
    function findExpertButton() {
        // 方法1：通过文字查找
        const expertTexts = ['Expert', '专家模式'];
        const allDivs = document.querySelectorAll('div');
        for (const div of allDivs) {
            const text = div.textContent.trim();
            if (expertTexts.includes(text) || expertTexts.some(t => text === t)) {
                // 向上查找可点击的父元素
                let target = div;
                while (target && !target.getAttribute('role') && target.tagName !== 'BUTTON') {
                    target = target.parentElement;
                    if (target === document.body) break;
                }
                return target !== document.body ? target : div;
            }
        }

        // 方法2：通过包含特定 svg 的元素查找
        const svgs = document.querySelectorAll('svg');
        for (const svg of svgs) {
            const path = svg.querySelector('path');
            if (path && path.getAttribute('d')?.includes('7.99969 2.14671')) {
                let target = svg.closest('div[class]');
                while (target && target.children.length > 0) {
                    const parent = target.parentElement;
                    if (parent && parent.textContent.trim().match(/Expert|专家模式/)) {
                        return parent;
                    }
                    target = parent;
                }
            }
        }

        return null;
    }

    // 查找联网模式按钮
    function findSearchButton() {
        const searchTexts = ['Search', '智能搜索'];
        const toggleButtons = document.querySelectorAll('div[class*="toggle-button"], div[role="button"]');

        for (const btn of toggleButtons) {
            const text = btn.textContent.trim();
            if (searchTexts.some(t => text.includes(t))) {
                return btn;
            }
        }

        return null;
    }

    // 执行切换操作
    function switchModes() {
        console.log('[DeepSeek Helper] 开始检查模式状态...');

        // 切换专家模式
        const expertBtn = findExpertButton();
        if (expertBtn) {
            const isSelected = expertBtn.classList.contains('selected') ||
                              expertBtn.querySelector('.selected') !== null ||
                              expertBtn.querySelector('[class*="selected"]') !== null;

            // 检查父元素或自身是否有 selected 样式
            const hasSelectedStyle = expertBtn.closest('[class*="selected"]') !== null;

            if (!isSelected && !hasSelectedStyle) {
                console.log('[DeepSeek Helper] 点击开启专家模式');
                simulateClick(expertBtn);
            } else {
                console.log('[DeepSeek Helper] 专家模式已开启');
            }
        } else {
            console.log('[DeepSeek Helper] 未找到专家模式按钮');
        }

        // 关闭联网模式
        const searchBtn = findSearchButton();
        if (searchBtn) {
            const isSelected = searchBtn.classList.contains('ds-toggle-button--selected') ||
                              searchBtn.querySelector('.ds-toggle-button--selected') !== null ||
                              searchBtn.getAttribute('aria-checked') === 'true';

            if (isSelected) {
                console.log('[DeepSeek Helper] 点击关闭联网模式');
                simulateClick(searchBtn);
            } else {
                console.log('[DeepSeek Helper] 联网模式已关闭');
            }
        } else {
            console.log('[DeepSeek Helper] 未找到联网模式按钮');
        }
    }

    // 等待页面加载完成
    function waitForPageLoad() {
        return new Promise((resolve) => {
            if (document.readyState === 'complete') {
                resolve();
            } else {
                window.addEventListener('load', resolve);
            }
        });
    }

    // 主函数
    async function main() {
        await waitForPageLoad();

        // 延迟执行，确保动态元素加载完成
        setTimeout(() => {
            switchModes();
        }, 2000);

        // 监听 URL 变化（SPA 路由）
        let lastUrl = location.href;
        const observer = new MutationObserver(() => {
            if (location.href !== lastUrl) {
                lastUrl = location.href;
                setTimeout(switchModes, 2000);
            }
        });

        observer.observe(document, { subtree: true, childList: true });

        // 监听 DOM 变化，等待按钮出现
        let retryCount = 0;
        const maxRetries = 10;
        const retryInterval = setInterval(() => {
            retryCount++;
            const expertBtn = findExpertButton();
            const searchBtn = findSearchButton();

            if (expertBtn && searchBtn) {
                switchModes();
                clearInterval(retryInterval);
            } else if (retryCount >= maxRetries) {
                clearInterval(retryInterval);
            }
        }, 1000);
    }

    main();
})();