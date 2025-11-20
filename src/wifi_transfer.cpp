#include "wifi_transfer.h"
#include "ui.h"
#include <SPI.h>
#include <FS.h>

// Forward declaration for safeBeep() from main.cpp
extern void safeBeep(int freq, int duration, bool checkSound = true);

WiFiTransferState transferState = TRANSFER_MENU;
WebServer* webServer = nullptr;
bool serverRunning = false;
int uploadCount = 0;
int downloadCount = 0;
String lastAction = "Ready";

// HTML for the web interface - stored in PROGMEM (flash) to save RAM
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Transfer</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Courier New', monospace;
      padding: 10px;
      background: #f5f5f5;
      display: flex;
      flex-direction: column;
      align-items: center;
      min-height: 100vh;
    }
    .header {
      background: white;
      border: 3px solid black;
      border-radius: 30px;
      padding: 8px 20px;
      margin: 10px 0 15px 0;
      display: flex;
      align-items: center;
      gap: 8px;
      font-size: 20px;
      font-weight: bold;
      letter-spacing: 2px;
    }
    .version {
      position: fixed;
      bottom: 5px;
      right: 10px;
      font-size: 10px;
      color: #999;
      font-weight: normal;
    }
    .star {
      width: 24px;
      height: 24px;
      background: #FFD700;
      clip-path: polygon(50% 0%, 61% 35%, 98% 35%, 68% 57%, 79% 91%, 50% 70%, 21% 91%, 32% 57%, 2% 35%, 39% 35%);
    }
    .upload-section {
      background: white;
      border: 3px solid black;
      border-radius: 25px;
      padding: 20px;
      width: 100%;
      max-width: 600px;
      text-align: center;
      margin-bottom: 15px;
    }
    .upload-section.dragover {
      background: #ffffaa;
    }
    .upload-section h2 {
      font-size: 28px;
      margin-bottom: 8px;
      letter-spacing: 1px;
    }
    .upload-section p {
      font-size: 14px;
      margin: 6px 0;
    }
    input[type=file] {
      display: none;
    }
    .btn {
      background: white;
      border: 2px solid black;
      border-radius: 20px;
      padding: 8px 16px;
      font-size: 14px;
      font-weight: bold;
      font-family: 'Courier New', monospace;
      cursor: pointer;
      margin: 4px;
      display: inline-block;
    }
    .btn:hover {
      background: #f0f0f0;
    }
    .btn:active {
      background: #e0e0e0;
    }
    .files-section {
      background: #FFD700;
      border: 3px solid black;
      border-radius: 20px;
      padding: 15px;
      width: 100%;
      max-width: 600px;
      min-height: 300px;
    }
    .files-section h2 {
      font-size: 20px;
      margin-bottom: 12px;
      letter-spacing: 1px;
      color: black;
    }
    .folder-controls {
      display: flex;
      gap: 6px;
      margin-bottom: 12px;
      flex-wrap: wrap;
    }
    .file-item {
      padding: 10px 10px 10px 15px;
      margin: 6px 0;
      border-bottom: 1px solid rgba(0,0,0,0.2);
      display: flex;
      justify-content: space-between;
      align-items: center;
      background: rgba(255,255,255,0.3);
      border-radius: 8px;
    }
    .file-item.folder {
      background: rgba(0,0,0,0.1);
      font-weight: bold;
    }
    .file-item.indent-1 { margin-left: 15px; }
    .file-item.indent-2 { margin-left: 30px; }
    .file-item.indent-3 { margin-left: 45px; }
    .file-item.indent-4 { margin-left: 60px; }
    .file-info {
      flex: 1;
      overflow: visible;
    }
    .file-name {
      font-weight: bold;
      font-size: 14px;
      color: black;
      overflow: visible;
    }
    .file-size {
      color: #333;
      font-size: 12px;
      margin-left: 8px;
    }
    .folder-header {
      font-weight: bold;
      color: black;
      margin: 12px 0 6px 0;
      padding: 8px 8px 8px 20px;
      background: rgba(0,0,0,0.1);
      border-radius: 0px;
      font-size: 14px;
      cursor: pointer;
      display: flex;
      align-items: center;
      gap: 12px;
      user-select: none;
      overflow: visible;
    }
    .folder-header:hover {
      background: rgba(0,0,0,0.15);
    }
    .folder-contents {
      display: block;
    }
    .folder-contents.collapsed {
      display: none;
    }
    .folder-arrow {
      font-size: 12px;
      transition: transform 0.2s;
      flex-shrink: 0;
      display: inline-block;
      width: 30px;
      margin-right: 8px;
      text-align: center;
      transform: rotate(90deg);
    }
    .folder-arrow.collapsed {
      transform: rotate(0deg);
    }
    .file-buttons {
      display: flex;
      gap: 4px;
    }
    .btn-small {
      background: white;
      border: 2px solid black;
      border-radius: 15px;
      padding: 6px 12px;
      font-size: 12px;
      font-weight: bold;
      font-family: 'Courier New', monospace;
      cursor: pointer;
    }
    .btn-download:hover { background: #e3f2fd; }
    .btn-delete:hover { background: #ffebee; }
    .btn-move:hover { background: #fff9c4; }
    .status {
      position: fixed;
      bottom: 10px;
      right: 10px;
      background: white;
      border: 2px solid black;
      border-radius: 15px;
      padding: 10px 15px;
      font-weight: bold;
      max-width: 250px;
      font-size: 12px;
    }
    .progress {
      width: 100%;
      height: 20px;
      background: #e0e0e0;
      border: 2px solid black;
      border-radius: 15px;
      overflow: hidden;
      margin: 10px 0;
      display: none;
    }
    .progress-bar {
      height: 100%;
      background: #FFD700;
      width: 0%;
      transition: width 0.3s;
    }
  </style>
</head>
<body>
  <div class="header">
    <div class="star"></div>
    <span>TRANSFER</span>
  </div>

  <div class="upload-section" id="dropZone">
    <h2>Upload!</h2>
    <p>Drag & Drop them thangs!</p>
    <p>or</p>
    <input type="file" id="fileInput" multiple webkitdirectory>
    <button class="btn" onclick="document.getElementById('fileInput').click()">Choose Folder</button>
    <button class="btn" onclick="selectFiles()">Choose Files</button>
    <button class="btn" onclick="uploadFiles()">Upload</button>
    <div class="progress" id="progressContainer">
      <div class="progress-bar" id="progressBar"></div>
    </div>
  </div>

  <div class="files-section">
    <h2 id="filesTitle">SD Files</h2>
    <div class="folder-controls">
      <button class="btn" onclick="showCreateFolder()">+ New Folder</button>
      <input type="text" id="newFolderInput" placeholder="Folder name" style="display:none; padding:10px; border:2px solid black; border-radius:20px; font-family:'Courier New';">
      <button class="btn" id="createFolderBtn" onclick="createFolder()" style="display:none;">Create</button>
      <button class="btn" id="cancelFolderBtn" onclick="cancelCreateFolder()" style="display:none;">Cancel</button>
    </div>
    <div id="fileListContainer">Loading...</div>
  </div>

  <div class="status" id="status" style="display:none;">Ready</div>

  <script>
    const dropZone = document.getElementById('dropZone');
    const fileInput = document.getElementById('fileInput');
    const status = document.getElementById('status');
    let allFolders = [];

    function showStatus(msg) {
      status.textContent = msg;
      status.style.display = 'block';
      setTimeout(() => status.style.display = 'none', 3000);
    }

    function toggleFolder(folderName) {
      const contents = document.getElementById('folder-' + folderName);
      const arrow = document.getElementById('arrow-' + folderName);
      if (contents && arrow) {
        contents.classList.toggle('collapsed');
        arrow.classList.toggle('collapsed');
      }
    }

    dropZone.addEventListener('dragover', (e) => {
      e.preventDefault();
      dropZone.classList.add('dragover');
    });

    dropZone.addEventListener('dragleave', () => {
      dropZone.classList.remove('dragover');
    });

    dropZone.addEventListener('drop', (e) => {
      e.preventDefault();
      dropZone.classList.remove('dragover');
      fileInput.files = e.dataTransfer.files;
      uploadFiles();
    });

    function selectFiles() {
      const fileInput = document.getElementById('fileInput');
      fileInput.removeAttribute('webkitdirectory');
      fileInput.click();
    }

    // Optimize image for device (resize to 240x135, convert to JPG)
    async function optimizeImage(file) {
      return new Promise((resolve) => {
        const img = new Image();
        const canvas = document.createElement('canvas');
        const ctx = canvas.getContext('2d');

        img.onload = () => {
          // Resize to device screen size
          canvas.width = 240;
          canvas.height = 135;

          // Draw resized image
          ctx.drawImage(img, 0, 0, 240, 135);

          // Convert to blob (JPG format, 85% quality)
          canvas.toBlob((blob) => {
            // Create new file with _optimized suffix
            const newName = file.name.replace(/\\.(png|jpg|jpeg|gif|bmp)$/i, '_optimized.jpg');
            const newFile = new File([blob], newName, { type: 'image/jpeg' });
            resolve(newFile);
          }, 'image/jpeg', 0.85);
        };

        img.onerror = () => resolve(file); // If error, use original
        img.src = URL.createObjectURL(file);
      });
    }

    async function uploadFiles() {
      const files = fileInput.files;
      if (files.length === 0) {
        showStatus('No files selected');
        return;
      }

      const progressContainer = document.getElementById('progressContainer');
      const progressBar = document.getElementById('progressBar');
      progressContainer.style.display = 'block';

      // Check if any images need optimization (only for files > 1MB)
      let hasLargeImages = false;
      for (let file of files) {
        if (file.type.startsWith('image/') && file.size > 1000000) { // > 1MB
          hasLargeImages = true;
          break;
        }
      }

      let shouldOptimize = false;
      if (hasLargeImages) {
        shouldOptimize = confirm('Some images are very large (>1MB) and may not display.\\n\\nOptimize them automatically? (Click Cancel to upload as-is)');
      }

      for (let i = 0; i < files.length; i++) {
        let file = files[i];
        const originalPath = file.webkitRelativePath || file.name;
        let uploadPath = originalPath;

        // Only optimize if user agreed AND file is large image
        if (shouldOptimize && file.type.startsWith('image/') && file.size > 1000000) {
          try {
            showStatus(`Optimizing ${file.name}...`);
            const optimizedFile = await optimizeImage(file);
            if (optimizedFile && optimizedFile.size < file.size) {
              file = optimizedFile;
              uploadPath = file.name;
              showStatus(`Uploading optimized ${file.name}...`);
            } else {
              showStatus(`Uploading ${originalPath}...`);
            }
          } catch (err) {
            console.error('Optimization failed:', err);
            showStatus(`Uploading ${originalPath}... (optimization failed)`);
          }
        } else {
          showStatus(`Uploading ${originalPath}...`);
        }

        progressBar.style.width = ((i / files.length) * 100) + '%';

        const formData = new FormData();
        formData.append('file', file);
        formData.append('path', uploadPath);

        try {
          const response = await fetch('/upload', {
            method: 'POST',
            body: formData
          });

          if (response.ok) {
            showStatus(`Uploaded ${uploadPath}`);
          } else {
            showStatus(`Failed: ${uploadPath}`);
          }
        } catch (error) {
          showStatus(`Error: ${uploadPath}`);
        }
      }

      progressBar.style.width = '100%';
      setTimeout(() => {
        progressContainer.style.display = 'none';
        progressBar.style.width = '0%';
        fileInput.value = '';
        // Re-add webkitdirectory for next folder upload
        fileInput.setAttribute('webkitdirectory', '');
        refreshFiles();
      }, 1000);
    }

    async function refreshFiles() {
      document.getElementById('fileListContainer').textContent = 'Loading...';
      try {
        // Fetch SD card info
        const infoResponse = await fetch('/info');
        if (!infoResponse.ok) throw new Error('Info fetch failed: ' + infoResponse.status);
        const info = await infoResponse.json();
        const freeMB = info.totalMB - info.usedMB;
        document.getElementById('filesTitle').textContent = `SD Files - ${info.usedMB}MB / ${info.totalMB}MB`;

        // Fetch file list
        const response = await fetch('/list');
        if (!response.ok) throw new Error('List fetch failed: ' + response.status);
        const files = await response.json();
        displayFiles(files);
      } catch (error) {
        document.getElementById('fileListContainer').innerHTML = '<p style="padding:20px;color:red;">Error: ' + error.message + '</p>';
        console.error('refreshFiles error:', error);
      }
    }

    function displayFiles(files) {
      const container = document.getElementById('fileListContainer');
      if (files.length === 0) {
        container.innerHTML = '<p style="padding:20px; color:black;">No files yet!</p>';
        return;
      }

      let html = '';
      const loadedFolders = new Set();

      // Render files - folders show as clickable headers, files show normally
      files.forEach(file => {
        if (file.isDir) {
          const folderPath = file.name;
          const folderName = folderPath.startsWith('/') ? folderPath.substring(1) : folderPath;
          const folderId = 'folder_' + folderName.replace(/[^a-zA-Z0-9]/g, '_');

          html += `
            <div class="folder-header" data-folder="${folderPath}" data-loaded="false" id="header-${folderId}">
              <span class="folder-arrow collapsed" id="arrow-${folderId}">></span>
              <span style="overflow:visible;display:inline-block;">${folderName}/</span>
              <span style="margin-left:auto;font-size:10px;color:#666;" id="count-${folderId}">click to load</span>
            </div>
            <div class="folder-contents collapsed" id="contents-${folderId}">
            </div>
          `;
        } else {
          const filePath = file.name.startsWith('/') ? file.name.substring(1) : file.name;
          html += renderFileItem({...file, path: filePath, depth: 0});
        }
      });

      container.innerHTML = html;

      // Add event delegation for folder clicks and file buttons
      container.addEventListener('click', async function(e) {
        // Handle folder header clicks
        const folderHeader = e.target.closest('.folder-header');
        if (folderHeader) {
          const folderPath = folderHeader.getAttribute('data-folder');
          const isLoaded = folderHeader.getAttribute('data-loaded') === 'true';
          const folderId = folderHeader.id.replace('header-', '');
          const contents = document.getElementById('contents-' + folderId);
          const arrow = document.getElementById('arrow-' + folderId);
          const count = document.getElementById('count-' + folderId);

          if (!isLoaded) {
            // Load folder contents
            count.textContent = 'loading...';
            try {
              const response = await fetch('/directory?path=' + encodeURIComponent(folderPath));
              const folderFiles = await response.json();

              let folderHtml = '';
              folderFiles.forEach(file => {
                folderHtml += renderFileItem({...file, path: file.name.substring(1), depth: 1});
              });
              contents.innerHTML = folderHtml;

              count.textContent = folderFiles.length + ' items';
              folderHeader.setAttribute('data-loaded', 'true');
            } catch (error) {
              count.textContent = 'error';
              contents.innerHTML = '<p style="padding:10px;color:red;">Error loading folder</p>';
            }
          }

          // Toggle visibility
          contents.classList.toggle('collapsed');
          arrow.classList.toggle('collapsed');
          return;
        }

        // Handle file button clicks
        if (e.target.tagName === 'BUTTON') {
          const action = e.target.getAttribute('data-action');
          const filename = e.target.getAttribute('data-file');

          if (action === 'move') {
            showMoveDialog(filename);
          } else if (action === 'download') {
            downloadFile(filename);
          } else if (action === 'delete') {
            deleteFile(filename);
          }
        }
      });
    }

    function renderFileItem(file, currentFolder) {
      const indentClass = file.depth > 0 ? `indent-${Math.min(file.depth, 4)}` : '';
      const fileName = file.path.split('/').pop();
      const safeId = 'file_' + file.name.replace(/[^a-zA-Z0-9]/g, '_');

      if (file.isDir) {
        return `
          <div class="file-item folder ${indentClass}" data-path="${file.name}" id="${safeId}">
            <div class="file-info">
              <span class="file-name" style="margin-left:10px;">• ${fileName}/</span>
              <span class="file-size">folder</span>
            </div>
            <div class="file-buttons">
              <button class="btn-small btn-delete" data-action="delete" data-file="${file.name}">Delete</button>
            </div>
          </div>
        `;
      } else {
        return `
          <div class="file-item ${indentClass}" data-path="${file.name}" id="${safeId}">
            <div class="file-info">
              <span class="file-name" style="margin-left:10px;">• ${fileName}</span>
              <span class="file-size">${formatSize(file.size)}</span>
            </div>
            <div class="file-buttons">
              <button class="btn-small btn-move" data-action="move" data-file="${file.name}">Move</button>
              <button class="btn-small btn-download" data-action="download" data-file="${file.name}">Download</button>
              <button class="btn-small btn-delete" data-action="delete" data-file="${file.name}">Delete</button>
            </div>
          </div>
        `;
      }
    }

    function formatSize(bytes) {
      if (bytes < 1024) return bytes + ' B';
      if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
      return (bytes / 1048576).toFixed(1) + ' MB';
    }

    function downloadFile(filename) {
      window.location.href = '/download?file=' + encodeURIComponent(filename);
      showStatus(`Downloading ${filename}...`);
    }

    async function deleteFile(filename) {
      if (!confirm(`Delete ${filename}?`)) return;

      try {
        const response = await fetch('/delete?file=' + encodeURIComponent(filename), {
          method: 'DELETE'
        });

        if (response.ok) {
          showStatus(`Deleted ${filename}`);
          refreshFiles();
        } else {
          showStatus(`Failed to delete ${filename}`);
        }
      } catch (error) {
        showStatus(`Error deleting ${filename}`);
      }
    }

    function showCreateFolder() {
      document.getElementById('newFolderInput').style.display = 'inline-block';
      document.getElementById('createFolderBtn').style.display = 'inline-block';
      document.getElementById('cancelFolderBtn').style.display = 'inline-block';
      document.getElementById('newFolderInput').focus();
    }

    function cancelCreateFolder() {
      document.getElementById('newFolderInput').style.display = 'none';
      document.getElementById('createFolderBtn').style.display = 'none';
      document.getElementById('cancelFolderBtn').style.display = 'none';
      document.getElementById('newFolderInput').value = '';
    }

    async function createFolder() {
      const folderName = document.getElementById('newFolderInput').value.trim();
      if (!folderName) {
        showStatus('Please enter a folder name');
        return;
      }

      try {
        const response = await fetch('/mkdir?name=' + encodeURIComponent(folderName), {
          method: 'POST'
        });

        if (response.ok) {
          showStatus(`Created folder: ${folderName}`);
          cancelCreateFolder();
          refreshFiles();
        } else {
          showStatus(`Failed to create folder`);
        }
      } catch (error) {
        showStatus(`Error creating folder`);
      }
    }

    function showMoveDialog(filename) {
      const fileName = filename.startsWith('/') ? filename.substring(1) : filename;

      // Collect all unique folder paths from files
      const folderSet = new Set();
      folderSet.add('/'); // Always include root

      // Get all files again to extract folder paths
      fetch('/list').then(r => r.json()).then(files => {
        files.forEach(file => {
          if (file.isDir) {
            // Add actual directories
            folderSet.add(file.name);
          } else {
            // Extract folder path from file path
            const path = file.name.startsWith('/') ? file.name : '/' + file.name;
            const lastSlash = path.lastIndexOf('/');
            if (lastSlash > 0) {
              folderSet.add(path.substring(0, lastSlash));
            }
          }
        });

        console.log('Available folders:', Array.from(folderSet));

        let options = '<option value="/">/ (root)</option>';
        Array.from(folderSet).sort().forEach(folder => {
          if (folder !== '/') {
            const displayName = folder.startsWith('/') ? folder : '/' + folder;
            options += `<option value="${displayName}">${displayName}/</option>`;
          }
        });

        showMoveDialogWithOptions(filename, fileName, options);
      });
    }

    function showMoveDialogWithOptions(filename, fileName, options) {
      const dialog = `
        <div style="position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.5);display:flex;align-items:center;justify-content:center;z-index:1000;" id="moveDialog">
          <div style="background:white;border:3px solid black;border-radius:20px;padding:20px;max-width:400px;width:90%;" id="moveDialogInner">
            <h3 style="margin-bottom:15px;font-size:16px;">Move ${fileName}</h3>
            <select id="moveDestination" style="width:100%;padding:8px;border:2px solid black;border-radius:15px;font-family:'Courier New';margin-bottom:15px;font-size:14px;">
              ${options}
            </select>
            <div style="display:flex;gap:8px;justify-content:center;">
              <button class="btn" id="moveBtn" data-filename="${filename}">Move</button>
              <button class="btn" id="cancelMoveBtn">Cancel</button>
            </div>
          </div>
        </div>
      `;

      document.body.insertAdjacentHTML('beforeend', dialog);

      // Add event listeners for dialog
      document.getElementById('moveBtn').addEventListener('click', function() {
        executeMove(this.getAttribute('data-filename'));
      });
      document.getElementById('cancelMoveBtn').addEventListener('click', closeMoveDialog);
      document.getElementById('moveDialog').addEventListener('click', function(e) {
        if (e.target.id === 'moveDialog') closeMoveDialog();
      });
    }

    function closeMoveDialog() {
      const dialog = document.getElementById('moveDialog');
      if (dialog) dialog.remove();
    }

    async function executeMove(filename) {
      const destination = document.getElementById('moveDestination').value;

      try {
        const response = await fetch('/move', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: `from=${encodeURIComponent(filename)}&to=${encodeURIComponent(destination)}`
        });

        if (response.ok) {
          showStatus(`Moved ${filename}`);
          closeMoveDialog();
          refreshFiles();
        } else {
          showStatus(`Failed to move file`);
        }
      } catch (error) {
        showStatus(`Error moving file`);
      }
    }

    refreshFiles();
  </script>
</body>
</html>
)rawliteral";

void enterWiFiTransferApp() {
  transferState = TRANSFER_MENU;
  uploadCount = 0;
  downloadCount = 0;
  lastAction = "Ready";
  drawTransferMenu();
}

void drawTransferMenu() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE);

  if (WiFi.status() != WL_CONNECTED) {
    canvas.setTextColor(TFT_RED);
    canvas.drawString("Not connected to WiFi!", 40, 30);  // Raised 20px
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("Connect to WiFi first", 50, 45);  // Raised 20px
  } else {
    canvas.setTextColor(0xAFE5);  // WiFi Connected - chartreuse
    canvas.drawString("WiFi Connected", 70, 30);  // Raised 20px
    canvas.setTextColor(0x0400);  // Press ENTER - darker green
    canvas.drawString("Press ENTER to start", 50, 50);  // Raised 20px
    canvas.drawString("web server", 80, 62);  // Raised 20px
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("Upload & Download files", 50, 80);  // Raised 20px
  canvas.drawString("via web browser", 65, 90);  // Raised 20px
  canvas.drawString("`=Back", 95, 100);  // Raised 20px
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawTransferRunning() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  canvas.setTextSize(2);
  canvas.setTextColor(0xFBDF);  // Baby pink
  canvas.drawString("Server Running", 40, 25);
  
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_RED);  // Red for "Open in browser:"
  canvas.drawString("Open in browser:", 10, 45);
  
  canvas.setTextColor(TFT_YELLOW);
  canvas.setTextSize(1);
  String url = "http://" + WiFi.localIP().toString();
  canvas.drawString(url.c_str(), 10, 60);
  
  // Stats
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(("Uploads: " + String(uploadCount)).c_str(), 10, 80);
  canvas.drawString(("Downloads: " + String(downloadCount)).c_str(), 10, 92);

  canvas.setTextColor(TFT_LIGHTGREY);
  canvas.drawString(lastAction.c_str(), 10, 104);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("Any key to stop server", 50, 125);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void startWebServer() {
  if (WiFi.status() != WL_CONNECTED) {
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextColor(TFT_RED);
    canvas.drawString("WiFi not connected!", 50, 60);
    delay(2000);
    return;
  }
  
  // SD card is already initialized in setup() - just check if it's mounted
  if (SD.cardType() == CARD_NONE) {
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextColor(TFT_RED);
    canvas.drawString("SD Card Error!", 60, 60);
    delay(2000);
    return;
  }
  
  webServer = new WebServer(80);
  
  webServer->on("/", HTTP_GET, handleRoot);
  webServer->on("/list", HTTP_GET, handleFileList);
  webServer->on("/directory", HTTP_GET, handleDirectoryContents);
  webServer->on("/info", HTTP_GET, []() {
    String json = "{";
    json += "\"totalMB\":" + String((SD.totalBytes() / 1048576)) + ",";
    json += "\"usedMB\":" + String((SD.usedBytes() / 1048576)) + ",";
    json += "\"type\":\"SD\"";
    json += "}";
    webServer->send(200, "application/json", json);
  });
  webServer->on("/upload", HTTP_POST, []() {
    webServer->send(200);
  }, handleFileUpload);
  webServer->on("/download", HTTP_GET, handleFileDownload);
  webServer->on("/delete", HTTP_DELETE, handleFileDelete);
  webServer->on("/mkdir", HTTP_POST, []() {
    String folderName = webServer->arg("name");
    if (folderName.length() == 0) {
      webServer->send(400, "text/plain", "Folder name required");
      return;
    }

    String folderPath = "/" + folderName;
    if (SD.mkdir(folderPath)) {
      webServer->send(200, "text/plain", "Folder created");
      lastAction = "Created folder: " + folderName;
      safeBeep(1100, 50, false);
      drawTransferRunning();
    } else {
      webServer->send(500, "text/plain", "Failed to create folder");
    }
  });
  webServer->on("/move", HTTP_POST, []() {
    String fromPath = webServer->arg("from");
    String toFolder = webServer->arg("to");

    if (fromPath.length() == 0 || toFolder.length() == 0) {
      webServer->send(400, "text/plain", "Source and destination required");
      return;
    }

    // Ensure paths start with /
    if (!fromPath.startsWith("/")) fromPath = "/" + fromPath;
    if (!toFolder.startsWith("/")) toFolder = "/" + toFolder;

    // Extract filename from source path
    int lastSlash = fromPath.lastIndexOf('/');
    String fileName = fromPath.substring(lastSlash + 1);

    // Build destination path
    String toPath = toFolder;
    if (!toPath.endsWith("/")) toPath += "/";
    toPath += fileName;

    // Use rename to move the file
    if (SD.rename(fromPath.c_str(), toPath.c_str())) {
      webServer->send(200, "text/plain", "File moved");
      lastAction = "Moved: " + fileName;
      safeBeep(1000, 50, false);
      drawTransferRunning();
    } else {
      webServer->send(500, "text/plain", "Failed to move file");
    }
  });

  webServer->begin();
  serverRunning = true;
  transferState = TRANSFER_RUNNING;

  safeBeep(1200, 100, false);
  delay(100);
  safeBeep(1500, 100, false);

  drawTransferRunning();
}

