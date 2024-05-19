/*
* DEVELOPER's MEMO:
*   1. WebUI icons: https://fonts.google.com/icons
*
*/
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
/**
 * Must be aligned with slots.h -> SlotMeta structure
 *
 * @type {StructDescriptors}
 */
const Structs = {
    slots: [
        { name: "name", type: "string", size: 25 },
        { name: "slot", type: "byte", size: 1 },
        { name: "resolutionID", type: "string", size: 1 },
        { name: "scanlines", type: "byte", size: 1 },
        { name: "scanlinesStrength", type: "byte", size: 1 },
        { name: "wantVdsLineFilter", type: "byte", size: 1 },
        { name: "wantStepResponse", type: "byte", size: 1 },
        { name: "wantPeaking", type: "byte", size: 1 },
    ],
};
/**
 * Description placeholder
 *
 * @type {{ pos: number; parseStructArray(buff: ArrayBuffer, structsDescriptors: StructDescriptors, struct: string): any; getValue(buff: {}, structItem: { type: "string" | "byte"; size: number; }): any; getSize(structsDescriptors: StructDescriptors, struct: string): any; }}
 */
const StructParser = {
    pos: 0,
    parseStructArray(buff, structsDescriptors, struct) {
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
    getValue(buff, structItem) {
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
    getSize(structsDescriptors, struct) {
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
        'a': "n/a",
        'c': "button1280x960",
        'd': "button1280x960",
        'e': "button1280x1024",
        'f': "button1280x1024",
        'g': "button1280x720",
        'h': "button1280x720",
        'i': "button720x480",
        'j': "button720x480",
        'k': "button1920x1080",
        'l': "button1920x1080",
        'm': "button15kHzScaleDown",
        'n': "button15kHzScaleDown",
        'o': "button768×576",
        'p': "button768×576",
        'q': "buttonSourcePassThrough",
        's': "buttonSourcePassThrough",
        'u': "buttonSourcePassThrough",
        'w': "buttonLoadCustomPreset",
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
    activeResolution: "",
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
        toggleConsole: null,
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
/**
 * Description placeholder
 */
const timeOutWs = () => {
    console.log("timeOutWs");
    if (GBSControl.ws) {
        GBSControl.ws.close();
    }
    GBSControl.isWsActive = false;
    displayWifiWarning(true);
};
/**
 * Description placeholder
 */
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
    GBSControl.ws.onmessage = (message) => {
        clearTimeout(GBSControl.wsTimeout);
        GBSControl.wsTimeout = setTimeout(timeOutWs, 2700);
        GBSControl.isWsActive = true;
        const [messageDataAt0, // always #
        messageDataAt1, // selected resolution ! (hex)
        messageDataAt2, // selected slot ID (char)
        messageDataAt3, // adcAutoGain & scanlines & vdsLineFilter & wantPeaking & PalForce60 & wantOutputComponent (binary)
        messageDataAt4, // matchPresetSource & enableFrameTimeLock & deintMode & wantTap6 & wantStepResponse & wantFullHeight (binary)
        messageDataAt5, // enableCalibrationADC & preferScalingRgbhv & disableExternalClockGenerator (binary)
        ] = message.data;
        console.log(message);
        if (messageDataAt0 != "#") {
            GBSControl.queuedText += message.data;
            GBSControl.dataQueued += message.data.length;
            if (GBSControl.dataQueued >= 70000) {
                GBSControl.ui.terminal.value = "";
                GBSControl.dataQueued = 0;
            }
        }
        else {
            console.log("buttonMapping: " + messageDataAt1);
            // ! curent/selected resolution
            const resID = GBSControl.buttonMapping[messageDataAt1];
            const resEl = document.querySelector(`[gbs-element-ref="${resID}"]`);
            const activePresetButton = resEl
                ? resEl.getAttribute("gbs-element-ref")
                : "none";
            GBSControl.ui.presetButtonList.forEach(toggleButtonActive(activePresetButton));
            // ! current/selected preset slot
            const slotId = "slot-" + messageDataAt2;
            const activeSlotButton = document.querySelector(`[gbs-element-ref="${slotId}"]`);
            if (activeSlotButton) {
                GBSControl.ui.slotButtonList.forEach(toggleButtonActive(slotId));
            }
            if (messageDataAt3 && messageDataAt4 && messageDataAt5) {
                const optionByte0 = messageDataAt3.charCodeAt(0);
                const optionByte1 = messageDataAt4.charCodeAt(0);
                const optionByte2 = messageDataAt5.charCodeAt(0);
                const optionButtonList = [
                    ...nodelistToArray(GBSControl.ui.toggleList),
                    ...nodelistToArray(GBSControl.ui.toggleSwichList),
                ];
                const toggleMethod = (button, mode) => {
                    if (button.tagName === "TD") {
                        button.innerText = mode ? "toggle_on" : "toggle_off";
                    }
                    button = button.tagName !== "TD" ? button : button.parentElement;
                    if (mode) {
                        button.setAttribute("active", "");
                    }
                    else {
                        button.removeAttribute("active");
                    }
                };
                optionButtonList.forEach((button) => {
                    const toggleData = button.getAttribute("gbs-toggle") ||
                        button.getAttribute("gbs-toggle-switch");
                    if (toggleData !== null) {
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
                    }
                });
            }
        }
    };
};
/**
 * Description placeholder
 *
 * @returns {boolean}
 */
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
        }
        else {
            return true;
        }
    }
    else if (GBSControl.ws.readyState == 0) {
        GBSControl.wsNoSuccessConnectingCounter++;
        if (GBSControl.wsNoSuccessConnectingCounter >= 14) {
            console.log("ws still connecting, retry");
            GBSControl.ws.close();
            GBSControl.wsNoSuccessConnectingCounter = 0;
        }
        return true;
    }
    else {
        return true;
    }
};
/**
 * Description placeholder
 */
