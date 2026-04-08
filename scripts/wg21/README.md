# wg21.link 下载器 + 简化工具

你需要首先 `curl https://wg21.link/index.json -O` 获取索引再去使用。

- `download.py`：指定 `index.json`，下载提案
- `simplify.py`：指定已下载的提案目录，简化样式

具体使用方式可以查阅脚本内的 `--help`。

`download.py` 支持断点续传，因此以后更新只需要重新拉取新的 `index.json` 再调用脚本即可；理论上不需要单独使用 `html2html`/ `html2text` / `pdf2text`。

其他杂项脚本：
- `cleanup.py`：启发式清理掉旧修订号的提案，这对于 N 开头的很有用
- `cleanup_manually.py`：手动指定文件名关键字，只保留字典序最大的文件，相当于前面的手动版

另外推荐 `tar.zst` 归档后再按需搜索文档。具体可以在我的仓库里搜索 `zxgrep.py` 脚本，可以省下很多空间而且不影响日常使用。

不精确的简单统计：未简化样式 1.6 GB，简化样式 400 MB，归档 60 MB。
