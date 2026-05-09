// ==UserScript==
// @name         Zhihu Markdown 导出（含原图）
// @namespace    https://yourname.dev/
// @version      0.3.3
// @description  导出知乎专栏文章和问答为 Markdown，支持表格、删除线、数学公式等
// @author       you
// @match        https://www.zhihu.com/*
// @match        https://zhuanlan.zhihu.com/*
// @require      https://cdn.jsdelivr.net/npm/turndown/dist/turndown.js
// @grant        GM_setClipboard
// ==/UserScript==

(function () {
  'use strict';

  // --------- Turndown 初始化 ---------
  const turndownService = new TurndownService({
    headingStyle: 'atx',
    codeBlockStyle: 'fenced',
    bulletListMarker: '-',
    emDelimiter: '*'
  });

  // ---- 自定义 Turndown 规则 ----

  // 换行
  turndownService.addRule('lineBreak', {
    filter: 'br',
    replacement: function () {
      return '  \n';
    }
  });

  // 表格：<table> → GFM Markdown 表格
  turndownService.addRule('zhihuTable', {
    filter: 'table',
    replacement: function (content, node) {
      var rows = [];
      var trs = node.querySelectorAll('tr');
      if (!trs.length) return '';

      var maxCols = 0;
      trs.forEach(function (tr) {
        var cells = [];
        tr.querySelectorAll('th, td').forEach(function (cell) {
          var text = (cell.innerText || cell.textContent || '').trim()
            .replace(/\n/g, ' ')
            .replace(/\|/g, '\\|');
          var colspan = parseInt(cell.getAttribute('colspan') || '1', 10);
          cells.push(text);
          for (var i = 1; i < colspan; i++) cells.push('');
        });
        if (cells.length) {
          if (cells.length > maxCols) maxCols = cells.length;
          rows.push(cells);
        }
      });

      if (!maxCols) return '';

      rows.forEach(function (r) {
        while (r.length < maxCols) r.push('');
      });

      var lines = [];
      rows.forEach(function (row, idx) {
        lines.push('| ' + row.join(' | ') + ' |');
        if (idx === 0) {
          lines.push('| ' + row.map(function () { return '---'; }).join(' | ') + ' |');
        }
      });

      return '\n\n' + lines.join('\n') + '\n\n';
    }
  });

  // 删除线
  turndownService.addRule('strikethrough', {
    filter: ['del', 's', 'strike'],
    replacement: function (content) {
      return '~~' + content + '~~';
    }
  });

  // 数学公式：<img eeimg>
  turndownService.addRule('mathFormula', {
    filter: function (node) {
      return node.nodeName === 'IMG' && node.hasAttribute('eeimg');
    },
    replacement: function (content, node) {
      var formula = node.getAttribute('alt') || node.getAttribute('data-formula') || '';
      formula = formula.replace(/^\[公式\]$/, '').trim();
      if (!formula) return '';
      var isBlock = node.getAttribute('eeimg') === '2';
      return isBlock ? '\n\n$$' + formula + '$$\n\n' : '$' + formula + '$';
    }
  });

  // .ztext-math 元素
  turndownService.addRule('ztextMath', {
    filter: function (node) {
      return node.classList && node.classList.contains('ztext-math');
    },
    replacement: function (content, node) {
      var formula = node.getAttribute('data-formula') || node.textContent.trim();
      var isBlock = node.getAttribute('data-eeimg') === '2';
      if (!formula) return '';
      return isBlock ? '\n\n$$' + formula + '$$\n\n' : '$' + formula + '$';
    }
  });

  // 脚注引用
  turndownService.addRule('footnoteRef', {
    filter: function (node) {
      return node.nodeName === 'SUP' && node.getAttribute('data-draft-type') === 'reference';
    },
    replacement: function (content, node) {
      var text = node.textContent.trim();
      return '[^' + text + ']';
    }
  });

  // 下划线
  turndownService.addRule('underline', {
    filter: 'u',
    replacement: function (content) {
      return '<u>' + content + '</u>';
    }
  });

  // 高亮
  turndownService.addRule('highlightMark', {
    filter: 'mark',
    replacement: function (content) {
      return '==' + content + '==';
    }
  });

  // --------- 工具函数 ---------

  // 清理 Markdown 输出中多余的转义
  function cleanMarkdown(md) {
    if (!md) return md;

    // 保护代码块、行内代码、数学公式，避免误改其内容
    var protectedBlocks = [];
    var ph = function (match) {
      protectedBlocks.push(match);
      return '\x00P' + (protectedBlocks.length - 1) + '\x00';
    };

    // 代码块
    var cleaned = md.replace(/```[\s\S]*?```/g, ph);
    // 行内代码
    cleaned = cleaned.replace(/`[^`]+`/g, ph);
    // 块级数学
    cleaned = cleaned.replace(/\$\$[\s\S]*?\$\$/g, ph);
    // 行内数学
    cleaned = cleaned.replace(/\$[^$\n]+?\$/g, ph);

    // \_ → _（词内下划线不需要转义）
    cleaned = cleaned.replace(/\\_/g, '_');

    // \* → *，但保留 \*\*
    cleaned = cleaned.replace(/\\\*\\\*/g, '\x00DS\x00');
    cleaned = cleaned.replace(/\\\*/g, '*');
    cleaned = cleaned.replace(/\x00DS\x00/g, '**');

    // 还原保护区域
    cleaned = cleaned.replace(/\x00P(\d+)\x00/g, function (_, i) {
      return protectedBlocks[parseInt(i)];
    });

    return cleaned;
  }

  // 尽量还原为原图 URL
  function getBestImageUrl(img) {
    if (!img) return '';

    const attrs = {};
    for (const attr of img.attributes) {
      attrs[attr.name] = attr.value;
    }

    const token =
      attrs['data-original-token'] ||
      attrs['data-image-token'] ||
      attrs['data-token'] ||
      '';

    const candidates = [
      attrs['data-actualsrc'],
      attrs['data-actual-src'],
      attrs['data-original'],
      attrs['data-raw-src'],
      attrs['data-default-watermark-src'],
      img.currentSrc,
      img.src
    ].filter(Boolean);

    for (let url of candidates) {
      const cleaned = cleanupZhihuImageUrl(url, token);
      if (cleaned) return cleaned;
    }

    return '';
  }

  // 清理知乎图片 URL
  function cleanupZhihuImageUrl(url, token) {
    try {
      const u = new URL(url, location.href);

      if (!/zhimg\.com$/.test(u.hostname)) {
        return url;
      }

      if (token) {
        u.pathname = u.pathname.replace(
          /v\d?-[0-9a-zA-Z]+/,
          'v2-' + token.replace(/^v\d?-/, '')
        );
      }

      u.pathname = u.pathname.replace(
        /_[^\/.]+(?=\.[^.\/]+$)/,
        ''
      );

      u.search = '';

      return u.href;
    } catch (e) {
      return url;
    }
  }

  // 规范化内容
  function normalizeContent(root) {
    if (!root) return;

    root.querySelectorAll(
      'script, style, button, svg, .ztext-empty-paragraph, .ContentItem-expandButton'
    ).forEach(el => el.remove());

    root.querySelectorAll('figure').forEach(fig => {
      const img = fig.querySelector('img');
      if (img && fig.parentNode) {
        fig.parentNode.insertBefore(img, fig);
      }
      fig.remove();
    });

    root.querySelectorAll('img').forEach(img => {
      if (img.hasAttribute('eeimg')) return;

      const url = getBestImageUrl(img);
      if (!url) {
        img.remove();
        return;
      }
      img.setAttribute('src', url);
      img.removeAttribute('srcset');

      const caption =
        img.getAttribute('data-caption') ||
        img.getAttribute('data-original-caption') ||
        img.getAttribute('alt') ||
        '';
      if (caption) {
        img.setAttribute('alt', caption);
      }
    });
  }

  // 显示 Markdown 弹窗
  function showMarkdownModal(markdown) {
    const existing = document.getElementById('zhihu-md-modal');
    if (existing) existing.remove();

    const modal = document.createElement('div');
    modal.id = 'zhihu-md-modal';
    Object.assign(modal.style, {
      position: 'fixed',
      top: '0',
      left: '0',
      right: '0',
      bottom: '0',
      backgroundColor: 'rgba(0, 0, 0, 0.5)',
      zIndex: '2147483647',
      display: 'flex',
      justifyContent: 'center',
      alignItems: 'center'
    });

    const content = document.createElement('div');
    Object.assign(content.style, {
      backgroundColor: 'white',
      borderRadius: '8px',
      width: '80%',
      maxWidth: '800px',
      height: '80%',
      maxHeight: '600px',
      display: 'flex',
      flexDirection: 'column',
      boxShadow: '0 4px 20px rgba(0,0,0,0.3)'
    });

    const header = document.createElement('div');
    Object.assign(header.style, {
      padding: '15px 20px',
      borderBottom: '1px solid #e0e0e0',
      display: 'flex',
      justifyContent: 'space-between',
      alignItems: 'center'
    });

    const title = document.createElement('div');
    title.textContent = 'Markdown 导出结果';
    Object.assign(title.style, {
      fontSize: '16px',
      fontWeight: 'bold',
      color: '#333'
    });

    const btnGroup = document.createElement('div');
    Object.assign(btnGroup.style, {
      display: 'flex',
      gap: '10px'
    });

    const copyBtn = document.createElement('button');
    copyBtn.textContent = '复制全部';
    Object.assign(copyBtn.style, {
      padding: '6px 12px',
      backgroundColor: '#0084ff',
      color: 'white',
      border: 'none',
      borderRadius: '4px',
      cursor: 'pointer',
      fontSize: '14px'
    });

    copyBtn.addEventListener('click', async () => {
      try {
        if (typeof GM_setClipboard === 'function') {
          GM_setClipboard(markdown, { type: 'text', mimetype: 'text/plain' });
        } else if (navigator.clipboard && navigator.clipboard.writeText) {
          await navigator.clipboard.writeText(markdown);
        } else {
          textarea.select();
          document.execCommand('copy');
        }

        const originalText = copyBtn.textContent;
        copyBtn.textContent = '✓ 已复制';
        copyBtn.style.backgroundColor = '#52c41a';
        setTimeout(() => {
          copyBtn.textContent = originalText;
          copyBtn.style.backgroundColor = '#0084ff';
        }, 2000);
      } catch (e) {
        console.error('复制失败：', e);
        alert('复制失败，请手动选择文本复制');
      }
    });

    const closeBtn = document.createElement('button');
    closeBtn.textContent = '关闭';
    Object.assign(closeBtn.style, {
      padding: '6px 12px',
      backgroundColor: '#f0f0f0',
      color: '#333',
      border: '1px solid #d0d0d0',
      borderRadius: '4px',
      cursor: 'pointer',
      fontSize: '14px'
    });

    closeBtn.addEventListener('click', () => {
      modal.remove();
    });

    btnGroup.appendChild(copyBtn);
    btnGroup.appendChild(closeBtn);
    header.appendChild(title);
    header.appendChild(btnGroup);

    const textarea = document.createElement('textarea');
    textarea.value = markdown;
    Object.assign(textarea.style, {
      flex: '1',
      margin: '20px',
      padding: '15px',
      border: '1px solid #d0d0d0',
      borderRadius: '4px',
      fontSize: '14px',
      fontFamily: 'Consolas, Monaco, "Courier New", monospace',
      lineHeight: '1.5',
      resize: 'none',
      outline: 'none'
    });

    content.appendChild(header);
    content.appendChild(textarea);
    modal.appendChild(content);

    modal.addEventListener('click', (e) => {
      if (e.target === modal) {
        modal.remove();
      }
    });

    const handleEsc = (e) => {
      if (e.key === 'Escape') {
        modal.remove();
        document.removeEventListener('keydown', handleEsc);
      }
    };
    document.addEventListener('keydown', handleEsc);

    document.body.appendChild(modal);
    textarea.focus();
    textarea.select();
  }

  // --------- 专栏文章导出 ---------

  function exportColumnArticle() {
    const titleEl =
      document.querySelector('h1.Post-Title') ||
      document.querySelector('h1.ArticleHeader-title') ||
      document.querySelector('h1');

    const title = titleEl
      ? titleEl.innerText.trim()
      : document.title.replace(/ - 专栏 - 知乎$| - 知乎$/, '');

    const contentEl =
      document.querySelector('.Post-RichTextContainer .RichText.ztext') ||
      document.querySelector('article .RichText.ztext') ||
      document.querySelector('.RichText.ztext');

    if (!contentEl) {
      alert('找不到专栏正文节点，可能是知乎页面结构更新了。');
      return '';
    }

    const clone = contentEl.cloneNode(true);
    normalizeContent(clone);

    const bodyMd = turndownService.turndown(clone.innerHTML).trim();

    const mdParts = [];
    if (title) mdParts.push('# ' + title, '');
    mdParts.push(bodyMd);

    return mdParts.join('\n');
  }

  // --------- 从 AnswerItem 提取回答信息 ---------

  function extractAnswerInfo(item, idx) {
    const contentEl = item.querySelector('.RichText.ztext');
    if (!contentEl) return null;

    const authorEl =
      item.querySelector('.AuthorInfo-head .UserLink-link') ||
      item.querySelector('.AuthorInfo-content .UserLink-link') ||
      item.querySelector('.UserLink-link');

    const authorName = authorEl
      ? authorEl.innerText.trim()
      : '回答者 ' + (idx + 1);

    const voteEl =
      item.querySelector('.VoteButton--up .VoteButton--count') ||
      item.querySelector('.VoteButton--count');
    const voteText = voteEl ? voteEl.innerText.trim() : '';

    const clone = contentEl.cloneNode(true);
    normalizeContent(clone);
    const ansMd = turndownService.turndown(clone.innerHTML).trim();

    const header =
      '## 回答 ' + (idx + 1) + ' - ' + authorName +
      (voteText ? '（赞同 ' + voteText + '）' : '');

    return header + '\n\n' + ansMd;
  }

  // --------- 问题 + 全部回答导出 ---------

  function exportQuestionWithAnswers() {
    const titleEl =
      document.querySelector('h1.QuestionHeader-title') ||
      document.querySelector('.QuestionHeader-title') ||
      document.querySelector('h1');

    const title = titleEl
      ? titleEl.innerText.trim()
      : document.title.replace(/ - 知乎$/, '');

    const descEl =
      document.querySelector('.QuestionHeader-detail .RichText.ztext') ||
      document.querySelector('.QuestionHeader-detail .RichContent-inner .RichText.ztext') ||
      document.querySelector('.QuestionHeader-detail .RichContent-inner .RichText');

    let descMd = '';
    if (descEl) {
      const descClone = descEl.cloneNode(true);
      normalizeContent(descClone);
      descMd = turndownService.turndown(descClone.innerHTML).trim();
    }

    const answerItems = Array.from(
      document.querySelectorAll('.Question-main .AnswerItem')
    ).filter(item => item.querySelector('.RichText.ztext'));

    if (!answerItems.length) {
      alert('当前页面没有找到回答内容（可能尚未下拉加载）。');
    }

    const answerMdParts = [];
    answerItems.forEach((item, idx) => {
      const part = extractAnswerInfo(item, idx);
      if (part) answerMdParts.push(part);
    });

    const md = [];

    if (title) md.push('# ' + title, '');

    if (descMd) {
      md.push('> ' + descMd.replace(/\n/g, '\n> '), '', '---', '');
    }

    if (answerMdParts.length) {
      md.push(answerMdParts.join('\n\n---\n\n'));
    }

    return md.join('\n');
  }

  // --------- 单个回答导出 ---------

  function exportSingleAnswer() {
    const answerIdMatch = location.pathname.match(/\/answer\/(\d+)/);
    const answerId = answerIdMatch ? answerIdMatch[1] : null;

    let item = null;

    if (answerId) {
      // 方式1：name 属性
      item = document.querySelector('.AnswerItem[name="' + answerId + '"]');

      // 方式2：data-zop 中的 itemId
      if (!item) {
        item = document.querySelector(
          '.AnswerItem[data-zop*="\\"itemId\\":\\"' + answerId + '\\""]'
        );
      }

      // 方式3：meta[itemprop="url"]
      if (!item) {
        const metas = document.querySelectorAll(
          '.AnswerItem meta[itemprop="url"]'
        );
        for (const meta of metas) {
          const content = meta.getAttribute('content') || '';
          if (content.includes('/answer/' + answerId)) {
            item = meta.closest('.AnswerItem');
            break;
          }
        }
      }
    }

    // fallback
    if (!item) {
      item = document.querySelector('.Question-main .AnswerItem');
    }

    if (!item) {
      alert('找不到当前回答节点。');
      return '';
    }

    const contentEl = item.querySelector('.RichText.ztext');
    if (!contentEl) {
      alert('找不到回答正文节点。');
      return '';
    }

    const titleEl =
      document.querySelector('h1.QuestionHeader-title') ||
      document.querySelector('.QuestionHeader-title') ||
      document.querySelector('h1');

    const questionTitle = titleEl
      ? titleEl.innerText.trim()
      : document.title.replace(/ - 知乎$/, '');

    const authorEl =
      item.querySelector('.AuthorInfo-head .UserLink-link') ||
      item.querySelector('.AuthorInfo-content .UserLink-link') ||
      item.querySelector('.UserLink-link');

    const authorName = authorEl
      ? authorEl.innerText.trim()
      : '回答者';

    const voteEl =
      item.querySelector('.VoteButton--up .VoteButton--count') ||
      item.querySelector('.VoteButton--count');
    const voteText = voteEl ? voteEl.innerText.trim() : '';

    const clone = contentEl.cloneNode(true);
    normalizeContent(clone);
    const ansMd = turndownService.turndown(clone.innerHTML).trim();

    const md = [];

    if (questionTitle) md.push('# ' + questionTitle, '');

    md.push(
      '> 回答者：' + authorName +
        (voteText ? '（赞同 ' + voteText + '）' : ''),
      ''
    );

    md.push(ansMd);

    return md.join('\n');
  }

  // --------- 页面类型判断 + 导出入口 ---------

  function doExport() {
    const href = location.href;

    let markdown = '';

    if (/^https:\/\/zhuanlan\.zhihu\.com\/p\//.test(href)) {
      markdown = exportColumnArticle();
    } else if (/^https:\/\/www\.zhihu\.com\/question\/\d+\/answer\/\d+/.test(href)) {
      markdown = exportSingleAnswer();
    } else if (/^https:\/\/www\.zhihu\.com\/question\/\d+/.test(href)) {
      markdown = exportQuestionWithAnswers();
    } else {
      alert('当前页面暂不支持导出 Markdown。');
      return;
    }

    if (!markdown || !markdown.trim()) {
      alert('未能生成 Markdown，可能是页面结构变动。');
      return;
    }

    markdown = cleanMarkdown(markdown);

    showMarkdownModal(markdown);
  }

  // --------- 按钮 UI ---------

  function addExportButton() {
    if (document.getElementById('zhihu-md-export-btn')) return;

    const btn = document.createElement('button');
    btn.id = 'zhihu-md-export-btn';
    btn.textContent = '导出 Markdown';

    btn.style.cssText = [
      'position: fixed !important',
      'right: 20px !important',
      'bottom: 80px !important',
      'z-index: 2147483647 !important',
      'padding: 8px 12px !important',
      'background-color: #0084ff !important',
      'color: #fff !important',
      'border: none !important',
      'border-radius: 4px !important',
      'font-size: 14px !important',
      'cursor: pointer !important',
      'box-shadow: 0 2px 8px rgba(0,0,0,.2) !important',
      'display: block !important',
      'visibility: visible !important',
      'opacity: 1 !important',
      'pointer-events: auto !important'
    ].join('; ');

    btn.addEventListener('mouseenter', () => {
      btn.style.backgroundColor = '#0070e0';
    });
    btn.addEventListener('mouseleave', () => {
      btn.style.backgroundColor = '#0084ff';
    });

    btn.addEventListener('click', () => {
      try {
        doExport();
      } catch (e) {
        console.error('[知乎MD导出] 错误：', e);
        alert('导出过程中出错：' + e.message);
      }
    });

    document.body.appendChild(btn);
    console.log('[知乎MD导出] 按钮已添加');
  }

  function ready(fn) {
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', fn);
    } else {
      fn();
    }
  }

  ready(() => {
    addExportButton();
  });

})();