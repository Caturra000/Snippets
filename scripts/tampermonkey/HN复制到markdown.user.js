// ==UserScript==
// @name         HN Comments to Markdown
// @namespace    https://news.ycombinator.com/
// @version      1.3
// @description  Convert Hacker News comments to flat Markdown and copy to clipboard
// @match        https://news.ycombinator.com/item*
// @grant        GM_setClipboard
// ==/UserScript==

(function () {
    'use strict';

    /* ── 浮动按钮 ── */
    const btn = document.createElement('button');
    btn.textContent = 'Copy as Markdown';
    btn.style.cssText = `
        position:fixed; bottom:24px; right:24px; z-index:9999;
        background:#ff6600; color:#fff; border:none;
        padding:8px 16px; font:bold 13px Verdana,Geneva,sans-serif;
        cursor:pointer; box-shadow:0 2px 8px rgba(0,0,0,.15);
        transition:opacity .2s;
    `;
    btn.addEventListener('mouseenter', () => (btn.style.opacity = '.85'));
    btn.addEventListener('mouseleave', () => (btn.style.opacity = '1'));
    document.body.appendChild(btn);

    btn.addEventListener('click', () => {
        const md = generateMarkdown();
        try { GM_setClipboard(md, 'text'); }
        catch (_) { navigator.clipboard.writeText(md); }
        showToast('Copied to clipboard!');
    });

    /* ── 主生成逻辑 ── */
    function generateMarkdown() {
        const userMap = {};
        document.querySelectorAll('tr.athing.comtr').forEach(row => {
            const u = row.querySelector('.comhead .hnuser')?.textContent?.trim() || '';
            if (row.id && u) userMap[row.id] = u;
        });

        const titleEl = document.querySelector('.athing.submission .titleline a');
        const title = titleEl?.textContent?.trim() || '';
        const titleUrl = titleEl?.getAttribute('href') || '';
        const score = document.querySelector('.subline .score')?.textContent?.trim() || '';
        const user = document.querySelector('.subline .hnuser')?.textContent?.trim() || '';
        const postTime = document.querySelector('.subline .age a')?.textContent?.trim() || '';

        let md = `# ${title}\n\n`;
        if (titleUrl) md += `**Link**: ${titleUrl}\n\n`;
        const meta = [score, user && `by **${user}**`, postTime].filter(Boolean);
        if (meta.length) md += meta.join(' | ') + '\n\n';

        document.querySelectorAll('tr.athing.comtr').forEach(row => {
            const userName = row.querySelector('.comhead .hnuser')?.textContent?.trim() || '';
            const time = row.querySelector('.comhead .age a')?.textContent?.trim() || '';
            const id = row.id || '';

            const parentLink = row.querySelector('.comhead a.clicky[href^="#"]');
            let replyTag = '';
            if (parentLink) {
                const parentId = parentLink.getAttribute('href').replace('#', '');
                const parentUser = userMap[parentId];
                if (parentUser) replyTag = ` | 回复 @${parentUser}`;
            }

            const text = htmlToMarkdown(row.querySelector('.commtext'));

            md += `**${userName}** | ${time}${replyTag}\n\n`;
            md += text + '\n\n';
        });

        return md.trim() + '\n';
    }

    /* ── HTML → Markdown 片段 ── */
    function htmlToMarkdown(el) {
        if (!el) return '';
        let h = el.innerHTML;

        // 1. 提取代码块，用占位符替换，防止后续处理破坏代码块内的换行
        const codeBlocks = [];
        h = h.replace(/<pre>(?:\s*<code>)?([\s\S]*?)(?:<\/code>\s*)?<\/pre>/gi, (_, c) => {
            codeBlocks.push('\n\n```\n' + decode(c.trim()) + '\n```\n\n');
            return `\n\n%%CODEBLOCK_${codeBlocks.length - 1}%%\n\n`;
        });

        // 2. 处理行内代码
        h = h.replace(/<code>([\s\S]*?)<\/code>/gi, (_, c) =>
            '`' + decode(c.trim()) + '`');

        // 3. 处理链接
        h = h.replace(/<a\s+href="([^"]*)"[^>]*>([\s\S]*?)<\/a>/gi, (_, href, t) =>
            '[' + t.replace(/<[^>]+>/g, '').trim() + '](' + href + ')');

        // 4. 段落与换行
        h = h.replace(/<p\s*\/?>/gi, '\n\n');
        h = h.replace(/<\/p>/gi, '');
        h = h.replace(/<br\s*\/?>/gi, '\n\n');

        // 5. 斜体 / 粗体
        h = h.replace(/<(i|em)>([\s\S]*?)<\/\1>/gi, '*$2*');
        h = h.replace(/<(b|strong)>([\s\S]*?)<\/\1>/gi, '**$2**');

        // 6. 移除残余标签
        h = h.replace(/<[^>]+>/g, '');

        // 7. 解码 HTML 实体
        h = decode(h);

        // 8. 清理软换行（段落内部的单个 \n 变空格）
        h = h.replace(/([^\n])\n([^\n])/g, '$1 $2');

        // 9. 合并多余空行
        h = h.replace(/\n{3,}/g, '\n\n');

        // 10. 恢复代码块
        h = h.replace(/%%CODEBLOCK_(\d+)%%/g, (_, i) => codeBlocks[parseInt(i)]);

        // 11. 再次合并可能产生的多余空行
        h = h.replace(/\n{3,}/g, '\n\n');

        return h.trim();
    }

    /* ── HTML 实体解码 ── */
    function decode(text) {
        const ta = document.createElement('textarea');
        ta.innerHTML = text;
        return ta.value;
    }

    /* ── Toast 提示 ── */
    function showToast(msg) {
        const t = document.createElement('div');
        t.textContent = msg;
        t.style.cssText = `
            position:fixed; bottom:70px; right:24px; z-index:9999;
            background:#333; color:#fff; padding:8px 16px;
            font:13px Verdana,Geneva,sans-serif;
            box-shadow:0 2px 8px rgba(0,0,0,.2);
            transition:opacity .4s;
        `;
        document.body.appendChild(t);
        setTimeout(() => { t.style.opacity = '0'; setTimeout(() => t.remove(), 400); }, 1500);
    }
})();