void stopWebServer() {
  if (webServer) {
    webServer->stop();
    delete webServer;
    webServer = nullptr;
  }
  serverRunning = false;
  transferState = TRANSFER_MENU;

  safeBeep(800, 100, false);
  drawTransferMenu();
}

void handleWebServerLoop() {
  if (serverRunning && webServer) {
    webServer->handleClient();
  }
}

void handleRoot() {
  // Connection melody - three ascending beeps!
  safeBeep(800, 80, false);
  delay(100);
  safeBeep(1000, 80, false);
  delay(100);
  safeBeep(1200, 100, false);

  // Send HTML page from PROGMEM (flash memory)
  webServer->send_P(200, "text/html", htmlPage);
}

void listDirectory(File dir, String path, String &json, bool &first, int depth = 0) {
  // Limit recursion depth to avoid stack overflow
  if (depth > 5) return;

  while (true) {
    // Feed watchdog to prevent timeout with many files
    yield();

    File entry = dir.openNextFile();
    if (!entry) break;

    String entryName = String(entry.name());

    // Get filename without path for checking
    String fileName = entryName;
    int lastSlash = fileName.lastIndexOf('/');
    if (lastSlash >= 0) {
      fileName = fileName.substring(lastSlash + 1);
    }

    // Skip system files, hidden files, and temporary files
    if (fileName.startsWith(".") ||
        fileName.startsWith("~") ||
        fileName.endsWith("~") ||
        fileName.startsWith("System Volume Information") ||
        fileName == "FSCK0000.REC" ||
        fileName == "live" ||
        fileName == "store.db" ||
        entryName.indexOf("/.") > 0) {
      entry.close();
      continue;
    }

    if (!first) json += ",";
    first = false;

    if (entry.isDirectory()) {
      json += "{\"name\":\"" + entryName + "\",\"size\":0,\"isDir\":true}";
      // Recursively list subdirectory
      listDirectory(entry, entryName, json, first, depth + 1);
    } else {
      json += "{\"name\":\"" + entryName + "\",\"size\":" + String(entry.size()) + ",\"isDir\":false}";
    }

    entry.close();
  }
}

