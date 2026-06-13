chrome.storage.local.get(['targetUrl'], (result) => {
  const targetUrl = result.targetUrl;
  
  if (!targetUrl) {
    document.getElementById('msg').innerHTML = '还未设置链接。<br><br><button id="goOptions" style="padding:10px 20px; cursor:pointer;">去设置</button>';
    document.getElementById('goOptions').addEventListener('click', () => {
      chrome.runtime.openOptionsPage();
    });
    return;
  }

  const handleRedirectResult = () => {
    if (chrome.runtime.lastError) {
      const errMsg = chrome.runtime.lastError.message;
      document.getElementById('msg').innerHTML = `
        <div style="color: red; font-size: 20px; font-weight: bold; margin-bottom: 20px;">跳转失败！</div>
        <div style="text-align: left; display: inline-block; background: #f5f5f5; padding: 20px; border-radius: 8px;">
          <p style="margin-top:0; color:#d32f2f;">拦截原因: ${errMsg}</p>
          <p>如果您尝试访问 <b>file:///</b> 开头的本地文件，请务必授权：</p>
          1. 复制并打开扩展管理页：<b>chrome://extensions/</b><br>
          2. 找到本扩展，点击 <b>"详细信息"</b><br>
          3. 往下滚，找到并打开 <b>"允许访问文件网址"</b> 开关<br>
          4. 重新点击 ➕ 号即可生效。
        </div>
      `;
    }
  };

  try {
    chrome.tabs.getCurrent((tab) => {
      if (tab && tab.id) {
        chrome.tabs.update(tab.id, { url: targetUrl }, handleRedirectResult);
      } else {
        chrome.tabs.update({ url: targetUrl }, handleRedirectResult);
      }
    });
  } catch (err) {
    document.getElementById('msg').innerHTML = `<div style="color: red;">执行出错: ${err.message}</div>`;
  }
});