const createIntervalChecks = () => {
    GBSControl.wsCheckTimer = setInterval(checkWebSocketServer, 500);
    GBSControl.updateTerminalTimer = setInterval(updateTerminal, 50);
};
/* API services */
/**
 * Description placeholder
 *
 * @param {string} link
 * @returns {*}
 */
const loadDoc = (link) => {
    return fetch(`http://${GBSControl.serverIP}/sc?${link}&nocache=${new Date().getTime()}`);
};
/**
 * Description placeholder
 *
 * @param {string} link
 * @returns {*}
 */
const loadUser = (link) => {
    if (link == "a" || link == "1") {
        GBSControl.isWsActive = false;
        GBSControl.ui.terminal.value += "\nRestart\n";
        GBSControl.ui.terminal.scrollTop = GBSControl.ui.terminal.scrollHeight;
    }
    return fetch(`http://${GBSControl.serverIP}/uc?${link}&nocache=${new Date().getTime()}`);
};
/** SLOT management */
const removePreset = () => {
    const currentSlot = document.querySelector('[gbs-role="slot"][active]');
    if (!currentSlot) {
        return;
    }
    const currentIndex = currentSlot.getAttribute("gbs-slot-id");
    const currentName = currentSlot.getAttribute("gbs-name");
    if (currentName && currentName.trim() !== "Empty") {
        fetch(`http://${GBSControl.serverIP}/slot/remove?index=${currentIndex}&${+new Date()}`).then(() => {
        });
    }
    ;
};
/**
 *
 * @returns
 */
const savePreset = () => {
    const currentSlot = document.querySelector('[gbs-role="slot"][active]');
    if (!currentSlot) {
        return;
    }
    const key = currentSlot.getAttribute("gbs-element-ref");
    const currentIndex = currentSlot.getAttribute("gbs-slot-id");
    gbsPrompt("Assign a slot name", GBSControl.structs.slots[currentIndex].name || key)
        .then((currentName) => {
        if (currentName && currentName.trim() !== "Empty") {
            currentSlot.setAttribute("gbs-name", currentName);
            fetch(`http://${GBSControl.serverIP}/slot/save?index=${currentIndex}&name=${currentName.substring(0, 24)}&${+new Date()}`).then(() => {
                loadUser("4").then(() => {
                    setTimeout(() => {
                        fetchSlotNames().then((success) => {
                            if (success) {
                                updateSlotNames();
                            }
                        });
                    }, 500);
                });
            });
        }
    })
        .catch(() => { });
};
/**
 * Description placeholder
 */
