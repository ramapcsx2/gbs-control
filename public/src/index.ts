/** STRUCTS */
interface Struct {
  name: string;
  type: "byte" | "string";
  size: number;
}

interface StructDescriptors {
  [key: string]: Struct[];
}

const Structs: StructDescriptors = {
  slots: [
    { name: "name", type: "string", size: 25 },
    { name: "presetID", type: "byte", size: 1 },
    { name: "scanlines", type: "byte", size: 1 },
    { name: "scanlinesStrength", type: "byte", size: 1 },
    { name: "slot", type: "byte", size: 1 },
    { name: "wantVdsLineFilter", type: "byte", size: 1 },
    { name: "wantStepResponse", type: "byte", size: 1 },
    { name: "wantPeaking", type: "byte", size: 1 },
  ],
};

const StructParser = {
  pos: 0,
  parseStructArray(
    buff: ArrayBuffer,
    structsDescriptors: StructDescriptors,
    struct: string
  ) {
    const currentStruct = structsDescriptors[struct];

    this.pos = 0;
    buff = new Uint8Array(buff);

    if (currentStruct) {
      const structSize = StructParser.getSize(structsDescriptors, struct);

      return [...Array(buff.byteLength / structSize)].map(() => {
        return currentStruct.reduce((acc, structItem) => {
          acc[structItem.name] = this.getValue(buff, structItem);
          return acc;
        }, {});
      });
    }

    return null;
  },
  getValue(buff: any[], structItem: { type: "byte" | "string"; size: number }) {
    switch (structItem.type) {
      case "byte":
        return buff[this.pos++];

      case "string":
        const currentPos = this.pos;
        this.pos += structItem.size;

        return [...Array(structItem.size)]
          .map(() => " ")
          .map((_char, index) => {
            if (buff[currentPos + index] > 31) {
              return String.fromCharCode(buff[currentPos + index]);
            }
            return "";
          })
          .join("")
          .trim();
    }
  },
  getSize(structsDescriptors: StructDescriptors, struct: string) {
    const currentStruct = structsDescriptors[struct];
    return currentStruct.reduce((acc, prop) => {
      acc += prop.size;
      return acc;
    }, 0);
  },
};

/* GBSControl Global Object*/
const GBSControl = {
  buttonMapping: {
    1: "button1280x960",
    2: "button1280x1024",
    3: "button1280x720",
    4: "button720x480",
    5: "button1920x1080",
    6: "button15kHzScaleDown",
    8: "buttonSourcePassThrough",
    9: "buttonLoadCustomPreset",
  },
  controlKeysMobileMode: "move",
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
  dataQueued: 0,
  isWsActive: false,
  maxSlots: 72,
  queuedText: "",
  scanSSIDDone: false,
  serverIP: "",
  structs: null,
  timeOutWs: 0,
  ui: {
    backupButton: null,
    backupInput: null,
    customSlotFilters: null,
    developerSwitch: null,
    loader: null,
    outputClear: null,
    presetButtonList: null,
    progressBackup: null,
    progressRestore: null,
    slotButtonList: null,
    slotContainer: null,
    terminal: null,
    toggleList: null,
    toggleSwichList: null,
    webSocketConnectionWarning: null,
    wifiConnect: null,
    wifiConnectButton: null,
    wifiList: null,
    wifiListTable: null,
    wifiPasswordInput: null,
    wifiSSDInput: null,
    wifiApButton: null,
    wifiStaButton: null,
    wifiStaSSID: null,
    alert: null,
    alertOk: null,
    alertContent: null,
    prompt: null,
    promptOk: null,
    promptCancel: null,
    promptContent: null,
    promptInput: null,
  },
  updateTerminalTimer: 0,
  webSocketServerUrl: "",
  wifi: {
    mode: "ap",
    ssid: "",
  },
  ws: null,
  wsCheckTimer: 0,
  wsConnectCounter: 0,
  wsNoSuccessConnectingCounter: 0,
  wsTimeout: 0,
};

/** websocket services */
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
  displayWifiWarning(true);
};