void handleFileList() {
  // Only list ROOT level - no recursion!
  String json = "[";
  bool first = true;

  File root = SD.open("/");
  if (root) {
    while (true) {
      yield();  // Feed watchdog

      File entry = root.openNextFile();
      if (!entry) break;

      String entryName = String(entry.name());
      String fileName = entryName;
      int lastSlash = fileName.lastIndexOf('/');
      if (lastSlash >= 0) {
        fileName = fileName.substring(lastSlash + 1);
      }

      // Skip system files
      if (fileName.startsWith(".") || fileName.startsWith("~") ||
          fileName.endsWith("~") || fileName.startsWith("System Volume Information") ||
          fileName == "FSCK0000.REC" || fileName == "live" || fileName == "store.db") {
        entry.close();
        continue;
      }

      if (!first) json += ",";
      first = false;

      if (entry.isDirectory()) {
        json += "{\"name\":\"" + entryName + "\",\"size\":0,\"isDir\":true}";
      } else {
        json += "{\"name\":\"" + entryName + "\",\"size\":" + String(entry.size()) + ",\"isDir\":false}";
      }

      entry.close();
    }
    root.close();
  } else {
    json += "{\"name\":\"Error: Cannot open SD card\",\"size\":0,\"isDir\":false}";
  }

  json += "]";
  webServer->send(200, "application/json", json);
}