const loadPreset = () => {
    loadUser("3").then(() => {
        if (GBSStorage.read("customSlotFilters") === true) {
            setTimeout(() => {
                fetch(`http://${GBSControl.serverIP}/gbs/restore-filters?${+new Date()}`);
            }, 250);
        }
    });
};
/**
 * Description placeholder
 *
 * @returns {*}
 */
const getSlotsHTML = () => {
    // prettier-ignore
    return [
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '.', '_', '~', '(', ')', '!', '*', ':', ','
    ].map((chr, idx) => {
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
/**
 * Description placeholder
 *
 * @param {string} slot
 */
const setSlot = (slot) => {
    fetch(`http://${GBSControl.serverIP}/slot/set?slot=${slot}&${+new Date()}`);
};
/**
 * Description placeholder
 */
const updateSlotNames = () => {
    for (let i = 0; i < GBSControl.maxSlots; i++) {
        const el = document.querySelector(`[gbs-slot-id="${i}"]`);
        el.setAttribute("gbs-name", GBSControl.structs.slots[i].name);
        el.setAttribute("gbs-meta", getSlotPresetName(GBSControl.structs.slots[i].resolutionID));
    }
};
/**
 * Description placeholder
 *
 * @returns {*}
 */
const fetchSlotNames = () => {
    return fetch(`http://${GBSControl.serverIP}/bin/slots.bin?${+new Date()}`)
        .then((response) => response.arrayBuffer())
        .then((arrayBuffer) => {
        if (arrayBuffer.byteLength ===
            StructParser.getSize(Structs, "slots") * GBSControl.maxSlots) {
            GBSControl.structs = {
                slots: StructParser.parseStructArray(arrayBuffer, Structs, "slots"),
            };
            return true;
        }
        return false;
    });
};
/**
 * Must be aligned with options.h -> OutputResolution
 *
 * @param {string} resolutionID
 * @returns {("1280x960" | "1280x1024" | "1280x720" | "1920x1080" | "DOWNSCALE" | "720x480" | "768x576" | "BYPASS" | "CUSTOM")}
 */
const getSlotPresetName = (resolutionID) => {
    switch (resolutionID) {
        case 'c':
        case 'd':
            // case 0x011:
            return "1280x960";
        case 'e':
        case 'f':
            // case 0x012:
            return "1280x1024";
        case 'g':
        case 'h':
            // case 0x013:
            return "1280x720";
        case 'i':
        case 'j':
            // case 0x015:
            return "720x480";
        case 'k':
        case 'l':
            return "1920x1080";
        case 'm':
        case 'n':
            // case 0x016:
            return "DOWNSCALE";
        case 'o':
        case 'p':
            return "768x576";
        case 'q':
            return "BYPASS";
        case 's': // bypass 1
            return "HD_BYPASS";
        case 'u': // bypass 2
            return "BYPASS_RGBHV";
        // case 12:
        //     return "CUSTOM";
        default:
            return "NONE";
    }
};
/**
 * Description placeholder
 */
const fetchSlotNamesErrorRetry = () => {
    setTimeout(fetchSlotNamesAndInit, 1000);
};
/**
 * Description placeholder
 */
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
const serial = (funcs) => funcs.reduce((promise, func) => promise.then((result) => func().then(Array.prototype.concat.bind(result))), Promise.resolve([]));
/** helpers */
const toggleHelp = () => {
    let help = GBSStorage.read("help") || false;
    GBSStorage.write("help", !help);
    updateHelp(!help);
};
/**
 * Description placeholder
 */
const toggleDeveloperMode = () => {
    const developerMode = GBSStorage.read("developerMode") || false;
    GBSStorage.write("developerMode", !developerMode);
    updateDeveloperMode(!developerMode);
};
/**
 * Description placeholder
 */
const toggleCustomSlotFilters = () => {
    const customSlotFilters = GBSStorage.read("customSlotFilters");
    GBSStorage.write("customSlotFilters", !customSlotFilters);
    updateCustomSlotFilters(!customSlotFilters);
};
/**
 * Description placeholder
 *
 * @param {boolean} help
 */
const updateHelp = (help) => {
    if (help) {
        document.body.classList.remove("gbs-help-hide");
    }
    else {
        document.body.classList.add("gbs-help-hide");
    }
};
/**
 * Toggle console visibility (see corresponding button)
 *
 * @param {boolean} developerMode
 */
const updateConsoleVisibility = (developerMode) => {
    // const developerMode = GBSStorage.read("developerMode") as boolean;
    if (developerMode) {
        const consoleStatus = GBSStorage.read("consoleVisible");
        if (consoleStatus !== true) {
            GBSStorage.write("consoleVisible", true);
            GBSControl.ui.toggleConsole.setAttribute("active", "");
            document.body.classList.remove("gbs-output-hide");
        }
        else {
            GBSStorage.write("consoleVisible", false);
            GBSControl.ui.toggleConsole.removeAttribute("active");
            document.body.classList.add("gbs-output-hide");
        }
    }
};
/**
 * Description placeholder
 *
 * @param {boolean} developerMode
 */
const updateDeveloperMode = (developerMode) => {
    const el = document.querySelector('[gbs-section="developer"]');
    if (developerMode) {
        GBSStorage.write("consoleVisible", true);
        el.removeAttribute("hidden");
        GBSControl.ui.developerSwitch.setAttribute("active", "");
        document.body.classList.remove("gbs-output-hide");
    }
    else {
        el.setAttribute("hidden", "");
        GBSControl.ui.developerSwitch.removeAttribute("active");
        document.body.classList.add("gbs-output-hide");
    }
    GBSControl.ui.developerSwitch.querySelector(".gbs-icon").innerText = developerMode ? "toggle_on" : "toggle_off";
};
/**
 * Description placeholder
 *
 * @param {boolean} [customFilters=GBSStorage.read("customSlotFilters") === true]
 */
const updateCustomSlotFilters = (customFilters = GBSStorage.read("customSlotFilters") === true) => {
    if (customFilters) {
        GBSControl.ui.customSlotFilters.setAttribute("active", "");
    }
    else {
        GBSControl.ui.customSlotFilters.removeAttribute("active");
    }
    GBSControl.ui.customSlotFilters.querySelector(".gbs-icon").innerText = customFilters ? "toggle_on" : "toggle_off";
};
/**
 * Description placeholder
 *
 * @type {({ lsObject: {}; write(key: string, value: any): void; read(key: string): string | number | boolean; })}
 */
const GBSStorage = {
    lsObject: {},
    write(key, value) {
        GBSStorage.lsObject = GBSStorage.lsObject || {};
        GBSStorage.lsObject[key] = value;
        localStorage.setItem("GBSControlSlotNames", JSON.stringify(GBSStorage.lsObject));
    },
    read(key) {
        GBSStorage.lsObject = JSON.parse(localStorage.getItem("GBSControlSlotNames") || "{}");
        return GBSStorage.lsObject[key];
    },
};
/**
 * Description placeholder
 *
 * @template Element
 * @param {(| HTMLCollectionOf<globalThis.Element>
 *     | NodeListOf<globalThis.Element>)} nodelist
 * @returns {Element[]}
 */
const nodelistToArray = (nodelist) => {
    return Array.prototype.slice.call(nodelist);
};
/**
 * Description placeholder
 *
 * @param {string} id
 * @returns {(button: HTMLElement, _index: any, _array: any) => void}
 */
const toggleButtonActive = (id) => (button, _index, _array) => {
    button.removeAttribute("active");
    if (button.getAttribute("gbs-element-ref") === id) {
        button.setAttribute("active", "");
    }
};
/**
 * Description placeholder
 *
 * @param {boolean} mode
 */
const displayWifiWarning = (mode) => {
    GBSControl.ui.webSocketConnectionWarning.style.display = mode
        ? "block"
        : "none";
};
/**
 * Description placeholder
 */
const updateTerminal = () => {
    if (GBSControl.queuedText.length > 0) {
        requestAnimationFrame(() => {
            GBSControl.ui.terminal.value += GBSControl.queuedText;
            GBSControl.ui.terminal.scrollTop = GBSControl.ui.terminal.scrollHeight;
            GBSControl.queuedText = "";
        });
    }
};
/**
 * Description placeholder
 */
const updateViewPort = () => {
    document.documentElement.style.setProperty("--viewport-height", window.innerHeight + "px");
};
/**
 * Description placeholder
 */
const hideLoading = () => {
    GBSControl.ui.loader.setAttribute("style", "display:none");
};
/**
 * Description placeholder
 *
 * @param {Response} response
 * @returns {Response}
 */
const checkFetchResponseStatus = (response) => {
    if (!response.ok) {
        throw new Error(`HTTP ${response.status} - ${response.statusText}`);
    }
    return response;
};
/**
 * Description placeholder
 *
 * @param {File} file
 */
const readLocalFile = (file) => {
    const reader = new FileReader();
    reader.addEventListener("load", (event) => {
        doRestore(reader.result);
    });
    reader.readAsArrayBuffer(file);
};
/** backup / restore */
const doBackup = () => {
    let backupFiles;
    let done = 0;
    let total = 0;
    fetch(`http://${GBSControl.serverIP}/data/dir`)
        .then((r) => r.json())
        .then((files) => {
        backupFiles = files;
        total = files.length;
        const funcs = files.map((path) => () => __awaiter(this, void 0, void 0, function* () {
            return yield fetch(`http://${GBSControl.serverIP}/data/download?file=${path}&${+new Date()}`).then((response) => {
                GBSControl.ui.progressBackup.setAttribute("gbs-progress", `${done}/${total}`);
                done++;
                return checkFetchResponseStatus(response) && response.arrayBuffer();
            });
        }));
        return serial(funcs);
    })
        .then((files) => {
        const headerDescriptor = files.reduce((acc, f, index) => {
            acc[backupFiles[index]] = f.byteLength;
            return acc;
        }, {});
        const backupFilesJSON = JSON.stringify(headerDescriptor);
        const backupFilesJSONSize = backupFilesJSON.length;
        const mainHeader = [
            (backupFilesJSONSize >> 24) & 255,
            (backupFilesJSONSize >> 16) & 255,
            (backupFilesJSONSize >> 8) & 255,
            (backupFilesJSONSize >> 0) & 255,
        ];
        const outputArray = [
            ...mainHeader,
            ...backupFilesJSON.split("").map((c) => c.charCodeAt(0)),
            ...files.reduce((acc, f, index) => {
                acc = acc.concat(Array.from(new Uint8Array(f)));
                return acc;
            }, []),
        ];
        downloadBlob(new Blob([new Uint8Array(outputArray)]), `gbs-control.backup-${+new Date()}.bin`);
        GBSControl.ui.progressBackup.setAttribute("gbs-progress", ``);
    });
};
/**
 * Restore SLOTS from backup
 *
 * @param {ArrayBuffer} file
 */
const doRestore = (file) => {
    const { backupInput } = GBSControl.ui;
    const fileBuffer = new Uint8Array(file);
    const headerCheck = fileBuffer.slice(4, 6);
    if (headerCheck[0] !== 0x7b || headerCheck[1] !== 0x22) {
        backupInput.setAttribute("disabled", "");
        gbsAlert("Invalid Backup File")
            .then(() => {
            backupInput.removeAttribute("disabled");
        }, () => {
            backupInput.removeAttribute("disabled");
        })
            .catch(() => {
            backupInput.removeAttribute("disabled");
        });
        return;
    }
    const b0 = fileBuffer[0], b1 = fileBuffer[1], b2 = fileBuffer[2], b3 = fileBuffer[3];
    const headerSize = (b0 << 24) + (b1 << 16) + (b2 << 8) + b3;
    const headerString = Array.from(fileBuffer.slice(4, headerSize + 4))
        .map((c) => String.fromCharCode(c))
        .join("");
    const headerObject = JSON.parse(headerString);
    const files = Object.keys(headerObject);
    let pos = headerSize + 4;
    let total = files.length;
    let done = 0;
    const funcs = files.map((fileName) => () => __awaiter(this, void 0, void 0, function* () {
        const fileContents = fileBuffer.slice(pos, pos + headerObject[fileName]);
        const formData = new FormData();
        formData.append("file", new Blob([fileContents], { type: "application/octet-stream" }), fileName.substr(1));
        return yield fetch(`http://${GBSControl.serverIP}/data/upload`, {
            method: "POST",
            body: formData,
        }).then((response) => {
            GBSControl.ui.progressRestore.setAttribute("gbs-progress", `${done}/${total}`);
            done++;
            pos += headerObject[fileName];
            return response;
        });
    }));
    serial(funcs).then(() => {
        //   serial(funcs).then(() => {
        GBSControl.ui.progressRestore.setAttribute("gbs-progress", ``);
        loadUser("a").then(() => {
            gbsAlert("Restarting GBSControl.\nPlease wait until wifi reconnects then click OK")
                .then(() => {
                window.location.reload();
            })
                .catch(() => { });
        });
    });
};
/**
 * Description placeholder
 *
 * @param {Blob} blob
 * @param {string} [name="file.txt"]
 */
const downloadBlob = (blob, name = "file.txt") => {
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
    link.dispatchEvent(new MouseEvent("click", {
        bubbles: true,
        cancelable: true,
        view: window,
    }));
    // Remove link from body
    document.body.removeChild(link);
};
/** WIFI management */
const wifiGetStatus = () => {
    return fetch(`http://${GBSControl.serverIP}/wifi/status?${+new Date()}`)
        .then((r) => r.json())
        .then((wifiStatus) => {
        GBSControl.wifi = wifiStatus;
        if (GBSControl.wifi.mode === "ap") {
            GBSControl.ui.wifiApButton.setAttribute("active", "");
            GBSControl.ui.wifiApButton.classList.add("gbs-button__secondary");
            GBSControl.ui.wifiStaButton.removeAttribute("active", "");
            GBSControl.ui.wifiStaButton.classList.remove("gbs-button__secondary");
            GBSControl.ui.wifiStaSSID.innerHTML = "STA | Scan Network";
        }
        else {
            GBSControl.ui.wifiApButton.removeAttribute("active", "");
            GBSControl.ui.wifiApButton.classList.remove("gbs-button__secondary");
            GBSControl.ui.wifiStaButton.setAttribute("active", "");
            GBSControl.ui.wifiStaButton.classList.add("gbs-button__secondary");
            GBSControl.ui.wifiStaSSID.innerHTML = `${GBSControl.wifi.ssid}`;
        }
    });
};
/**
 * Description placeholder
 */
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
    fetch(`http://${GBSControl.serverIP}/wifi/connect`, {
        method: "POST",
        body: formData,
    }).then(() => {
        gbsAlert(`GBSControl will restart and will connect to ${ssid}. Please wait some seconds then press OK`)
            .then(() => {
            window.location.href = "http://gbscontrol.local/";
        })
            .catch(() => { });
    });
};
/**
 * Description placeholder
 */
const wifiScanSSID = () => {
    GBSControl.ui.wifiStaButton.setAttribute("disabled", "");
    GBSControl.ui.wifiListTable.innerHTML = "";
    if (!GBSControl.scanSSIDDone) {
        fetch(`http://${GBSControl.serverIP}/wifi/list?${+new Date()}`, {
            method: 'POST'
        }).then(() => {
            GBSControl.scanSSIDDone = true;
            setTimeout(wifiScanSSID, 3000);
        });
        return;
    }
    fetch(`http://${GBSControl.serverIP}/wifi/list?${+new Date()}`, {
        method: 'POST'
    })
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
        <td class="gbs-icon" style="opacity:${parseInt(ssid.strength, 10) / 100}">wifi</td>
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
/**
 * Description placeholder
 *
 * @param {Event} event
 */
const wifiSelectSSID = (event) => {
    GBSControl.ui
        .wifiSSDInput.value = event.target.parentElement.getAttribute("gbs-ssid");
    GBSControl.ui.wifiPasswordInput.classList.remove("gbs-wifi__input--error");
    GBSControl.ui.wifiList.setAttribute("hidden", "");
    GBSControl.ui.wifiConnect.removeAttribute("hidden");
};
/**
 * Description placeholder
 *
 * @returns {*}
 */
const wifiSetAPMode = () => {
    if (GBSControl.wifi.mode === "ap") {
        return;
    }
    //   const formData = new FormData();
    //   formData.append("n", "dummy");
    return fetch(`http://${GBSControl.serverIP}/wifi/ap`, {
        method: "POST",
        // body: formData,
    }).then((response) => {
        gbsAlert("Switching to AP mode. Please connect to gbscontrol SSID and then click OK")
            .then(() => {
            window.location.href = "http://192.168.4.1";
        })
            .catch(() => { });
        return response;
    });
};
/** button click management */
const controlClick = (control) => () => {
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
/**
 * Description placeholder
 *
 * @param {HTMLButtonElement} control
 * @returns {() => void}
 */
const controlMouseDown = (control) => () => {
    clearInterval(control["__interval"]);
    const click = controlClick(control);
    click();
    control["__interval"] = setInterval(click, 300);
};
/**
 * Description placeholder
 *
 * @param {HTMLButtonElement} control
 * @returns {() => void}
 */
const controlMouseUp = (control) => () => {
    clearInterval(control["__interval"]);
};
/** inits */
const initMenuButtons = () => {
    const menuButtons = nodelistToArray(document.querySelector(".gbs-menu").querySelectorAll("button"));
    const sections = nodelistToArray(document.querySelectorAll("section"));
    const scroll = document.querySelector(".gbs-scroll");
    menuButtons.forEach((button) => button.addEventListener("click", () => {
        const section = button.getAttribute("gbs-section");
        sections.forEach((section) => section.setAttribute("hidden", ""));
        document
            .querySelector(`section[name="${section}"]`)
            .removeAttribute("hidden");
        menuButtons.forEach((btn) => btn.removeAttribute("active"));
        button.setAttribute("active", "");
        scroll.scrollTo(0, 1);
    }));
};
/**
 * Description placeholder
 */
const initGBSButtons = () => {
    const actions = {
        user: loadUser,
        action: loadDoc,
        setSlot,
    };
    const buttons = nodelistToArray(document.querySelectorAll("[gbs-click]"));
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
            button.addEventListener(!("ontouchstart" in window) ? "mousedown" : "touchstart", () => {
                callback();
                clearInterval(button["__interval"]);
                button["__interval"] = setInterval(callback, 300);
            });
            button.addEventListener(!("ontouchstart" in window) ? "mouseup" : "touchend", () => {
                clearInterval(button["__interval"]);
            });
        }
    });
};
/**
 * Description placeholder
 */
const initClearButton = () => {
    GBSControl.ui.outputClear.addEventListener("click", () => {
        GBSControl.ui.terminal.value = "";
    });
};
/**
 * Description placeholder
 */
const initControlMobileKeys = () => {
    const controls = document.querySelectorAll("[gbs-control-target]");
    const controlsKeys = document.querySelectorAll("[gbs-control-key]");
    controls.forEach((control) => {
        control.addEventListener("click", () => {
            GBSControl.controlKeysMobileMode = control.getAttribute("gbs-control-target");
            controls.forEach((crtl) => {
                crtl.removeAttribute("active");
            });
            control.setAttribute("active", "");
        });
    });
    controlsKeys.forEach((control) => {
        control.addEventListener(!("ontouchstart" in window) ? "mousedown" : "touchstart", controlMouseDown(control));
        control.addEventListener(!("ontouchstart" in window) ? "mouseup" : "touchend", controlMouseUp(control));
    });
};
/**
 * Description placeholder
 */
const initLegendHelpers = () => {
    nodelistToArray(document.querySelectorAll(".gbs-fieldset__legend--help")).forEach((e) => {
        e.addEventListener("click", toggleHelp);
    });
};
/**
 * Description placeholder
 */
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
/**
 * Description placeholder
 */
const initSlotButtons = () => {
    GBSControl.ui.slotContainer.innerHTML = getSlotsHTML();
    GBSControl.ui.slotButtonList = nodelistToArray(document.querySelectorAll('[gbs-role="slot"]'));
};
/**
 * Description placeholder
 */
const initUIElements = () => {
    GBSControl.ui = {
        terminal: document.getElementById("outputTextArea"),
        webSocketConnectionWarning: document.getElementById("websocketWarning"),
        presetButtonList: nodelistToArray(document.querySelectorAll("[gbs-role='preset']")),
        slotButtonList: nodelistToArray(document.querySelectorAll('[gbs-role="slot"]')),
        toggleConsole: document.querySelector("[gbs-output-toggle]"),
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
/**
 * Description placeholder
 */
const initGeneralListeners = () => {
    window.addEventListener("resize", () => {
        updateViewPort();
    });
    GBSControl.ui.backupInput.addEventListener("change", (event) => {
        const fileList = event.target["files"];
        readLocalFile(fileList[0]);
        GBSControl.ui.backupInput.value = "";
    });
    GBSControl.ui.backupButton.addEventListener("click", doBackup);
    GBSControl.ui.wifiListTable.addEventListener("click", wifiSelectSSID);
    GBSControl.ui.wifiConnectButton.addEventListener("click", wifiConnect);
    GBSControl.ui.wifiApButton.addEventListener("click", wifiSetAPMode);
    GBSControl.ui.wifiStaButton.addEventListener("click", wifiScanSSID);
    GBSControl.ui.developerSwitch.addEventListener("click", toggleDeveloperMode);
    GBSControl.ui.customSlotFilters.addEventListener("click", toggleCustomSlotFilters);
    GBSControl.ui.alertOk.addEventListener("click", () => {
        GBSControl.ui.alert.setAttribute("hidden", "");
        gbsAlertPromise.resolve();
    });
    GBSControl.ui.promptOk.addEventListener("click", () => {
        GBSControl.ui.prompt.setAttribute("hidden", "");
        const value = GBSControl.ui.promptInput.value;
        if (value !== undefined || value.length > 0) {
            gbsPromptPromise.resolve(GBSControl.ui.promptInput.value);
        }
        else {
            gbsPromptPromise.reject();
        }
    });
    GBSControl.ui.promptCancel.addEventListener("click", () => {
        GBSControl.ui.prompt.setAttribute("hidden", "");
        gbsPromptPromise.reject();
    });
    GBSControl.ui.promptInput.addEventListener("keydown", (event) => {
        if (event.keyCode === 13) {
            GBSControl.ui.prompt.setAttribute("hidden", "");
            const value = GBSControl.ui.promptInput.value;
            if (value !== undefined || value.length > 0) {
                gbsPromptPromise.resolve(GBSControl.ui.promptInput.value);
            }
            else {
                gbsPromptPromise.reject();
            }
        }
        if (event.keyCode === 27) {
            gbsPromptPromise.reject();
        }
    });
};
/**
 * Description placeholder
 */
const initDeveloperMode = () => {
    const devMode = GBSStorage.read("developerMode");
    if (devMode === undefined) {
        GBSStorage.write("developerMode", false);
        updateDeveloperMode(false);
    }
    else {
        updateDeveloperMode(devMode);
    }
    // toggle console visibility button
    GBSControl.ui.toggleConsole.addEventListener("click", () => {
        updateConsoleVisibility(devMode);
    });
};
/**
 * Description placeholder
 */
const initHelp = () => {
    let help = GBSStorage.read("help");
    if (help === undefined) {
        help = false;
        GBSStorage.write("help", help);
    }
    updateHelp(help);
};
/**
 * Description placeholder
 *
 * @type {{ resolve: any; reject: any; }}
 */
const gbsAlertPromise = {
    resolve: null,
    reject: null,
};
/**
 * Description placeholder
 *
 * @param {*} event
 */
const alertKeyListener = (event) => {
    if (event.keyCode === 13) {
        gbsAlertPromise.resolve();
    }
    if (event.keyCode === 27) {
        gbsAlertPromise.reject();
    }
};
/**
 * Description placeholder
 *
 * @param {string} text
 * @returns {*}
 */
const gbsAlert = (text) => {
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
/**
 * Description placeholder
 *
 * @type {{ resolve: any; reject: any; }}
 */
const gbsPromptPromise = {
    resolve: null,
    reject: null,
};
/**
 * Description placeholder
 *
 * @param {string} text
 * @param {string} [defaultValue=""]
 * @returns {*}
 */
const gbsPrompt = (text, defaultValue = "") => {
    GBSControl.ui.promptContent.textContent = text;
    GBSControl.ui.prompt.removeAttribute("hidden");
    GBSControl.ui.promptInput.value = defaultValue;
    return new Promise((resolve, reject) => {
        gbsPromptPromise.resolve = resolve;
        gbsPromptPromise.reject = reject;
        GBSControl.ui.promptInput.focus();
    });
};
/**
 * Description placeholder
 */
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
/**
 * Description placeholder
 */
const main = () => {
    GBSControl.serverIP = location.hostname;
    GBSControl.webSocketServerUrl = `ws://${GBSControl.serverIP}:81/`;
    document
        .querySelector(".gbs-loader img")
        .setAttribute("src", document.head
        .querySelector('[rel="apple-touch-icon"]')
        .getAttribute("href"));
    fetchSlotNamesAndInit();
};
main();