const createWebSocket = () => {
  if (GBSControl.ws && checkReadyState()) {
    return;
  }

  GBSControl.wsNoSuccessConnectingCounter = 0;
  GBSControl.ws = new WebSocket(GBSControl.webSocketServerUrl, ["arduino"]);

  GBSControl.ws.onopen = () => {
    console.log("ws onopen");

    displayWifiWarning(false);

    GBSControl.wsConnectCounter++;
    clearTimeout(GBSControl.wsTimeout);
    GBSControl.wsTimeout = setTimeout(timeOutWs, 6000);
    GBSControl.isWsActive = true;
    GBSControl.wsNoSuccessConnectingCounter = 0;
  };

  GBSControl.ws.onclose = () => {
    console.log("ws.onclose");

    clearTimeout(GBSControl.wsTimeout);
    GBSControl.isWsActive = false;
  };

  GBSControl.ws.onmessage = (message: any) => {
    clearTimeout(GBSControl.wsTimeout);
    GBSControl.wsTimeout = setTimeout(timeOutWs, 2700);
    GBSControl.isWsActive = true;

    const [
      messageDataAt0,
      messageDataAt1,
      messageDataAt2,
      messageDataAt3,
      messageDataAt4,
      messageDataAt5,
    ] = message.data;

    if (messageDataAt0 != "#") {
      GBSControl.queuedText += message.data;
      GBSControl.dataQueued += message.data.length;

      if (GBSControl.dataQueued >= 70000) {
        GBSControl.ui.terminal.value = "";
        GBSControl.dataQueued = 0;
      }
    } else {
      const presetId = GBSControl.buttonMapping[messageDataAt1];
      const presetEl = document.querySelector(
        `[gbs-element-ref="${presetId}"]`
      );
      const activePresetButton = presetEl
        ? presetEl.getAttribute("gbs-element-ref")
        : "none";

      GBSControl.ui.presetButtonList.forEach(
        toggleButtonActive(activePresetButton)
      );

      const slotId = "slot-" + messageDataAt2;
      const activeSlotButton = document.querySelector(
        `[gbs-element-ref="${slotId}"]`
      );

      if (activeSlotButton) {
        GBSControl.ui.slotButtonList.forEach(toggleButtonActive(slotId));
      }

      if (messageDataAt3 && messageDataAt4 && messageDataAt5) {
        const optionByte0 = messageDataAt3.charCodeAt(0);
        const optionByte1 = messageDataAt4.charCodeAt(0);
        const optionByte2 = messageDataAt5.charCodeAt(0);
        const optionButtonList = [
          ...nodelistToArray<HTMLButtonElement>(GBSControl.ui.toggleList),
          ...nodelistToArray<HTMLButtonElement>(GBSControl.ui.toggleSwichList),
        ];

        const toggleMethod = (
          button: HTMLTableCellElement | HTMLElement,
          mode: boolean
        ) => {
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
            button.getAttribute("gbs-toggle") ||
            button.getAttribute("gbs-toggle-switch");

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
            case "frameTimeLockMethod_vtotal_vsst":
              toggleMethod(button, (optionByte1 & 0x02) == 0x02 && (optionByte1 & 0x08) != 0x08);
              break;
            case "frameTimeLockMethod_vtotal_only":
              toggleMethod(button, (optionByte1 & 0x02) == 0x02 && (optionByte1 & 0x08) == 0x08);
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
            case "disableExternalClockGenerator":
              toggleMethod(button, (optionByte2 & 0x04) == 0x04);
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

const createIntervalChecks = () => {
  GBSControl.wsCheckTimer = setInterval(checkWebSocketServer, 500);
  GBSControl.updateTerminalTimer = setInterval(updateTerminal, 50);
};

/* API services */

const loadDoc = (link: string) => {
  return fetch(
    `http://${GBSControl.serverIP}/sc?${link}&nocache=${new Date().getTime()}`
  );
};

const loadUser = (link: string) => {
  if (link == "a" || link == "1") {
    GBSControl.isWsActive = false;
    GBSControl.ui.terminal.value += "\nRestart\n";
    GBSControl.ui.terminal.scrollTop = GBSControl.ui.terminal.scrollHeight;
  }

  return fetch(
    `http://${GBSControl.serverIP}/uc?${link}&nocache=${new Date().getTime()}`
  );
};

/** SLOT management */

const savePreset = () => {
  const currentSlot = document.querySelector('[gbs-role="slot"][active]');

  if (!currentSlot) {
    return;
  }

  const key = currentSlot.getAttribute("gbs-element-ref");
  const currentIndex = currentSlot.getAttribute("gbs-slot-id");
  gbsPrompt(
    "Assign a slot name",
    GBSControl.structs.slots[currentIndex].name || key
  )
    .then((currentName: string) => {
      if (currentName && currentName.trim() !== "Empty") {
        currentSlot.setAttribute("gbs-name", currentName);
        fetch(
          `/slot/save?index=${currentIndex}&name=${currentName.substring(
            0,
            24
          )}&${+new Date()}`
        ).then(() => {
          loadUser("4").then(() => {
            setTimeout(() => {
              fetchSlotNames().then((success: boolean) => {
                if (success) {
                  updateSlotNames();
                }
              });
            }, 500);
          });
        });
      }
    })
    .catch(() => {});
};

const removePreset = () => {
    fetch(`/slot/remove?0&nocache=${new Date().getTime()}`).then(() => {
        setTimeout(() => {
            clearTimeout(GBSControl.wsTimeout);
            // GBSControl.wsTimeout = setTimeout(timeOutWs, 6000); //TODO: calc timeout
            fetch(`/slot/remove?1&nocache=${new Date().getTime()}`).then(() => {
                setTimeout(() => {
                    fetchSlotNames().then((success) => {
                        if (success) {
                            updateSlotNames();
                        }
                    });
                }, 500);
            });
        }, 200);
    });
};

const loadPreset = () => {
  loadUser("3").then(() => {
    if (GBSStorage.read("customSlotFilters") === true) {
      setTimeout(() => {
        fetch(`/gbs/restore-filters?${+new Date()}`);
      }, 250);
    }
  });
};

const getSlotsHTML = () => {
  // prettier-ignore
  return [
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9','-','.','_','~','(',')','!','*',':',','
  ].map((chr,idx)=>{

    return `<button
    class="gbs-button gbs-button__slot"
    gbs-slot-id="${idx}"
    gbs-message="${chr}"
    gbs-message-type="setSlot"
    gbs-click="normal"
    gbs-element-ref="slot-${chr}"
    gbs-meta="1024&#xa;x768"
    gbs-role="slot"
    gbs-name="slot-${idx}"
  ></button>`;

  }).join('');
};

const setSlot = (slot: string) => {
  fetch(`/slot/set?slot=${slot}&${+new Date()}`);
};

const updateSlotNames = () => {
  for (let i = 0; i < GBSControl.maxSlots; i++) {
    const el = document.querySelector(`[gbs-slot-id="${i}"]`);

    el.setAttribute("gbs-name", GBSControl.structs.slots[i].name);
    el.setAttribute(
      "gbs-meta",
      getSlotPresetName(parseInt(GBSControl.structs.slots[i].presetID, 10))
    );
  }
};

const fetchSlotNames = () => {
  return fetch(`/bin/slots.bin?${+new Date()}`)
    .then((response) => response.arrayBuffer())
    .then((arrayBuffer: ArrayBuffer) => {
      if (
        arrayBuffer.byteLength ===
        StructParser.getSize(Structs, "slots") * GBSControl.maxSlots
      ) {
        GBSControl.structs = {
          slots: StructParser.parseStructArray(arrayBuffer, Structs, "slots"),
        };
        return true;
      }
      return false;
    });
};

const getSlotPresetName = (presetID: number) => {
  switch (presetID) {
    case 0x01:
    case 0x011:
      return "1280x960";
    case 0x02:
    case 0x012:
      return "1280x1024";
    case 0x03:
    case 0x013:
      return "1280x720";
    case 0x05:
    case 0x015:
      return "1920x1080";
    case 0x06:
    case 0x016:
      return "DOWNSCALE";
    case 0x04:
      return "720x480";
    case 0x14:
      return "768x576";
    case 0x21: // bypass 1
    case 0x22: // bypass 2
      return "BYPASS";
    default:
      return "CUSTOM";
  }
};

const fetchSlotNamesErrorRetry = () => {
  setTimeout(fetchSlotNamesAndInit, 1000);
};

const fetchSlotNamesAndInit = () => {
  fetchSlotNames()
    .then((success) => {
      if (!success) {
        fetchSlotNamesErrorRetry();
        return;
      }
      initUIElements();
      wifiGetStatus().then(() => {
        initUI();
        updateSlotNames();
        createWebSocket();
        createIntervalChecks();
        setTimeout(hideLoading, 1000);
      });
    }, fetchSlotNamesErrorRetry)
    .catch(fetchSlotNamesErrorRetry);
};

/** Promises */
const serial = (funcs: (() => Promise<any>)[]) =>
  funcs.reduce(
    (promise, func) =>
      promise.then((result) =>
        func().then(Array.prototype.concat.bind(result))
      ),
    Promise.resolve([])
  );

/** helpers */

const toggleHelp = () => {
  let help = GBSStorage.read("help") || false;

  GBSStorage.write("help", !help);
  updateHelp(!help);
};

const toggleDeveloperMode = () => {
  const developerMode = GBSStorage.read("developerMode") || false;

  GBSStorage.write("developerMode", !developerMode);
  updateDeveloperMode(!developerMode);
};

const toggleCustomSlotFilters = () => {
  const customSlotFilters = GBSStorage.read("customSlotFilters");
  GBSStorage.write("customSlotFilters", !customSlotFilters);
  updateCustomSlotFilters(!customSlotFilters);
};

const updateHelp = (help: boolean) => {
  if (help) {
    document.body.classList.remove("gbs-help-hide");
  } else {
    document.body.classList.add("gbs-help-hide");
  }
};

const updateDeveloperMode = (developerMode: boolean) => {
  const el = document.querySelector('[gbs-section="developer"]') as HTMLElement;
  if (developerMode) {
    el.removeAttribute("hidden");
    GBSControl.ui.developerSwitch.setAttribute("active", "");
    document.body.classList.remove("gbs-output-hide");
  } else {
    el.setAttribute("hidden", "");
    GBSControl.ui.developerSwitch.removeAttribute("active");
    document.body.classList.add("gbs-output-hide");
  }

  GBSControl.ui.developerSwitch.querySelector(
    ".gbs-icon"
  ).innerText = developerMode ? "toggle_on" : "toggle_off";
};

const updateCustomSlotFilters = (
  customFilters: boolean = GBSStorage.read("customSlotFilters") === true
) => {
  if (customFilters) {
    GBSControl.ui.customSlotFilters.setAttribute("active", "");
  } else {
    GBSControl.ui.customSlotFilters.removeAttribute("active");
  }

  GBSControl.ui.customSlotFilters.querySelector(
    ".gbs-icon"
  ).innerText = customFilters ? "toggle_on" : "toggle_off";
};

const GBSStorage = {
  lsObject: {},
  write(key: string, value: any) {
    GBSStorage.lsObject = GBSStorage.lsObject || {};
    GBSStorage.lsObject[key] = value;
    localStorage.setItem(
      "GBSControlSlotNames",
      JSON.stringify(GBSStorage.lsObject)
    );
  },
  read(key: string): string | number | boolean {
    GBSStorage.lsObject = JSON.parse(
      localStorage.getItem("GBSControlSlotNames") || "{}"
    );
    return GBSStorage.lsObject[key];
  },
};

const nodelistToArray = <Element>(
  nodelist:
    | HTMLCollectionOf<globalThis.Element>
    | NodeListOf<globalThis.Element>
): Element[] => {
  return Array.prototype.slice.call(nodelist);
};

const toggleButtonActive = (id: string) => (
  button: HTMLElement,
  _index: any,
  _array: any
) => {
  button.removeAttribute("active");

  if (button.getAttribute("gbs-element-ref") === id) {
    button.setAttribute("active", "");
  }
};

const displayWifiWarning = (mode: boolean) => {
  GBSControl.ui.webSocketConnectionWarning.style.display = mode
    ? "block"
    : "none";
};

const updateTerminal = () => {
  if (GBSControl.queuedText.length > 0) {
    requestAnimationFrame(() => {
      GBSControl.ui.terminal.value += GBSControl.queuedText;
      GBSControl.ui.terminal.scrollTop = GBSControl.ui.terminal.scrollHeight;
      GBSControl.queuedText = "";
    });
  }
};

const updateViewPort = () => {
  document.documentElement.style.setProperty(
    "--viewport-height",
    window.innerHeight + "px"
  );
};

const hideLoading = () => {
  GBSControl.ui.loader.setAttribute("style", "display:none");
};

const checkFetchResponseStatus = (response: Response) => {
  if (!response.ok) {
    throw new Error(`HTTP ${response.status} - ${response.statusText}`);
  }
  return response;
};

const readLocalFile = (file: File) => {
  const reader = new FileReader();
  reader.addEventListener("load", (event) => {
    doRestore(reader.result as ArrayBuffer);
  });
  reader.readAsArrayBuffer(file);
};

/** backup / restore */

const doBackup = () => {
  let backupFiles: string[];
  let done = 0;
  let total = 0;
  fetch("/spiffs/dir")
    .then((r) => r.json())
    .then((files: string[]) => {
      backupFiles = files;
      total = files.length;
      const funcs = files.map((path: string) => () => {
        return fetch(`/spiffs/download?file=${path}&${+new Date()}`).then(
          (response) => {
            GBSControl.ui.progressBackup.setAttribute(
              "gbs-progress",
              `${done}/${total}`
            );
            done++;
            return checkFetchResponseStatus(response) && response.arrayBuffer();
          }
        );
      });

      return serial(funcs);
    })
    .then((files: ArrayBuffer[]) => {
      const headerDescriptor = files.reduce((acc, f, index) => {
        acc[backupFiles[index]] = f.byteLength;
        return acc;
      }, {});

      const backupFilesJSON = JSON.stringify(headerDescriptor);
      const backupFilesJSONSize = backupFilesJSON.length;

      const mainHeader = [
        (backupFilesJSONSize >> 24) & 255, // size
        (backupFilesJSONSize >> 16) & 255, // size
        (backupFilesJSONSize >> 8) & 255, // size
        (backupFilesJSONSize >> 0) & 255,
      ];

      const outputArray: number[] = [
        ...mainHeader,
        ...backupFilesJSON.split("").map((c) => c.charCodeAt(0)),
        ...files.reduce((acc, f, index) => {
          acc = acc.concat(Array.from(new Uint8Array(f)));
          return acc;
        }, []),
      ];

      downloadBlob(
        new Blob([new Uint8Array(outputArray)]),
        `gbs-control.backup-${+new Date()}.bin`
      );
      GBSControl.ui.progressBackup.setAttribute("gbs-progress", ``);
    });
};

const doRestore = (file: ArrayBuffer) => {
  const { backupInput } = GBSControl.ui;
  const fileBuffer = new Uint8Array(file);
  const headerCheck = fileBuffer.slice(4, 6);

  if (headerCheck[0] !== 0x7b || headerCheck[1] !== 0x22) {
    backupInput.setAttribute("disabled", "");
    gbsAlert("Invalid Backup File")
      .then(
        () => {
          backupInput.removeAttribute("disabled");
        },
        () => {
          backupInput.removeAttribute("disabled");
        }
      )
      .catch(() => {
        backupInput.removeAttribute("disabled");
      });
    return;
  }
  const b0 = fileBuffer[0],
    b1 = fileBuffer[1],
    b2 = fileBuffer[2],
    b3 = fileBuffer[3];
  const headerSize = (b0 << 24) + (b1 << 16) + (b2 << 8) + b3;
  const headerString = Array.from(fileBuffer.slice(4, headerSize + 4))
    .map((c) => String.fromCharCode(c))
    .join("");

  const headerObject = JSON.parse(headerString);
  const files = Object.keys(headerObject);
  let pos = headerSize + 4;
  let total = files.length;
  let done = 0;
  const funcs = files.map((fileName) => () => {
    const fileContents = fileBuffer.slice(pos, pos + headerObject[fileName]);
    const formData = new FormData();
    formData.append(
      "file",
      new Blob([fileContents], { type: "application/octet-stream" }),
      fileName.substr(1)
    );

    return fetch("/spiffs/upload", {
      method: "POST",
      body: formData,
    }).then((response) => {
      GBSControl.ui.progressRestore.setAttribute(
        "gbs-progress",
        `${done}/${total}`
      );
      done++;
      pos += headerObject[fileName];
      return response;
    });
  });

  serial(funcs).then(() => {
    GBSControl.ui.progressRestore.setAttribute("gbs-progress", ``);
    loadUser("a").then(() => {
      gbsAlert(
        "Restarting GBSControl.\nPlease wait until wifi reconnects then click OK"
      )
        .then(() => {
          window.location.reload();
        })
        .catch(() => {});
    });
  });
};

const downloadBlob = (blob: Blob, name = "file.txt") => {
  // Convert your blob into a Blob URL (a special url that points to an object in the browser's memory)
  const blobUrl = URL.createObjectURL(blob);

  // Create a link element
  const link = document.createElement("a");

  // Set link's href to point to the Blob URL
  link.href = blobUrl;
  link.download = name;

  // Append link to the body
  document.body.appendChild(link);

  // Dispatch click event on the link
  // This is necessary as link.click() does not work on the latest firefox
  link.dispatchEvent(
    new MouseEvent("click", {
      bubbles: true,
      cancelable: true,
      view: window,
    })
  );

  // Remove link from body
  document.body.removeChild(link);
};

/** WIFI management */
const wifiGetStatus = () => {
  return fetch(`/wifi/status?${+new Date()}`)
    .then((r) => r.json())
    .then((wifiStatus: { mode: string; ssid: string }) => {
      GBSControl.wifi = wifiStatus;
      if (GBSControl.wifi.mode === "ap") {
        GBSControl.ui.wifiApButton.setAttribute("active", "");
        GBSControl.ui.wifiApButton.classList.add("gbs-button__secondary");
        GBSControl.ui.wifiStaButton.removeAttribute("active", "");
        GBSControl.ui.wifiStaButton.classList.remove("gbs-button__secondary");
        GBSControl.ui.wifiStaSSID.innerHTML = "STA | Scan Network";
      } else {
        GBSControl.ui.wifiApButton.removeAttribute("active", "");
        GBSControl.ui.wifiApButton.classList.remove("gbs-button__secondary");
        GBSControl.ui.wifiStaButton.setAttribute("active", "");
        GBSControl.ui.wifiStaButton.classList.add("gbs-button__secondary");
        GBSControl.ui.wifiStaSSID.innerHTML = `${GBSControl.wifi.ssid}`;
      }
    });
};

const wifiConnect = () => {
  const ssid = GBSControl.ui.wifiSSDInput.value;
  const password = GBSControl.ui.wifiPasswordInput.value;

  if (!password.length) {
    GBSControl.ui.wifiPasswordInput.classList.add("gbs-wifi__input--error");
    return;
  }

  const formData = new FormData();
  formData.append("n", ssid);
  formData.append("p", password);

  fetch("/wifi/connect", {
    method: "POST",
    body: formData,
  }).then(() => {
    gbsAlert(
      `GBSControl will restart and will connect to ${ssid}. Please wait some seconds then press OK`
    )
      .then(() => {
        window.location.href = "http://gbscontrol.local/";
      })
      .catch(() => {});
  });
};

const wifiScanSSID = () => {
  GBSControl.ui.wifiStaButton.setAttribute("disabled", "");
  GBSControl.ui.wifiListTable.innerHTML = "";

  if (!GBSControl.scanSSIDDone) {
    fetch(`/wifi/list?${+new Date()}`).then(() => {
      GBSControl.scanSSIDDone = true;
      setTimeout(wifiScanSSID, 3000);
    });
    return;
  }

  fetch(`/wifi/list?${+new Date()}`)
    .then((e) => e.text())
    .then((result) => {
      GBSControl.scanSSIDDone = false;
      return result.length
        ? result
            .split("\n")
            .map((line) => line.split(","))
            .map(([strength, encripted, ssid]) => {
              return { strength, encripted, ssid };
            })
        : [];
    })
    .then((ssids) => {
      return ssids.reduce((acc, ssid) => {
        return `${acc}<tr gbs-ssid="${ssid.ssid}">
        <td class="gbs-icon" style="opacity:${
          parseInt(ssid.strength, 10) / 100
        }">wifi</td>
        <td>${ssid.ssid}</td>
        <td class="gbs-icon">${ssid.encripted ? "lock" : "lock_open"}</td>
      </tr>`;
      }, "");
    })
    .then((html) => {
      GBSControl.ui.wifiStaButton.removeAttribute("disabled");

      if (html.length) {
        GBSControl.ui.wifiListTable.innerHTML = html;
        GBSControl.ui.wifiList.removeAttribute("hidden");
        GBSControl.ui.wifiConnect.setAttribute("hidden", "");
      }
    });
};

const wifiSelectSSID = (event: Event) => {
  (GBSControl.ui
    .wifiSSDInput as HTMLInputElement).value = (event.target as HTMLElement).parentElement.getAttribute(
    "gbs-ssid"
  );
  GBSControl.ui.wifiPasswordInput.classList.remove("gbs-wifi__input--error");
  GBSControl.ui.wifiList.setAttribute("hidden", "");
  GBSControl.ui.wifiConnect.removeAttribute("hidden");
};

const wifiSetAPMode = () => {
  if (GBSControl.wifi.mode === "ap") {
    return;
  }

  const formData = new FormData();
  formData.append("n", "dummy");

  fetch("/wifi/connect", {
    method: "POST",
    body: formData,
  }).then(() => {
    gbsAlert(
      "Switching to AP mode. Please connect to gbscontrol SSID and then click OK"
    )
      .then(() => {
        window.location.href = "http://192.168.4.1";
      })
      .catch(() => {});
  });
};

/** button click management */
const controlClick = (control: HTMLButtonElement) => () => {
  const controlKey = control.getAttribute("gbs-control-key");
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
  clearInterval(control["__interval"]);

  const click = controlClick(control);
  click();
  control["__interval"] = setInterval(click, 300);
};

const controlMouseUp = (control: HTMLButtonElement) => () => {
  clearInterval(control["__interval"]);
};

/** inits */
const initMenuButtons = () => {
  const menuButtons = nodelistToArray<HTMLButtonElement>(
    document.querySelector(".gbs-menu").querySelectorAll("button")
  );
  const sections = nodelistToArray<HTMLElement>(
    document.querySelectorAll("section")
  );
  const scroll = document.querySelector(".gbs-scroll");

  menuButtons.forEach((button) =>
    button.addEventListener("click", () => {
      const section = button.getAttribute("gbs-section");

      sections.forEach((section) => section.setAttribute("hidden", ""));
      document
        .querySelector(`section[name="${section}"]`)
        .removeAttribute("hidden");

      menuButtons.forEach((btn) => btn.removeAttribute("active"));
      button.setAttribute("active", "");
      scroll.scrollTo(0, 1);
    })
  );
};

const initGBSButtons = () => {
  const actions = {
    user: loadUser,
    action: loadDoc,
    setSlot,
  };

  const buttons = nodelistToArray<HTMLElement>(
    document.querySelectorAll("[gbs-click]")
  );

  buttons.forEach((button) => {
    const clickMode = button.getAttribute("gbs-click");
    const message = button.getAttribute("gbs-message");
    const messageType = button.getAttribute("gbs-message-type");
    const action = actions[messageType];

    if (clickMode === "normal") {
      button.addEventListener("click", () => {
        action(message);
      });
    }

    if (clickMode === "repeat") {
      const callback = () => {
        action(message);
      };

      button.addEventListener(
        !("ontouchstart" in window) ? "mousedown" : "touchstart",
        () => {
          callback();
          clearInterval(button["__interval"]);
          button["__interval"] = setInterval(callback, 300);
        }
      );
      button.addEventListener(
        !("ontouchstart" in window) ? "mouseup" : "touchend",
        () => {
          clearInterval(button["__interval"]);
        }
      );
    }
  });
};

const initClearButton = () => {
  GBSControl.ui.outputClear.addEventListener("click", () => {
    GBSControl.ui.terminal.value = "";
  });
};

const initControlMobileKeys = () => {
  const controls = document.querySelectorAll("[gbs-control-target]");
  const controlsKeys = document.querySelectorAll("[gbs-control-key]");

  controls.forEach((control) => {
    control.addEventListener("click", () => {
      GBSControl.controlKeysMobileMode = control.getAttribute(
        "gbs-control-target"
      );
      controls.forEach((crtl) => {
        crtl.removeAttribute("active");
      });
      control.setAttribute("active", "");
    });
  });

  controlsKeys.forEach((control) => {
    control.addEventListener(
      !("ontouchstart" in window) ? "mousedown" : "touchstart",
      controlMouseDown(control as HTMLButtonElement)
    );
    control.addEventListener(
      !("ontouchstart" in window) ? "mouseup" : "touchend",
      controlMouseUp(control as HTMLButtonElement)
    );
  });
};

const initLegendHelpers = () => {
  nodelistToArray<HTMLElement>(
    document.querySelectorAll(".gbs-fieldset__legend--help")
  ).forEach((e) => {
    e.addEventListener("click", toggleHelp);
  });
};

const initUnloadListener = () => {
  window.addEventListener("unload", () => {
    clearInterval(GBSControl.wsCheckTimer);
    if (GBSControl.ws) {
      if (GBSControl.ws.readyState == 0 || GBSControl.ws.readyState == 1) {
        GBSControl.ws.close();
      }
    }
  });
};

const initSlotButtons = () => {
  GBSControl.ui.slotContainer.innerHTML = getSlotsHTML();
  GBSControl.ui.slotButtonList = nodelistToArray(
    document.querySelectorAll('[gbs-role="slot"]')
  ) as HTMLElement[];
};

const initUIElements = () => {
  GBSControl.ui = {
    terminal: document.getElementById("outputTextArea"),
    webSocketConnectionWarning: document.getElementById("websocketWarning"),
    presetButtonList: nodelistToArray(
      document.querySelectorAll("[gbs-role='preset']")
    ) as HTMLElement[],
    slotButtonList: nodelistToArray(
      document.querySelectorAll('[gbs-role="slot"]')
    ) as HTMLElement[],
    toggleList: document.querySelectorAll("[gbs-toggle]"),
    toggleSwichList: document.querySelectorAll("[gbs-toggle-switch]"),
    wifiList: document.querySelector("[gbs-wifi-list]"),
    wifiListTable: document.querySelector(".gbs-wifi__list"),
    wifiConnect: document.querySelector(".gsb-wifi__connect"),
    wifiConnectButton: document.querySelector("[gbs-wifi-connect-button]"),
    wifiSSDInput: document.querySelector('[gbs-input="ssid"]'),
    wifiPasswordInput: document.querySelector('[gbs-input="password"]'),
    wifiApButton: document.querySelector("[gbs-wifi-ap]"),
    wifiStaButton: document.querySelector("[gbs-wifi-station]"),
    wifiStaSSID: document.querySelector("[gbs-wifi-station-ssid]"),
    loader: document.querySelector(".gbs-loader"),
    progressBackup: document.querySelector("[gbs-progress-backup]"),
    progressRestore: document.querySelector("[gbs-progress-restore]"),
    outputClear: document.querySelector("[gbs-output-clear]"),
    slotContainer: document.querySelector("[gbs-slot-html]"),
    backupButton: document.querySelector(".gbs-backup-button"),
    backupInput: document.querySelector(".gbs-backup-input"),
    developerSwitch: document.querySelector("[gbs-dev-switch]"),
    customSlotFilters: document.querySelector("[gbs-slot-custom-filters]"),
    alert: document.querySelector('section[name="alert"]'),
    alertOk: document.querySelector("[gbs-alert-ok]"),
    alertContent: document.querySelector("[gbs-alert-content]"),
    prompt: document.querySelector('section[name="prompt"]'),
    promptOk: document.querySelector("[gbs-prompt-ok]"),
    promptCancel: document.querySelector("[gbs-prompt-cancel]"),
    promptContent: document.querySelector("[gbs-prompt-content]"),
    promptInput: document.querySelector('[gbs-input="prompt-input"]'),
  };
};

const initGeneralListeners = () => {
  window.addEventListener("resize", () => {
    updateViewPort();
  });

  GBSControl.ui.backupInput.addEventListener("change", (event) => {
    const fileList: FileList = event.target["files"];
    readLocalFile(fileList[0]);
    GBSControl.ui.backupInput.value = "";
  });

  GBSControl.ui.backupButton.addEventListener("click", doBackup);
  GBSControl.ui.wifiListTable.addEventListener("click", wifiSelectSSID);
  GBSControl.ui.wifiConnectButton.addEventListener("click", wifiConnect);
  GBSControl.ui.wifiApButton.addEventListener("click", wifiSetAPMode);
  GBSControl.ui.wifiStaButton.addEventListener("click", wifiScanSSID);
  GBSControl.ui.developerSwitch.addEventListener("click", toggleDeveloperMode);
  GBSControl.ui.customSlotFilters.addEventListener(
    "click",
    toggleCustomSlotFilters
  );

  GBSControl.ui.alertOk.addEventListener("click", () => {
    GBSControl.ui.alert.setAttribute("hidden", "");
    gbsAlertPromise.resolve();
  });

  GBSControl.ui.promptOk.addEventListener("click", () => {
    GBSControl.ui.prompt.setAttribute("hidden", "");
    const value = GBSControl.ui.promptInput.value;
    if (value !== undefined || value.length > 0) {
      gbsPromptPromise.resolve(GBSControl.ui.promptInput.value);
    } else {
      gbsPromptPromise.reject();
    }
  });

  GBSControl.ui.promptCancel.addEventListener("click", () => {
    GBSControl.ui.prompt.setAttribute("hidden", "");
    gbsPromptPromise.reject();
  });

  GBSControl.ui.promptInput.addEventListener("keydown", (event: any) => {
    if (event.keyCode === 13) {
      GBSControl.ui.prompt.setAttribute("hidden", "");
      const value = GBSControl.ui.promptInput.value;
      if (value !== undefined || value.length > 0) {
        gbsPromptPromise.resolve(GBSControl.ui.promptInput.value);
      } else {
        gbsPromptPromise.reject();
      }
    }
    if (event.keyCode === 27) {
      gbsPromptPromise.reject();
    }
  });
};

const initDeveloperMode = () => {
  const devMode = GBSStorage.read("developerMode") as boolean;
  if (devMode === undefined) {
    GBSStorage.write("developerMode", false);
    updateDeveloperMode(false);
  } else {
    updateDeveloperMode(devMode);
  }
};

const initHelp = () => {
  let help = GBSStorage.read("help") as boolean;
  if (help === undefined) {
    help = false;
    GBSStorage.write("help", help);
  }
  updateHelp(help);
};

const gbsAlertPromise = {
  resolve: null,
  reject: null,
};

const alertKeyListener = (event: any) => {
  if (event.keyCode === 13) {
    gbsAlertPromise.resolve();
  }
  if (event.keyCode === 27) {
    gbsAlertPromise.reject();
  }
};

const gbsAlert = (text: string) => {
  GBSControl.ui.alertContent.textContent = text;
  GBSControl.ui.alert.removeAttribute("hidden");
  document.addEventListener("keyup", alertKeyListener);
  return new Promise((resolve, reject) => {
    gbsAlertPromise.resolve = (e) => {
      document.removeEventListener("keyup", alertKeyListener);
      GBSControl.ui.alert.setAttribute("hidden", "");
      return resolve(e);
    };
    gbsAlertPromise.reject = () => {
      document.removeEventListener("keyup", alertKeyListener);
      GBSControl.ui.alert.setAttribute("hidden", "");
      return reject();
    };
  });
};

const gbsPromptPromise = {
  resolve: null,
  reject: null,
};

const gbsPrompt = (text: string, defaultValue = "") => {
  GBSControl.ui.promptContent.textContent = text;
  GBSControl.ui.prompt.removeAttribute("hidden");
  GBSControl.ui.promptInput.value = defaultValue;

  return new Promise<string>((resolve, reject) => {
    gbsPromptPromise.resolve = resolve;
    gbsPromptPromise.reject = reject;
    GBSControl.ui.promptInput.focus();
  });
};

const initUI = () => {
  updateCustomSlotFilters();
  initGeneralListeners();
  updateViewPort();
  initSlotButtons();
  initLegendHelpers();
  initMenuButtons();
  initGBSButtons();
  initClearButton();
  initControlMobileKeys();
  initUnloadListener();
  initDeveloperMode();
  initHelp();
};

const main = () => {
  const ip = location.hostname;
  GBSControl.serverIP = ip;
  GBSControl.webSocketServerUrl = `ws://${ip}:81/`;
  document
    .querySelector(".gbs-loader img")
    .setAttribute(
      "src",
      document.head
        .querySelector('[rel="apple-touch-icon"]')
        .getAttribute("href")
    );
  fetchSlotNamesAndInit();
};

main();
