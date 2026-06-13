## 背景

个人使用一个第三方的 Chromium，标签页导航功能非常弱。因此增加一个仿 Chromium 样式但是功能加强的单 HTML 导航页面。

但是又一个问题是，new tab 的情况并不能自定义，尽管应用商店已经有 NTR 扩展（new tab redirect），但是它没有支持本地文件。

因此一个拐弯魔教的做法是做一个改良的 NTR 再套上自制导航页，随便也解决了没法用鼠标手势的问题。

## 使用

地址栏打开 `chrome://extensions/`，启用开发者模式，选择加载未打包的扩展程序，选择附带 `CustomNewTab_Extension` 目录。

扩展选项中选择 `newtab.html`，比如 Windows 下 `file:///C:/Users/Caturra/newtab.html`。