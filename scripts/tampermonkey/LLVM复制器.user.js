// ==UserScript==
// @name         LLVM Discourse to Markdown
// @namespace    https://discourse.llvm.org/
// @version      1.3
// @description  Convert LLVM Discourse posts to flat Markdown and copy to clipboard
// @match        https://discourse.llvm.org/t/*
// @grant        GM_setClipboard
// ==/UserScript==

(function () {
    'use strict';

    /* ── 浮动按钮 ── */
    const btn = document.createElement('button');
    btn.textContent = 'Copy as Markdown';
    btn.style.cssText = `
        position:fixed; bottom:24px; right:24px; z-index:9999;
        background:#1E5AA8; color:#fff; border:none;
        padding:8px 16px; font:bold 13px Verdana,Geneva,sans-serif;
        cursor:pointer; box-shadow:0 2px 8px rgba(0,0,0,.15);
        transition:opacity .2s;
    `;
    btn.addEventListener('mouseenter', () => (btn.style.opacity = '.85'));
    btn.addEventListener('mouseleave', () => (btn.style.opacity = '1'));
    document.body.appendChild(btn);

    btn.addEventListener('click', async () => {
        btn.textContent = 'Loading...';
        btn.disabled = true;
        try {
            const md = await generateMarkdown();
            try { GM_setClipboard(md, 'text'); }
            catch (_) { navigator.clipboard.writeText(md); }
            showToast('Copied to clipboard!');
        } catch (e) {
            showToast('Error: ' + e.message);
            console.error(e);
        } finally {
            btn.textContent = 'Copy as Markdown';
            btn.disabled = false;
        }
    });

    /* ── 主生成逻辑 ── */
    async function generateMarkdown() {
        // 从 URL 提取 topic_id
        const match = location.pathname.match(/\/t\/(?:[^\/]+\/)?(\d+)(?:\/\d+)?/);
        if (!match) throw new Error('Could not determine topic ID');
        const topicId = match[1];

        const apiUrl = `https://discourse.llvm.org/t/${topicId}.json`;
        const resp = await fetch(apiUrl, { headers: { 'Accept': 'application/json', 'X-Requested-With': 'XMLHttpRequest' } });
        if (!resp.ok) throw new Error(`HTTP error! status: ${resp.status}`);
        const data = await resp.json();

        const title = data.title || document.title;

        // 从 DOM 获取分类和标签
        const cats = Array.from(document.querySelectorAll('.topic-category .badge-category__name')).map(c => c.textContent.trim());
        const tags = Array.from(document.querySelectorAll('.topic-category .discourse-tag')).map(t => t.textContent.trim());
        const forum = [...cats, ...tags].filter(Boolean).join(', ');

        const url = location.href;

        let md = `# ${title}\n\n`;
        const meta = [
            forum && `版块/标签: **${forum}**`,
            `**Link**: ${url}`
        ].filter(Boolean);
        if (meta.length) md += meta.join(' | ') + '\n\n';

        let posts = data.post_stream.posts;
        const stream = data.post_stream.stream;

        // 如果帖子未完全加载，分批请求剩余帖子
        if (posts.length < stream.length) {
            const loadedIds = new Set(posts.map(p => p.id));
            const missingIds = stream.filter(id => !loadedIds.has(id));

            const chunkSize = 20;
            for (let i = 0; i < missingIds.length; i += chunkSize) {
                const chunk = missingIds.slice(i, i + chunkSize);
                const params = chunk.map(id => `post_ids[]=${id}`).join('&');
                const chunkUrl = `https://discourse.llvm.org/t/${topicId}/posts.json?${params}`;
                const chunkResp = await fetch(chunkUrl, { headers: { 'Accept': 'application/json', 'X-Requested-With': 'XMLHttpRequest' } });
                if (chunkResp.ok) {
                    const chunkData = await chunkResp.json();
                    if (chunkData.post_stream && chunkData.post_stream.posts) {
                        posts = posts.concat(chunkData.post_stream.posts);
                    }
                }
            }
        }

        // 按 post_number 排序
        posts.sort((a, b) => a.post_number - b.post_number);

        posts.forEach(post => {
            const userName = post.username || '匿名';
            let time = '';
            if (post.created_at) {
                const d = new Date(post.created_at);
                time = d.toLocaleString('en-US', { month: 'short', day: 'numeric', year: 'numeric', hour: 'numeric', minute: '2-digit', hour12: true });
            }
            const postNum = post.post_number;

            // cooked 字段是后端渲染好的 HTML
            const contentHtml = post.cooked || '';
            const content = discourseHtmlToMarkdown(contentHtml);

            md += `**${userName}** | ${time}${postNum === 1 ? ' (主楼)' : ''}\n\n`;
            md += content + '\n\n';
        });

        return md.trim() + '\n';
    }

    /* ── Discourse HTML → Markdown ── */
    function discourseHtmlToMarkdown(htmlString) {
        if (!htmlString) return '';
        let h = htmlString;

        const codeBlocks = [];
        // 1. 提取多行代码块 <pre>...</pre>
        h = h.replace(/<pre(?:[^>]*)>([\s\S]*?)<\/pre>/gi, (_, c) => {
            let codeContent = c.replace(/<div class="codeblock-button-wrapper">[\s\S]*?<\/div>/gi, '');
            codeContent = codeContent.replace(/<[^>]+>/g, '');
            codeContent = decode(codeContent).trim();
            codeBlocks.push('\n\n```\n' + codeContent + '\n```\n\n');
            return `\n\n%%CODEBLOCK_${codeBlocks.length - 1}%%\n\n`;
        });

        // 2. 提取行内代码 <code>...</code>
        const inlineCodes = [];
        h = h.replace(/<code(?:[^>]*)>([\s\S]*?)<\/code>/gi, (_, c) => {
            const codeText = decode(c).trim();
            inlineCodes.push('`' + codeText + '`');
            return `%%INLINECODE_${inlineCodes.length - 1}%%`;
        });

        // 3. 处理 Discourse 的 quote (兼容包含多个 class 的情况)
        h = h.replace(/<aside[^>]*class=["'][^"']*\bquote\b[^"']*["'][^>]*>([\s\S]*?)<\/aside>/gi, (match, inner) => {
            let qUser = '匿名';
            // 优先从 aside 的 data-username 属性获取用户名
            const unameMatch = match.match(/data-username=["']([^"']+)["']/i);
            if (unameMatch) qUser = unameMatch[1];

            let qContent = '';
            // 尝试从 <div class="title"> 提取作为后备
            const titleMatch = inner.match(/<div[^>]*class=["'][^"']*\btitle\b[^"']*["'][^>]*>([\s\S]*?)<\/div>/i);
            if (titleMatch && !unameMatch) {
                let titleText = titleMatch[1].replace(/<img[^>]*>/gi, '').replace(/<[^>]+>/g, '').trim();
                const extractedUser = titleText.replace(/:$/, '').trim();
                if (extractedUser) qUser = extractedUser;
            }

            // 提取 blockquote 内容
            const blockMatch = inner.match(/<blockquote[^>]*>([\s\S]*?)<\/blockquote>/i);
            if (blockMatch) {
                qContent = processHtmlString(blockMatch[1], codeBlocks, inlineCodes);
            } else {
                let tempInner = inner;
                if (titleMatch) tempInner = tempInner.replace(titleMatch[0], '');
                qContent = processHtmlString(tempInner, codeBlocks, inlineCodes);
            }

            // 处理多行引用块
            qContent = qContent.split('\n').map(line => line.trim() ? '> ' + line : '>').join('\n');
            return `\n\n${qContent}\n>\n> 引用 | **${qUser}**\n\n`;
        });

        // 4. 处理其余普通 HTML 字符串
        return processHtmlString(h, codeBlocks, inlineCodes);
    }

    /* ── 通用 HTML 字符串处理 ── */
    function processHtmlString(h, codeBlocks = [], inlineCodes = []) {
        // 过滤 Emoji 图片
        h = h.replace(/<img[^>]*class=["'][^"']*\bemoji\b[^"']*["'][^>]*>/gi, '');
        h = h.replace(/<img[^>]*src=["'][^"']*emoji\.discourse-cdn\.com[^"']*["'][^>]*>/gi, '');

        // 提取灯箱图原图链接
        h = h.replace(/<div class="lightbox-wrapper">[\s\S]*?<a class="lightbox" href="([^"]+)"[\s\S]*?<\/a>[\s\S]*?<\/div>/gi, (_, orig) => `\n\n![](${orig})\n\n`);

        // 清理标题中的锚点链接
        h = h.replace(/<a\s+[^>]*class=["']anchor["'][^>]*><\/a>/gi, '');

        // 处理标题
        h = h.replace(/<h1[^>]*>([\s\S]*?)<\/h1>/gi, '\n\n# $1\n\n');
        h = h.replace(/<h2[^>]*>([\s\S]*?)<\/h2>/gi, '\n\n## $1\n\n');
        h = h.replace(/<h3[^>]*>([\s\S]*?)<\/h3>/gi, '\n\n### $1\n\n');
        h = h.replace(/<h4[^>]*>([\s\S]*?)<\/h4>/gi, '\n\n#### $1\n\n');
        h = h.replace(/<h5[^>]*>([\s\S]*?)<\/h5>/gi, '\n\n##### $1\n\n');
        h = h.replace(/<h6[^>]*>([\s\S]*?)<\/h6>/gi, '\n\n###### $1\n\n');

        // 处理表格
        h = h.replace(/<table[^>]*>([\s\S]*?)<\/table>/gi, (_, tableContent) => {
            let rows = [];
            const trRegex = /<tr[^>]*>([\s\S]*?)<\/tr>/gi;
            let trMatch;
            while ((trMatch = trRegex.exec(tableContent)) !== null) {
                let cells = [];
                const tdRegex = /<t[dh][^>]*>([\s\S]*?)<\/t[dh]>/gi;
                let tdMatch;
                while ((tdMatch = tdRegex.exec(trMatch[1])) !== null) {
                    let cellContent = tdMatch[1].replace(/<[^>]+>/g, '').trim();
                    cellContent = cellContent.replace(/\n/g, ' ');
                    cells.push(cellContent);
                }
                if (cells.length) rows.push(cells);
            }
            if (rows.length === 0) return '';
            let mdTable = '\n\n';
            mdTable += '| ' + rows[0].join(' | ') + ' |\n';
            mdTable += '|' + rows[0].map(() => '---').join('|') + '|\n';
            for (let i = 1; i < rows.length; i++) {
                mdTable += '| ' + rows[i].join(' | ') + ' |\n';
            }
            mdTable += '\n\n';
            return mdTable;
        });

        // 处理段落、换行、分割线
        h = h.replace(/<hr\s*\/?>/gi, '\n\n---\n\n');
        h = h.replace(/<p\s*\/?>/gi, '\n\n');
        h = h.replace(/<\/p>/gi, '\n\n');
        h = h.replace(/<br\s*\/?>/gi, '\n\n');

        // 处理粗体、斜体、删除线
        h = h.replace(/<(b|strong)>([\s\S]*?)<\/\1>/gi, '**$2**');
        h = h.replace(/<(i|em)>([\s\S]*?)<\/\1>/gi, '*$2*');
        h = h.replace(/<del>([\s\S]*?)<\/del>/gi, '~~$1~~');

        // 处理列表
        h = h.replace(/<li[^>]*>([\s\S]*?)<\/li>/gi, (_, c) => '- ' + c.replace(/\n/g, ' ').trim() + '\n');
        h = h.replace(/<\/?(ul|ol)[^>]*>/gi, '\n');

        // 处理图片和超链接
        h = h.replace(/<img[^>]*src=["']([^"']+)["'][^>]*>/gi, '![]($1)');
        h = h.replace(/<a\s+[^>]*href=["']([^"']+)["'][^>]*>([\s\S]*?)<\/a>/gi, '[$2]($1)');

        // 删除其余 HTML 标签
        h = h.replace(/<[^>]+>/g, '');

        // 解码 HTML 实体
        h = decode(h);

        // 清理多余的空行
        h = h.replace(/\n{3,}/g, '\n\n');

        // 恢复代码块
        h = h.replace(/%%CODEBLOCK_(\d+)%%/g, (_, i) => codeBlocks[parseInt(i)] || '');
        h = h.replace(/%%INLINECODE_(\d+)%%/g, (_, i) => inlineCodes[parseInt(i)] || '');

        // 再次清理恢复代码块后可能产生的多余空行
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