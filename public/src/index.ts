const toArray = <Element>(
  nodelist:
    | HTMLCollectionOf<globalThis.Element>
    | NodeListOf<globalThis.Element>
): Element[] => {
  return Array.prototype.slice.call(nodelist);
};

const GBSStorage = {
  lsObject: {},
  write(key: string, value: string) {
    GBSStorage.lsObject = GBSStorage.lsObject || {};
    GBSStorage.lsObject[key] = value;
    localStorage.setItem(
      "GBSControlSlotNames",
      JSON.stringify(GBSStorage.lsObject)
    );
  },
  read(key: string): string {
    GBSStorage.lsObject = JSON.parse(
      localStorage.getItem("GBSControlSlotNames") || "{}"
    );
    return GBSStorage.lsObject[key];
  },
};

const GBSControl = {
  controlKeysMobileMode: "move",
  serverIP: "",
  webSocketServerUrl: "",
  theTerminal: null,
  ws: null,
  wsNoSuccessConnectingCounter: 0,
  wsConnectCounter: 0,
  isWsActive: false,
  wsTimeout: 0,
  queuedText: "",
  dataQueued: 0,
  wsCheckTimer: 0,
  updateTerminalTimer: 0,
  timeOutWs: 0,
  buttonMapping: {
    1: "button1280x960",
    2: "button1280x1024",
    3: "button1280x720",
    4: "button720x480",
    5: "button1920x1080",
    6: "button15kHzScaleDown",
    8: "buttonSourcePassThrough",
    9: "buttonLoadCustomPreset",
    11: "slot1",
    12: "slot2",
    13: "slot3",
    14: "slot4",
    15: "slot5",
    16: "slot6",
    17: "slot7",
    18: "slot8",
    19: "slot9",
  },
  controlKeysMobile: {
    move: {
      type: "loadDoc",
      left: "7",
      up: "*",
      right: "6",
      down: "/",
    },
    scale: {
      type: "loadDoc",
      left: "h",
      up: "4",
      right: "z",
      down: "5",
    },
    borders: {
      type: "loadUser",
      left: "B",
      up: "C",
      right: "A",
      down: "D",
    },
  },
};

const toggleButtonsFor = (id: string) => (
  button: HTMLElement,
  _index: any,
  _array: any
) => {
  button.removeAttribute("active");
  if (button.getAttribute("id") === id) {
    button.setAttribute("active", "");
  }
};

const checkWebSocketServer = () => {
  if (!GBSControl.isWsActive) {
    if (GBSControl.ws) {
      /*
                        0     CONNECTING
                        1     OPEN
                        2     CLOSING
                        3     CLOSED
                        */
      switch (GBSControl.ws.readyState) {
        case 1:
        case 2:
          GBSControl.ws.close();
          break;
        case 3:
          GBSControl.ws = null;
          break;
      }
    }
    if (!GBSControl.ws) {
      createWebSocket();
    }
  }
};

const timeOutWs = () => {
  console.log("timeOutWs");
  if (GBSControl.ws) {
    GBSControl.ws.close();
  }
  GBSControl.isWsActive = false;
  document.getElementById("overlayNoWs").style.display = "block"; /*show*/
};