void handleDirectoryContents() {
  // Load specific directory on-demand
  String dirPath = webServer->arg("path");
  if (dirPath.length() == 0) dirPath = "/";

  // Ensure path starts with /
  if (!dirPath.startsWith("/")) dirPath = "/" + dirPath;

  String json = "[";
  bool first = true;

  File dir = SD.open(dirPath);
  if (dir && dir.isDirectory()) {
    while (true) {
      yield();  // Feed watchdog

      File entry = dir.openNextFile();
      if (!entry) break;

      String entryName = String(entry.name());
      String fileName = entryName;
      int lastSlash = fileName.lastIndexOf('/');
      if (lastSlash >= 0) {
        fileName = fileName.substring(lastSlash + 1);
      }

      // Skip system files
      if (fileName.startsWith(".") || fileName.startsWith("~") ||
          fileName.endsWith("~") || fileName.startsWith("System Volume Information") ||
          fileName == "FSCK0000.REC" || fileName == "live" || fileName == "store.db") {
        entry.close();
        continue;
      }

      if (!first) json += ",";
      first = false;

      if (entry.isDirectory()) {
        json += "{\"name\":\"" + entryName + "\",\"size\":0,\"isDir\":true}";
      } else {
        json += "{\"name\":\"" + entryName + "\",\"size\":" + String(entry.size()) + ",\"isDir\":false}";
      }

      entry.close();
    }
    dir.close();
  } else {
    json += "{\"name\":\"Error: Cannot open directory\",\"size\":0,\"isDir\":false}";
  }

  json += "]";
  webServer->send(200, "application/json", json);
}

