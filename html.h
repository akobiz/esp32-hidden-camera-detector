const char index_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8">
    <title>Hidden Camera Detector</title>
    <style>
      * {
        box-sizing: border-box;
        margin: 0;
        padding: 0;
      }

      body {
        font-family: Arial, sans-serif;
        background-color: #f5f7fa;
        color: #333;
        height: 100vh;
        display: flex;
        flex-direction: row;
      }

      /* Sidebar */
      .sidebar {
        width: 30%;
        background-color: #2c3e50;
        color: white;
        padding: 20px;
        display: flex;
        flex-direction: column;
        justify-content: flex-start;
        gap: 20px;
      }

      .sidebar-header {
        display: flex;
        align-items: center;
        gap: 15px;
        padding: 20px;
        border-bottom: 2px solid #1a252f;
        margin: -20px -20px 20px -20px;
        background-color: #1a252f;
        box-shadow: 0 2px 5px rgba(0,0,0,0.3);
      }

      .sidebar-header svg {
        width: 50px;
        height: 50px;
        filter: drop-shadow(0 2px 3px rgba(0,0,0,0.3));
      }

      .sidebar h1 {
        font-size: 1.8em;
        margin: 0;
        color: #ffffff;
        text-shadow: 1px 1px 2px rgba(0,0,0,0.3);
        font-weight: 600;
      }

      .button {
        padding: 12px 16px;
        background-color: #3498db;
        color: white;
        border: none;
        border-radius: 6px;
        font-size: 1em;
        cursor: pointer;
        transition: background-color 0.3s ease;
      }

      .button:hover {
        background-color: #2980b9;
      }

      .button:disabled {
        background-color: #95a5a6;
        cursor: not-allowed;
        opacity: 0.7;
      }

      .status-container {
        margin-top: 20px;
      }

      .status-label {
        font-size: 1.1em;
        margin-bottom: 8px;
      }

      .status-indicator {
        width: 20px;
        height: 20px;
        border-radius: 50%;
        display: inline-block;
        vertical-align: middle;
        margin-left: 10px;
        background-color: green; /* Default: free */
      }

      /* Main content */
      .main {
        width: 70%;
        padding: 20px;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: flex-start;
        height: 100vh;
        overflow: hidden;
      }

      .main p {
        font-size: 1.1em;
        margin-bottom: 20px;
        text-align: center;
      }

      #photo {
        width: 100%;
        height: 70vh;
        object-fit: contain;
        border: 2px solid #ccc;
        border-radius: 10px;
        box-shadow: 0 4px 10px rgba(0,0,0,0.1);
        background-color: #000;
      }

      #statusIndicator {
          width: 20px;
          height: 20px;
          border-radius: 50%;
          background-color: gray;
          transition: background-color 0.3s ease;
        }

        /* Анімація миготіння */
        @keyframes pulse {
          0%   { background-color: red; }
          50%  { background-color: darkred; }
          100% { background-color: red; }
        }

        .blinking {
          animation: pulse 1s infinite;
        }

        .progress-bar {
          width: 100%;
          height: 20px;
          background-color: #bdc3c7;
          border-radius: 10px;
          overflow: hidden;
          margin-top: 10px;
        }

        #progressFill {
          height: 100%;
          width: 0%;
          background-color: #27ae60;
          transition: width 0.5s ease;
        }

        #progressText {
          font-size: 0.9em;
          margin-top: 5px;
          display: inline-block;
        }

        .camera-detected {
          color: #e74c3c !important;
          font-weight: bold !important;
          text-transform: uppercase;
          animation: blink-text 1s infinite;
        }

        @keyframes blink-text {
          0% { opacity: 1; }
          50% { opacity: 0.5; }
          100% { opacity: 1; }
        }

        .highlight-button {
          background-color: #e74c3c !important;
          animation: pulse-button 1.5s infinite;
        }

        @keyframes pulse-button {
          0% { transform: scale(1); }
          50% { transform: scale(1.05); }
          100% { transform: scale(1); }
        }

        /* Адаптивні стилі */
        @media screen and (max-width: 1024px) {
          body {
            flex-direction: column;
          }
          
          .sidebar {
            width: 100%;
            height: auto;
          }
          
          .main {
            width: 100%;
            height: auto;
            padding: 10px;
          }
          
          #photo {
            height: 50vh;
          }
        }

        @media screen and (max-width: 768px) {
          .sidebar-header {
            padding: 15px;
          }
          
          .sidebar-header svg {
            width: 40px;
            height: 40px;
          }
          
          .sidebar h1 {
            font-size: 1.5em;
          }
          
          #photo {
            height: 40vh;
          }
        }

        @media screen and (max-width: 480px) {
          .sidebar {
            padding: 10px;
          }
          
          .button {
            padding: 10px 14px;
            font-size: 0.9em;
          }
          
          #photo {
            height: 30vh;
          }
        }

    </style>
  </head>
  <body>
    <div class="sidebar">
      <div class="sidebar-header">
        <svg viewBox="0 0 120 120" fill="none" xmlns="http://www.w3.org/2000/svg">
          <ellipse cx="60" cy="60" rx="50" ry="30" fill="#e0e0e0" stroke="#333" stroke-width="4"/>
          <circle cx="60" cy="60" r="18" fill="#fff" stroke="#333" stroke-width="4"/>
          <circle cx="60" cy="60" r="8" fill="#1976d2"/>
          <circle cx="65" cy="55" r="3" fill="#fff" opacity="0.7"/>
          <rect x="80" y="80" width="20" height="6" rx="3" fill="#1976d2" transform="rotate(45 80 80)"/>
        </svg>
        <h1>Camera Tools</h1>
      </div>
      <button class="button" id="startScanButton" onclick="startScan()">Розпочати сканування</button>
      <button class="button" id="continueButton" onclick="continueScan()" disabled>Продовжити сканування</button>
      <button class="button" id="refreshButton" onclick="stopScan()">Зупинити сканування</button>

      <div class="status-container">
          <div id="statusIndicator"></div>
          <span id="statusText">Перевіряю статус...</span>
          <div class="progress-bar">
            <div id="progressFill"></div>
          </div>
        <span id="progressText">0%</span>

      </div>

    </div>

    <div class="main">
      <p>Можлива прихована камера розміщена: :</p>
      <img id="photo" src="/photo" alt="Latest photo">
    </div>

    <script>
      // Авто-оновлення зображення кожну 0.5 секунд
      setInterval(() => {
          const img = document.getElementById("photo");
          img.src = "/photo?" + new Date().getTime(); // кешобхідник
        }, 500); // кожні 0.5 секунд
        
      // Змінні для стану застосунку  
      let isScanning = false;
      let isFlashDetected = false;
      
      // Функція для оновлення стану кнопок
      function updateButtonsState() {
        const startButton = document.getElementById("startScanButton");
        const continueButton = document.getElementById("continueButton");
        
        if (isFlashDetected) {
          // Якщо виявлено відблиск - можна продовжити сканування
          startButton.disabled = true;
          continueButton.disabled = false;
        } else if (isScanning) {
          // Якщо сканування в процесі - обидві кнопки неактивні
          startButton.disabled = true;
          continueButton.disabled = true;
        } else {
          // Якщо сканування не активне - можна почати сканування
          startButton.disabled = false;
          continueButton.disabled = true;
        }
      }

      function startScan() {
        fetch('/start-scan')
          .then(res => res.text())
          .then(msg => {
            alert(msg);
            isScanning = true;
            updateButtonsState();
          });
      }
      
      function continueScan() {
        fetch('/continue-scan')
          .then(res => res.text())
          .then(msg => {
            alert(msg);
            isFlashDetected = false;
            updateButtonsState();
          });
      }
      
      function stopScan() {
        fetch('/stop-scanning')
          .then(response => response.text())
          .then(msg => {
            alert(msg); // Показати повідомлення користувачу
            // Додатково можна оновити стан кнопок чи інтерфейсу
          })
          .catch(err => {
            alert("Помилка зупинки сканування!");
            console.error(err);
          });
      }
      
      function refreshImage() {
        const img = document.getElementById("photo");
        img.src = "/photo?" + new Date().getTime();
      }

      function updateStatus() {
        // Перевіряємо статус сканування
        fetch("/status")
          .then(response => response.text())
          .then(status => {
            const indicator = document.getElementById("statusIndicator");
            const statusText = document.getElementById("statusText");

            isScanning = status.trim() === "true";
            
            if (isScanning) {
              indicator.classList.add("blinking");
              statusText.innerText = "Система в процесі сканування простору...";
            } else {
              indicator.classList.remove("blinking");
              indicator.style.backgroundColor = "green";
              statusText.innerText = "Система очікує команди користувача...";
            }
            
            // Після оновлення статусу сканування, перевіряємо статус виявлення
            checkFlashDetected();
          })
          .catch(err => {
            console.error("Status fetch error:", err);
          });
      }
      
      function checkFlashDetected() {
        fetch("/is-flash-detected")
          .then(response => response.text())
          .then(status => {
            isFlashDetected = status.trim() === "true";
            const statusText = document.getElementById("statusText");
            const continueButton = document.getElementById("continueButton");
            
            // Оновлюємо текст статусу, якщо виявлено відблиск
            if (isFlashDetected) {
              statusText.innerText = "МОЖЛИВО ВИЯВЛЕНО КАМЕРУ! Перевірте зображення.";
              statusText.classList.add("camera-detected");
              continueButton.classList.add("highlight-button");
            } else {
              statusText.classList.remove("camera-detected");
              continueButton.classList.remove("highlight-button");
            }
            
            updateButtonsState();
          })
          .catch(err => {
            console.error("Flash detection status fetch error:", err);
          });
      }
      
      function updateProgress() {
        fetch("/progress")
          .then(response => response.text())
          .then(value => {
            const percent = parseInt(value);
            const fill = document.getElementById("progressFill");
            const text = document.getElementById("progressText");

            if (!isNaN(percent)) {
              fill.style.width = percent + "%";
              text.innerText = percent + "%";
            }
          })
          .catch(err => console.error("Progress fetch error:", err));
      }

  setInterval(updateStatus, 2000);
  setInterval(updateProgress, 5000);
  window.onload = function() {
    updateStatus();
    updateProgress();
  };
    </script>
  </body>
</html>
)rawliteral";
