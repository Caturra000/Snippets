document.addEventListener('DOMContentLoaded', () => {
  const urlInput = document.getElementById('urlInput');
  const saveBtn = document.getElementById('saveBtn');
  const status = document.getElementById('status');
  chrome.storage.local.get(['targetUrl'], (result) => {
    if (result.targetUrl) {
      urlInput.value = result.targetUrl;
    }
  });
  saveBtn.addEventListener('click', () => {
    const url = urlInput.value.trim();
    chrome.storage.local.set({ targetUrl: url }, () => {
      status.textContent = '保存成功！';
      setTimeout(() => { status.textContent = ''; }, 2000);
    });
  });
});