void handleFileUpload() {
  HTTPUpload& upload = webServer->upload();

  static File uploadFile;
  static String uploadPath;

  if (upload.status == UPLOAD_FILE_START) {
    // Get the path parameter if provided, otherwise use just filename
    uploadPath = webServer->arg("path");
    if (uploadPath.length() == 0) {
      uploadPath = String(upload.filename);
    }

    // Add leading slash if not present
    if (!uploadPath.startsWith("/")) {
      uploadPath = "/" + uploadPath;
    }

    // Create directory structure if needed
    int lastSlash = uploadPath.lastIndexOf('/');
    if (lastSlash > 0) {
      String dirPath = uploadPath.substring(0, lastSlash);

      // Create all directories in the path
      String currentPath = "";
      int start = 1; // Skip leading slash
      while (start < dirPath.length()) {
        int slashPos = dirPath.indexOf('/', start);
        if (slashPos == -1) slashPos = dirPath.length();

        currentPath += "/" + dirPath.substring(start, slashPos);
        if (!SD.exists(currentPath)) {
          SD.mkdir(currentPath);
        }
        start = slashPos + 1;
      }
    }

    // Open file for writing
    uploadFile = SD.open(uploadPath, FILE_WRITE);
    if (uploadFile) {
      lastAction = "Uploading: " + uploadPath;
      drawTransferRunning();
    } else {
      lastAction = "Error: Cannot create " + uploadPath;
      drawTransferRunning();
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      uploadCount++;
      lastAction = "Uploaded: " + uploadPath;
      safeBeep(1000, 50, false);
      drawTransferRunning();
    }
  }
}