const createWebSocket = () => {
  if (GBSControl.ws && checkReadyState()) {
    return;
  }

  GBSControl.wsNoSuccessConnectingCounter = 0;
  GBSControl.ws = new WebSocket(GBSControl.webSocketServerUrl, ["arduino"]);

  GBSControl.ws.onopen = function () {
    GBSControl.wsConnectCounter++;
    console.log("ws onopen");
    GBSControl.isWsActive = true;
    document.getElementById("overlayNoWs").style.display = "none"; /*hide*/
    GBSControl.wsTimeout = setTimeout(timeOutWs, 6000);
    GBSControl.wsNoSuccessConnectingCounter = 0;
    /*theTerminal.value += 'ws' + wsConnectCounter + ' connected\n';
                      theTerminal.scrollTop = theTerminal.scrollHeight;*/
  };
  GBSControl.ws.onclose = () => {
    console.log("ws.onclose");
    clearTimeout(GBSControl.wsTimeout);
    GBSControl.isWsActive = false;
  };
  GBSControl.ws.onmessage = function (e: { data: string | any[] }) {
    clearTimeout(GBSControl.wsTimeout);
    GBSControl.wsTimeout = setTimeout(timeOutWs, 2700);
    GBSControl.isWsActive = true;
    if (e.data[0] != "#") {
      GBSControl.queuedText += e.data;
      GBSControl.dataQueued += e.data.length;
      /*console.log(queuedText);
                          console.log(dataQueued);*/
      if (GBSControl.dataQueued >= 70000) {
        GBSControl.theTerminal.value = "";
        GBSControl.dataQueued = 0;
      }
    } else {
      const presetId = GBSControl.buttonMapping[e.data[1]];
      console.log("presetID", presetId, e.data[1]);
      const presetEl = document.getElementById(presetId);
      const activePresetButton = presetEl ? presetEl.getAttribute("id") : null;
      if (activePresetButton) {
        const presetButtonList = toArray(
          document.querySelectorAll("[role='presset']")
        ) as HTMLElement[];
        presetButtonList.forEach(toggleButtonsFor(activePresetButton));
      }

      const slotId = GBSControl.buttonMapping["1" + e.data[2]];
      const activeSlotButton = document.getElementById(slotId);
      if (activeSlotButton) {
        const slotButtonList = toArray(
          document.querySelectorAll('[role="slot"]')
        ) as HTMLElement[];

        slotButtonList.forEach(toggleButtonsFor(slotId));
      }

      if (e.data[3] && e.data[4] && e.data[5]) {
        const optionByte0 = e.data[3].charCodeAt(0);
        const optionByte1 = e.data[4].charCodeAt(0);
        const optionByte2 = e.data[5].charCodeAt(0);
        const optionButtonList = [
          ...toArray<HTMLButtonElement>(document.querySelectorAll("[toggle]")),
          ...toArray<HTMLButtonElement>(
            document.querySelectorAll("[toggle-switch]")
          ),
        ];

        const toggleMethod = (button, mode) => {
          if (button.tagName === "TD") {
            button.innerText = mode ? "toggle_on" : "toggle_off";
          }
          button = button.tagName !== "TD" ? button : button.parentElement;
          if (mode) {
            button.setAttribute("active", "");
          } else {
            button.removeAttribute("active");
          }
        };
        optionButtonList.forEach((button) => {
          const toggleData =
            button.getAttribute("toggle") ||
            button.getAttribute("toggle-switch");

          switch (toggleData) {
            case "adcAutoGain":
              toggleMethod(button, (optionByte0 & 0x01) == 0x01);
              break;
            case "scanlines":
              toggleMethod(button, (optionByte0 & 0x02) == 0x02);
              break;
            case "vdsLineFilter":
              toggleMethod(button, (optionByte0 & 0x04) == 0x04);
              break;
            case "peaking":
              toggleMethod(button, (optionByte0 & 0x08) == 0x08);
              break;
            case "palForce60":
              toggleMethod(button, (optionByte0 & 0x10) == 0x10);
              break;
            case "wantOutputComponent":
              toggleMethod(button, (optionByte0 & 0x20) == 0x20);
              break;
            /** 1 */

            case "matched":
              toggleMethod(button, (optionByte1 & 0x01) == 0x01);
              break;
            case "frameTimeLock":
              toggleMethod(button, (optionByte1 & 0x02) == 0x02);
              break;
            case "motionAdaptive":
              toggleMethod(button, (optionByte1 & 0x04) == 0x04);
              break;
            case "bob":
              toggleMethod(button, (optionByte1 & 0x04) != 0x04);
              break;
            // case "tap6":
            //   toggleMethod(button, (optionByte1 & 0x08) != 0x04);
            //   break;
            case "step":
              // TODO CHECK
              toggleMethod(button, (optionByte1 & 0x10) == 0x10);
              break;
            case "fullHeight":
              toggleMethod(button, (optionByte1 & 0x20) == 0x20);
              break;
            /** 2 */
            case "enableCalibrationADC":
              toggleMethod(button, (optionByte2 & 0x01) == 0x01);
              break;
            case "preferScalingRgbhv":
              toggleMethod(button, (optionByte2 & 0x02) == 0x02);
              break;
          }
        });
      }
    }
  };
};