void handleFileDownload() {
  String filename = webServer->arg("file");
  String filepath = "/" + filename;
  
  if (!SD.exists(filepath)) {
    webServer->send(404, "text/plain", "File not found");
    return;
  }
  
  File file = SD.open(filepath, FILE_READ);
  if (!file) {
    webServer->send(500, "text/plain", "Failed to open file");
    return;
  }
  
  webServer->streamFile(file, "application/octet-stream");
  file.close();

  downloadCount++;
  lastAction = "Downloaded: " + filename;
  safeBeep(1200, 50, false);
  drawTransferRunning();
}

void handleFileDelete() {
  String filename = webServer->arg("file");
  String filepath = "/" + filename;

  // Check if it's a directory
  File entry = SD.open(filepath);
  bool isDir = false;
  if (entry) {
    isDir = entry.isDirectory();
    entry.close();
  }

  bool success = false;
  if (isDir) {
    success = SD.rmdir(filepath);
  } else {
    success = SD.remove(filepath);
  }

  if (success) {
    webServer->send(200, "text/plain", isDir ? "Folder deleted" : "File deleted");
    lastAction = "Deleted: " + filename;
    safeBeep(800, 50, false);
    drawTransferRunning();
  } else {
    webServer->send(500, "text/plain", isDir ? "Failed to delete folder (may not be empty)" : "Failed to delete file");
  }
}