const checkReadyState = () => {
  if (GBSControl.ws.readyState == 2) {
    GBSControl.wsNoSuccessConnectingCounter++;
    /* console.log(wsNoSuccessConnectingCounter); */
    if (GBSControl.wsNoSuccessConnectingCounter >= 7) {
      console.log("ws still closing, force close");
      GBSControl.ws = null;
      GBSControl.wsNoSuccessConnectingCounter = 0;
      /* fall through */
      createWebSocket();
      return false;
    } else {
      return true;
    }
  } else if (GBSControl.ws.readyState == 0) {
    GBSControl.wsNoSuccessConnectingCounter++;
    /*console.log(wsNoSuccessConnectingCounter); */
    if (GBSControl.wsNoSuccessConnectingCounter >= 14) {
      console.log("ws still connecting, retry");
      GBSControl.ws.close();
      GBSControl.wsNoSuccessConnectingCounter = 0;
    }
    return true;
  } else {
    return true;
  }
};

const updateTerminal = () => {
  if (GBSControl.queuedText.length > 0) {
    requestAnimationFrame(function () {
      GBSControl.theTerminal.value += GBSControl.queuedText;
      GBSControl.theTerminal.scrollTop = GBSControl.theTerminal.scrollHeight;
      GBSControl.queuedText = "";
    });
  }
};

const loadDoc = (link: string) => {
  const xhttp = new XMLHttpRequest();
  xhttp.open(
    "GET",
    `http://${GBSControl.serverIP}/sc?${link}&nocache=${new Date().getTime()}`,
    true
  );
  xhttp.send();
};

const loadUser = (link: string) => {
  const xhttp = new XMLHttpRequest();
  xhttp.open(
    "GET",
    `http://${GBSControl.serverIP}/uc?${link}&nocache=${new Date().getTime()}`,
    true
  );
  xhttp.send();
  if (link == "a" || link == "1") {
    GBSControl.isWsActive = false;
    GBSControl.theTerminal.value += " Restart\n";
    GBSControl.theTerminal.scrollTop = GBSControl.theTerminal.scrollHeight;
  }
};

const initializeMenuButtons = () => {
  const menuButtons = toArray<HTMLButtonElement>(
    document.querySelector(".menu").querySelectorAll("button")
  );

  const sections = toArray<HTMLElement>(document.querySelectorAll("section"));
  const scroll = document.querySelector(".scroll");
  menuButtons.forEach((e) =>
    e.addEventListener("click", function () {
      const section = this.getAttribute("section");
      sections.forEach((e) => e.setAttribute("hidden", ""));
      document
        .querySelector(`section[name="${section}"]`)
        .removeAttribute("hidden");
      menuButtons.forEach((b) => b.removeAttribute("active"));
      this.setAttribute("active", "");
      scroll.scrollTo(0, 1);
    })
  );
};

const initializeUserButtons = () => {
  const userButtons = toArray<HTMLButtonElement>(
    document.querySelectorAll("[ws-user]")
  );
  userButtons.forEach((e) =>
    e.addEventListener("click", function () {
      const wsUser = this.getAttribute("ws-user");
      loadUser(wsUser);
    })
  );
};

const initializeActionButtons = () => {
  const actionButtons = toArray<HTMLButtonElement>(
    document.querySelectorAll("[ws-action]")
  );
  actionButtons.forEach((e) =>
    e.addEventListener("click", function () {
      const wsAction = this.getAttribute("ws-action");
      loadDoc(wsAction);
    })
  );
};

const initializeClearButton = () => {
  document.querySelector(".clear").addEventListener("click", () => {
    GBSControl.theTerminal.value = "";
  });
};

const controlClick = (control: HTMLButtonElement) => () => {
  const controlKey = control.getAttribute("controls-key");
  const target = GBSControl.controlKeysMobile[GBSControl.controlKeysMobileMode];
  switch (target.type) {
    case "loadDoc":
      loadDoc(target[controlKey]);
      break;
    case "loadUser":
      loadUser(target[controlKey]);
      break;
  }
};

const controlMouseDown = (control: HTMLButtonElement) => () => {
  const click = controlClick(control);
  click();
  control["__interval"] = setInterval(click, 300);
};

const controlMouseUp = (control: HTMLButtonElement) => () => {
  clearInterval(control["__interval"]);
};

const gainClick = (control: HTMLButtonElement) => () => {
  const gainKey = control.getAttribute("gain-key");
  switch (gainKey) {
    case "+":
      loadUser("n");
      break;
    case "-":
      loadUser("o");
      break;
  }
};

const gainMouseDown = (control: HTMLButtonElement) => () => {
  const click = gainClick(control);
  click();
  control["__interval"] = setInterval(click, 300);
};

const gainMouseUp = (control: HTMLButtonElement) => () => {
  clearInterval(control["__interval"]);
};

const initializeControlMobileKeys = () => {
  document.querySelectorAll("[controls]").forEach((control) => {
    control.addEventListener("click", () => {
      document.querySelectorAll("[controls]").forEach((control) => {
        control.removeAttribute("active");
      });
      GBSControl.controlKeysMobileMode = control.getAttribute("controls");
      control.setAttribute("active", "");
    });
  });
  document.querySelectorAll("[controls-key]").forEach((control) => {
    !("ontouchstart" in window) &&
      control.addEventListener(
        "mousedown",
        controlMouseDown(control as HTMLButtonElement)
      );
    "ontouchstart" in window &&
      control.addEventListener(
        "touchstart",
        controlMouseDown(control as HTMLButtonElement)
      );
    !("ontouchstart" in window) &&
      control.addEventListener(
        "mouseup",
        controlMouseUp(control as HTMLButtonElement)
      );
    "ontouchstart" in window &&
      control.addEventListener(
        "touchend",
        controlMouseUp(control as HTMLButtonElement)
      );
  });
};

const initializeGainKeys = () => {
  document.querySelectorAll("[gain-key]").forEach((control) => {
    !("ontouchstart" in window) &&
      control.addEventListener(
        "mousedown",
        gainMouseDown(control as HTMLButtonElement)
      );
    "ontouchstart" in window &&
      control.addEventListener(
        "touchstart",
        gainMouseDown(control as HTMLButtonElement)
      );
    !("ontouchstart" in window) &&
      control.addEventListener(
        "mouseup",
        gainMouseUp(control as HTMLButtonElement)
      );
    "ontouchstart" in window &&
      control.addEventListener(
        "touchend",
        gainMouseUp(control as HTMLButtonElement)
      );
  });
};

const savePresset = () => {
  const currentSlot = document.querySelector('[role="slot"][active]');
  const key = currentSlot.getAttribute("id");
  let currentName = prompt("Assign a slot name", GBSStorage.read(key) || key);
  GBSStorage.write(key, currentName);
  currentSlot.setAttribute("name", currentName);
  loadUser("4");
};

const initSlotNames = () => {
  for (let i = 1; i < 10; i++) {
    const slot = "slot" + i;
    document
      .getElementById(slot)
      .setAttribute("name", GBSStorage.read(slot) || slot);
  }
};

const initLegendHelpers = () => {
  toArray<HTMLElement>(document.querySelectorAll("legend")).forEach((e) => {
    e.addEventListener("click", () => {
      document.body.classList.toggle("hide-help");
    });
  });
};

const initialize = () => {
  const ip = location.hostname;
  GBSControl.serverIP = ip;
  GBSControl.webSocketServerUrl = `ws://${ip}:81/`;
  initializeMenuButtons();
  initializeUserButtons();
  initializeActionButtons();
  initializeClearButton();
  initializeControlMobileKeys();
  initializeGainKeys();
  initSlotNames();
  initLegendHelpers();
  GBSControl.theTerminal = document.getElementById("outputTextArea");
  GBSControl.ws = null;
  createWebSocket();
  GBSControl.wsCheckTimer = setInterval(checkWebSocketServer, 500);
  GBSControl.updateTerminalTimer = setInterval(updateTerminal, 50);

  window.addEventListener("unload", function (event) {
    clearInterval(GBSControl.wsCheckTimer);
    if (GBSControl.ws) {
      if (GBSControl.ws.readyState == 0 || GBSControl.ws.readyState == 1) {
        GBSControl.ws.close();
      }
    }
    console.log("unload");
  });
};

initialize();
