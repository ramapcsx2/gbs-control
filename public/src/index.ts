/*
 * DEVELOPER MEMO:
 *   1. WebUI icons: https://fonts.google.com/icons
 *
 */

/**
 * Description placeholder
 *
 * @interface Struct
 * @typedef {Struct}
 */
interface Struct {
    /**
     * Description placeholder
     *
     * @type {string}
     */
    name: string;
    /**
     * Description placeholder
     *
     * @type {("byte" | "string")}
     */
    type: "byte" | "string";
    /**
     * Description placeholder
     *
     * @type {number}
     */
    size: number;
}

/**
 * Description placeholder
 *
 * @interface StructDescriptors
 * @typedef {StructDescriptors}
 */
interface StructDescriptors {
    /**
     * Description placeholder
     */
    [key: string]: Struct[];
}

/**
 * Must be aligned with slots.h -> SlotMeta structure
 *
 * @type {StructDescriptors}
 */
const Structs: StructDescriptors = {
    slots: [
        { name: "name", type: "string", size: 25 },
        { name: "slot", type: "byte", size: 1 },
        { name: "resolutionID", type: "string", size: 1 },

        { name: "scanlines", type: "byte", size: 1 },
        { name: "scanlinesStrength", type: "byte", size: 1 },
        { name: "vdsLineFilter", type: "byte", size: 1 },
        { name: "stepResponse", type: "byte", size: 1 },
        { name: "peaking", type: "byte", size: 1 },
        { name: "adcAutoGain", type: "byte", size: 1 },
        { name: "frameTimeLock", type: "byte", size: 1 },

        { name: "frameTimeLockMethod", type: "byte", size: 1 },
        { name: "motionAdaptive", type: "byte", size: 1 },
        { name: "bob", type: "byte", size: 1 },
        { name: "fullHeight", type: "byte", size: 1 },
        { name: "matchPreset", type: "byte", size: 1 },
        { name: "palForce60", type: "byte", size: 1 },
    ],
};

/**
 * Description placeholder
 *
 * @type {{ pos: number; parseStructArray(buff: ArrayBuffer, structsDescriptors: StructDescriptors, struct: string): any; getValue(buff: {}, structItem: { type: "string" | "byte"; size: number; }): any; getSize(structsDescriptors: StructDescriptors, struct: string): any; }}
 */
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
    getValue(
        buff: any[],
        structItem: { type: "byte" | "string"; size: number }
    ) {
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
                            return String.fromCharCode(
                                buff[currentPos + index]
                            );
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
        a: "button240p",
        c: "button960p",
        d: "button960p", // 50Hz
        e: "button1024p",
        f: "button1024p", // 50Hz
        g: "button720p",
        h: "button720p", // 50Hz
        i: "button480p",
        j: "button480p", // 50Hz
        k: "button1080p",
        l: "button1080p", // 50Hz
        m: "button15kHz",
        n: "button15kHz", // 50Hz
        p: "button576p", // 50Hz
        q: "buttonSourcePassThrough",
        s: "buttonSourcePassThrough", // PresetHdBypass
        u: "buttonSourcePassThrough", // PresetBypassRGBHV
        // 'w': "buttonLoadCustomPreset",
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
    maxSlots: 50,
    queuedText: "",
    scanSSIDDone: false,
    serverIP: "",
    structs: null,
    timeOutWs: 0,
    ui: {
        backupButton: null,
        backupInput: null,
        // customSlotFilters: null,
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
        alertAck: null,
        alertAct: null,
        alertContent: null,
        prompt: null,
        promptOk: null,
        promptCancel: null,
        promptContent: null,
        promptInput: null,
        removeSlotButton: null,
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
 *
 * @template Element
 * @param {(| HTMLCollectionOf<globalThis.Element>
 *     | NodeListOf<globalThis.Element>)} nodelist
 * @returns {Element[]}
 */
const nodelistToArray = <Element>(
    nodelist:
        | HTMLCollectionOf<globalThis.Element>
        | NodeListOf<globalThis.Element>
): Element[] => {
    return Array.prototype.slice.call(nodelist);
};

/**
 * Description placeholder
 *
 * @param {string} id
 * @returns {(button: HTMLElement, _index: any, _array: any) => void}
 */
const toggleButtonActive =
    (id: string) => (button: HTMLElement, _index: any, _array: any) => {
        button.removeAttribute("active");

        if (button.getAttribute("gbs-element-ref") === id) {
            button.setAttribute("active", "");
        }
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
const alertKeyListener = (event: any) => {
    if (event.keyCode === 13) {
        gbsAlertPromise.resolve();
    }
    if (event.keyCode === 27) {
        gbsAlertPromise.reject();
    }
};

const alertActEventListener = (e: any) => {
    gbsAlertPromise.reject();
};

/**
 * Description placeholder
 *
 * @param {string} text
 * @returns {*}
 */
const gbsAlert = (text: string, ackText: string = "", actText: string = "") => {
    GBSControl.ui.alertContent.insertAdjacentHTML("afterbegin", text);
    GBSControl.ui.alert.removeAttribute("hidden");
    document.addEventListener("keyup", alertKeyListener);
    if (ackText !== "") {
        GBSControl.ui.alertAck.insertAdjacentHTML("afterbegin", ackText);
    } else
        GBSControl.ui.alertAck.insertAdjacentHTML(
            "afterbegin",
            '<div class="gbs-icon">done</div><div>Ok</div>'
        );

    if (actText !== "") {
        GBSControl.ui.alertAct.insertAdjacentHTML("afterbegin", actText);
        GBSControl.ui.alertAct.removeAttribute("disabled");
        GBSControl.ui.alertAct.addEventListener("click", alertActEventListener);
    }
    return new Promise((resolve, reject) => {
        const gbsAlertClean = () => {
            document.removeEventListener("keyup", alertKeyListener);
            GBSControl.ui.alertAct.removeEventListener(
                "click",
                alertActEventListener
            );
            GBSControl.ui.alertAct.setAttribute("disabled", "");
            GBSControl.ui.alertAck.textContent = "";
            GBSControl.ui.alertAct.textContent = "";
            GBSControl.ui.alertContent.textContent = "";
            GBSControl.ui.alert.setAttribute("hidden", "");
        };
        gbsAlertPromise.resolve = (e) => {
            gbsAlertClean();
            return resolve(e);
        };
        gbsAlertPromise.reject = () => {
            gbsAlertClean();
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

/**
 * Description placeholder
 *
 * @param {boolean} mode
 */
const displayWifiWarning = (mode: boolean) => {
    GBSControl.ui.webSocketConnectionWarning.style.display = mode
        ? "block"
        : "none";
};

/**
 * Flip a toggle switch
 *
 * @param {(HTMLTableCellElement | HTMLElement)} button
 * @param {boolean} mode
 */
const toggleButtonCheck = (
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


/**
 * Description placeholder
 *
 * @param {HTMLElement} button this is a slot button HTMLElement
 */
const removeSlotButtonCheck = (button: Element) => {
    if(button.hasAttribute("active")) {
        const currentName = button.getAttribute("gbs-name");
        if (currentName && currentName.trim() !== "Empty") {
            GBSControl.ui.removeSlotButton.removeAttribute("disabled")
        } else {
            GBSControl.ui.removeSlotButton.setAttribute("disabled", "")
        }
    }
};

/**
 * Description placeholder
 */
const updateTerminal = () => {
    if (GBSControl.queuedText.length > 0) {
        requestAnimationFrame(() => {
            GBSControl.ui.terminal.value += GBSControl.queuedText;
            GBSControl.ui.terminal.scrollTop =
                GBSControl.ui.terminal.scrollHeight;
            GBSControl.queuedText = "";
        });
    }
};

/**
 * Description placeholder
 */
const updateViewPort = () => {
    document.documentElement.style.setProperty(
        "--viewport-height",
        window.innerHeight + "px"
    );
};

/**
 * Handle webSocket response
 */
const createWebSocket = () => {
    if (GBSControl.ws && checkReadyState()) {
        return;
    }

    GBSControl.wsNoSuccessConnectingCounter = 0;
    GBSControl.ws = new WebSocket(GBSControl.webSocketServerUrl, ["arduino"]);

    GBSControl.ws.onopen = () => {
        // console.log("ws onopen");

        displayWifiWarning(false);

        GBSControl.wsConnectCounter++;
        clearTimeout(GBSControl.wsTimeout);
        GBSControl.wsTimeout = window.setTimeout(timeOutWs, 6000);
        GBSControl.isWsActive = true;
        GBSControl.wsNoSuccessConnectingCounter = 0;
    };

    GBSControl.ws.onclose = () => {
        // console.log("ws.onclose");

        clearTimeout(GBSControl.wsTimeout);
        GBSControl.isWsActive = false;
    };

    GBSControl.ws.onmessage = async (message: any) => {
        clearTimeout(GBSControl.wsTimeout);
        GBSControl.wsTimeout = window.setTimeout(timeOutWs, 2700);
        GBSControl.isWsActive = true;
        // message data is blob
        let buf = null;
        try {
            buf = await message.data.arrayBuffer();
        } catch (err) {
            // must not exit here since we're filtering out
            // terminal data and system state data with '#'
        }
        // into array of DEC values
        const bufArr = Array.from(new Uint8Array(buf));
        const [
            optionByte0, // always #
            optionByte1, // current slot ID (int)
            optionByte2, // current resolution ()
            // system preferences (preference file values)
            optionByte3, // wantScanlines & wantVdsLineFilter & wantStepResponse & wantPeaking & enableAutoGain & enableFrameTimeLock
            optionByte4, // deintMode & wantTap6 & wantFullHeight & matchPresetSource & PalForce60
            optionByte5, // wantOutputComponent & enableCalibrationADC & preferScalingRgbhv & disableExternalClockGenerator
            // developer tab
            optionByte6, // printInfos, invertSync, oversampling, ADC Filter
            // system tab
            optionByte7, // enableOTA
        ] = bufArr;

        if (optionByte0 != "#".charCodeAt(0)) {
            GBSControl.queuedText += message.data;
            GBSControl.dataQueued += message.data.length;

            if (GBSControl.dataQueued >= 70000) {
                GBSControl.ui.terminal.value = "";
                GBSControl.dataQueued = 0;
            }
            return;
        }

        // current slot
        const slotId = `slot-${String.fromCharCode(optionByte1)}`;
        const activeSlotButton = document.querySelector(
            `[gbs-element-ref="${slotId}"]`
        );
        if (activeSlotButton) {
            GBSControl.ui.slotButtonList.forEach(toggleButtonActive(slotId));
            // control slot remove button
            removeSlotButtonCheck(activeSlotButton);
        }
        // curent resolution
        const resID = GBSControl.buttonMapping[String.fromCharCode(optionByte2)];
        const resEl = document.querySelector(
            `[gbs-element-ref="${resID}"]`
        );
        const activePresetButton = resEl
            ? resEl.getAttribute("gbs-element-ref")
            : "none";
        GBSControl.ui.presetButtonList.forEach(
            toggleButtonActive(activePresetButton)
        );
        // settings tab & system preferences
        const optionButtonList = [
            ...nodelistToArray<HTMLButtonElement>(GBSControl.ui.toggleList),
            ...nodelistToArray<HTMLButtonElement>(
                GBSControl.ui.toggleSwichList
            ),
        ];

        optionButtonList.forEach((button) => {
            const toggleData =
                button.getAttribute("gbs-toggle") ||
                button.getAttribute("gbs-toggle-switch");

            if (toggleData !== null) {
                switch (toggleData) {
                    /** 0: settings */
                    // case "scanlinesStrength":
                    /** 1 */
                    case "scanlines":
                        toggleButtonCheck(
                            button,
                            (optionByte3 & 0x01) == 0x01
                        );
                        break;
                    case "vdsLineFilter":
                        toggleButtonCheck(
                            button,
                            (optionByte3 & 0x02) == 0x02
                        );
                        break;
                    case "stepResponse":
                        toggleButtonCheck(
                            button,
                            (optionByte3 & 0x04) == 0x04
                        );
                        break;
                    case "peaking":
                        toggleButtonCheck(
                            button,
                            (optionByte3 & 0x08) == 0x08
                        );
                        break;
                    case "adcAutoGain":
                        toggleButtonCheck(
                            button,
                            (optionByte3 & 0x10) == 0x10
                        );
                        break;
                    case "frameTimeLock":
                        toggleButtonCheck(
                            button,
                            (optionByte3 & 0x20) == 0x20
                        );
                        break;
                    /** 2 */
                    // case "fameTimeLockMethod":
                    /** 3 */
                    case "motionAdaptive":
                        toggleButtonCheck(
                            button,
                            (optionByte4 & 0x01) == 0x01
                        );
                        break;
                    case "bob":
                        toggleButtonCheck(
                            button,
                            (optionByte4 & 0x02) == 0x02
                        );
                        break;
                    case "fullHeight":
                        toggleButtonCheck(
                            button,
                            (optionByte4 & 0x04) == 0x04
                        );
                        break;
                    case "matchPreset":
                        toggleButtonCheck(
                            button,
                            (optionByte4 & 0x08) == 0x08
                        );
                        break;
                    case "palForce60":
                        toggleButtonCheck(
                            button,
                            (optionByte4 & 0x10) == 0x10
                        );
                        break;
                    /** 4: system preferences tab */
                    case "wantOutputComponent":
                        toggleButtonCheck(
                            button,
                            (optionByte5 & 0x01) == 0x01
                        );
                        break;
                    case "enableCalibrationADC":
                        toggleButtonCheck(
                            button,
                            (optionByte5 & 0x02) == 0x02
                        );
                        break;
                    case "preferScalingRgbhv":
                        toggleButtonCheck(
                            button,
                            (optionByte5 & 0x04) == 0x04
                        );
                        break;
                    case "disableExternalClockGenerator":
                        toggleButtonCheck(
                            button,
                            (optionByte5 & 0x08) == 0x08
                        );
                        break;
                }
            }
        });
        // developer tab
        const printInfoButton = document.querySelector(
            `button[gbs-message="i"][gbs-message-type="action"]`
        );
        const invertSync = document.querySelector(
            `button[gbs-message="8"][gbs-message-type="action"]`
        );
        const oversampling = document.querySelector(
            `button[gbs-message="o"][gbs-message-type="action"]`
        );
        const adcFilter = document.querySelector(
            `button[gbs-message="F"][gbs-message-type="action"]`
        );
        if ((optionByte6 & 0x01) == 0x01)
            printInfoButton.setAttribute("active", "");
        else printInfoButton.removeAttribute("active");
        if ((optionByte6 & 0x02) == 0x02)
            invertSync.setAttribute("active", "");
        else invertSync.removeAttribute("active");
        if ((optionByte6 & 0x04) == 0x04)
            oversampling.setAttribute("active", "");
        else oversampling.removeAttribute("active");
        if ((optionByte6 & 0x08) == 0x08)
            adcFilter.setAttribute("active", "");
        else adcFilter.removeAttribute("active");

        // system tab
        const enableOTAButton = document.querySelector(
            `button[gbs-message="c"][gbs-message-type="action"]`
        );
        if ((optionByte7 & 0x01) == 0x01)
            enableOTAButton.setAttribute("active", "");
        else enableOTAButton.removeAttribute("active");
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

/**
 * Description placeholder
 */
const createIntervalChecks = () => {
    GBSControl.wsCheckTimer = window.setInterval(checkWebSocketServer, 500);
    GBSControl.updateTerminalTimer = window.setInterval(updateTerminal, 50);
};

/* API services */

/**
 * Description placeholder
 *
 * @param {string} link
 * @returns {*}
 */
const loadDoc = (link: string) => {
    return fetch(
        `http://${GBSControl.serverIP}/sc?${link}&nocache=${new Date().getTime()}`
    );
};

/**
 * user command handler
 *
 * @param {string} link
 * @returns {*}
 */
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

/** WIFI management */
const wifiGetStatus = () => {
    return fetch(`http://${GBSControl.serverIP}/wifi/status?${+new Date()}`)
        .then((r) => r.json())
        .then((wifiStatus: { mode: string; ssid: string }) => {
            GBSControl.wifi = wifiStatus;
            if (GBSControl.wifi.mode === "ap") {
                GBSControl.ui.wifiApButton.setAttribute("active", "");
                GBSControl.ui.wifiApButton.classList.add(
                    "gbs-button__secondary"
                );
                GBSControl.ui.wifiStaButton.removeAttribute("active", "");
                GBSControl.ui.wifiStaButton.classList.remove(
                    "gbs-button__secondary"
                );
                GBSControl.ui.wifiStaSSID.innerHTML = "STA | Scan Network";
            } else {
                GBSControl.ui.wifiApButton.removeAttribute("active", "");
                GBSControl.ui.wifiApButton.classList.remove(
                    "gbs-button__secondary"
                );
                GBSControl.ui.wifiStaButton.setAttribute("active", "");
                GBSControl.ui.wifiStaButton.classList.add(
                    "gbs-button__secondary"
                );
                GBSControl.ui.wifiStaSSID.innerHTML = `${GBSControl.wifi.ssid}`;
            }
        });
};

/**
 * Does connect to selected WiFi network
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
        gbsAlert(
            `GBSControl will restart and will connect to ${ssid}. Please wait few seconds then press OK`
        )
            .then(() => {
                window.location.href = "http://gbscontrol.local/";
            })
            .catch(() => {});
    });
};

/**
 * Query WiFi networks
 */
const wifiScanSSID = () => {
    GBSControl.ui.wifiStaButton.setAttribute("disabled", "");
    GBSControl.ui.wifiListTable.innerHTML = "";

    if (!GBSControl.scanSSIDDone) {
        fetch(`http://${GBSControl.serverIP}/wifi/list?${+new Date()}`, {
            method: "POST",
        }).then(() => {
            GBSControl.scanSSIDDone = true;
            window.setTimeout(wifiScanSSID, 3000);
        });
        return;
    }

    fetch(`http://${GBSControl.serverIP}/wifi/list?${+new Date()}`, {
        method: "POST",
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

/**
 * Description placeholder
 *
 * @param {Event} event
 */
const wifiSelectSSID = (event: Event) => {
    (GBSControl.ui.wifiSSDInput as HTMLInputElement).value = (
        event.target as HTMLElement
    ).parentElement.getAttribute("gbs-ssid");
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
        gbsAlert(
            "Switching to AP mode. Please connect to gbscontrol SSID and then click OK"
        )
            .then(() => {
                window.location.href = "http://192.168.4.1";
            })
            .catch(() => {});
        return response;
    });
};

/** SLOT management */

/**
 * Description placeholder
 */
const fetchSlotNamesErrorRetry = () => {
    window.setTimeout(fetchSlotNamesAndInit, 1000);
};

/**
 * Description placeholder
 */
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
        // customSlotFilters: document.querySelector("[gbs-slot-custom-filters]"),
        alert: document.querySelector('section[name="alert"]'),
        alertAck: document.querySelector("[gbs-alert-ack]"),
        alertAct: document.querySelector("[gbs-alert-act]"),
        alertContent: document.querySelector("[gbs-alert-content]"),
        prompt: document.querySelector('section[name="prompt"]'),
        promptOk: document.querySelector("[gbs-prompt-ok]"),
        promptCancel: document.querySelector("[gbs-prompt-cancel]"),
        promptContent: document.querySelector("[gbs-prompt-content]"),
        promptInput: document.querySelector('[gbs-input="prompt-input"]'),
        removeSlotButton: document.querySelector('[gbs-element-ref="buttonRemoveCustomPreset"]'),
    };
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
                window.setTimeout(hideLoading, 1000);
            });
        }, fetchSlotNamesErrorRetry)
        .catch(fetchSlotNamesErrorRetry);
};

/**
 * Description placeholder
 *
 * @returns {*}
 */
const fetchSlotNames = () => {
    return fetch(`http://${GBSControl.serverIP}/bin/slots.bin?${+new Date()}`)
        .then((response) => response.arrayBuffer())
        .then((arrayBuffer: ArrayBuffer) => {
            if (
                arrayBuffer.byteLength ===
                StructParser.getSize(Structs, "slots") * GBSControl.maxSlots
            ) {
                GBSControl.structs = {
                    slots: StructParser.parseStructArray(
                        arrayBuffer,
                        Structs,
                        "slots"
                    ),
                };
                return true;
            }
            return false;
        });
};

/**
 * Remove slot handler
 */
const removePreset = () => {
    const currentSlot = document.querySelector('[gbs-role="slot"][active]');

    if (!currentSlot) {
        return;
    }

    const currentIndex = currentSlot.getAttribute("gbs-message");
    const currentName = currentSlot.getAttribute("gbs-name");
    if (currentName && currentName.trim() !== "Empty") {
        gbsAlert(
            `<p>Are you sure to remove slot: ${currentName}?</p><p>This action also removes all related presets.</p>`,
            '<div class="gbs-icon">done</div><div>Yes</div>',
            '<div class="gbs-icon">close</div><div>No</div>'
        )
            .then(() => {
                return fetch(
                    `http://${GBSControl.serverIP}/slot/remove?index=${currentIndex}&${+new Date()}`
                ).then(() => {
                    console.log("slot removed, reloadng slots...");
                    fetchSlotNames().then((success: boolean) => {
                        if (success) {
                            updateSlotNames();
                        } else {
                            fetchSlotNamesErrorRetry();
                        }
                    });
                });
            })
            .catch(() => {});
    }
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
    const currentIndex = currentSlot.getAttribute("gbs-message");
    gbsPrompt(
        "Assign a slot name",
        GBSControl.structs.slots[currentIndex].name || key
    )
        .then((currentName: string) => {
            if (currentName && currentName.trim() !== "Empty") {
                currentSlot.setAttribute("gbs-name", currentName);
                fetch(
                    `http://${GBSControl.serverIP}/slot/save?index=${currentIndex}&name=${currentName.substring(
                        0,
                        24
                    )}&${+new Date()}`
                ).then(() => {
                    // loadUser("4").then(() => {
                    window.setTimeout(() => {
                        fetchSlotNames().then((success: boolean) => {
                            if (success) {
                                updateSlotNames();
                            }
                        });
                    }, 500);
                    // });
                });
            }
        })
        .catch(() => {});
};

/**
 * Description placeholder
 */
// const loadPreset = () => {
//     loadUser("3").then(() => {
//         if (GBSStorage.read("customSlotFilters") === true) {
//             window.setTimeout(() => {
//                 fetch(`http://${GBSControl.serverIP}/gbs/restore-filters?${+new Date()}`);
//             }, 250);
//         }
//     });
// };

/**
 * Description placeholder
 *
 * @returns {*}
 */
const getSlotsHTML = () => {
    // prettier-ignore
    //     return [
    //         'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    //         'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    //         '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '.', '_', '~', '(', ')', '!', '*', ':', ','
    //     ].map((chr, idx) => {

    //         // gbs-message="${chr}"
    //         return `<button
    //     class="gbs-button gbs-button__slot"
    //     gbs-slot-message="${idx}"
    //     gbs-message-type="setSlot"
    //     gbs-click="normal"
    //     gbs-element-ref="slot-${chr}"
    //     gbs-meta="1024&#xa;x768"
    //     gbs-role="slot"
    //     gbs-name="slot-${idx}"
    //   ></button>`;

    //     }).join('');
    // TODO: 'i' max. rely on SLOTS_TOTAL which is ambigous
    let str = ``;
    for (let i = 0; i != GBSControl.maxSlots; i++) {
        str += `<button
        class="gbs-button gbs-button__slot"
        gbs-message="${i}"
        gbs-message-type="setSlot"
        gbs-click="normal"
        gbs-element-ref="slot-${i}"
        gbs-meta="NONE"
        gbs-role="slot"
        gbs-name="slot-${i}"
        ></button>`;
    }
    return str;
};

/**
 * Description placeholder
 *
 * @param {string} slot
 */
const setSlot = (slot: string, el: HTMLElement) => {
    fetch(
        `http://${GBSControl.serverIP}/slot/set?index=${slot}&${+new Date()}`
    );
};

/**
 * Description placeholder
 */
const updateSlotNames = () => {
    for (let i = 0; i < GBSControl.maxSlots; i++) {
        const el = document.querySelector(
            `[gbs-message="${i}"][gbs-role="slot"]`
        );

        el.setAttribute("gbs-name", GBSControl.structs.slots[i].name);
        el.setAttribute(
            "gbs-meta",
            getSlotPresetName(GBSControl.structs.slots[i].resolutionID)
        );
    }
};

/**
 * Must be aligned with options.h -> OutputResolution
 *
 * @param {string} resolutionID
 * @returns {("960p" | "1024p" | "720p" | "480p" | "1080p" | "DOWNSCALE" | "576p" | "BYPASS" | "BYPASS HD" | "BYPASS RGBHV" | "240p" | "NONE")}
 */
const getSlotPresetName = (resolutionID: string) => {
    switch (resolutionID) {
        case "c":
        case "d":
            // case 0x011:
            return "960p";
        case "e":
        case "f":
            // case 0x012:
            return "1024p";
        case "g":
        case "h":
            // case 0x013:
            return "720p";
        case "i":
        case "j":
            // case 0x015:
            return "480p";
        case "k":
        case "l":
            return "1080p";
        case "m":
        case "n":
            // case 0x016:
            return "DOWNSCALE";
        case "p":
            return "576p";
        case "q":
            return "BYPASS";
        case "s": // bypass 1
            return "BYPASS HD";
        case "u": // bypass 2
            return "BYPASS RGBHV";
        // case 12:
        //     return "CUSTOM";
        case "a":
            return "240p";
        default:
            return "NONE";
    }
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
// const toggleCustomSlotFilters = () => {
//     const customSlotFilters = GBSStorage.read("customSlotFilters");
//     GBSStorage.write("customSlotFilters", !customSlotFilters);
//     updateCustomSlotFilters(!customSlotFilters);
// };

/**
 * Description placeholder
 *
 * @param {boolean} help
 */
const updateHelp = (help: boolean) => {
    if (help) {
        document.body.classList.remove("gbs-help-hide");
    } else {
        document.body.classList.add("gbs-help-hide");
    }
};

/**
 * Toggle console visibility (see corresponding button)
 *
 * @param {boolean} developerMode
 */
const updateConsoleVisibility = (developerMode: boolean) => {
    // const developerMode = GBSStorage.read("developerMode") as boolean;
    if (developerMode) {
        const consoleStatus = GBSStorage.read("consoleVisible") as boolean;
        if (consoleStatus !== true) {
            GBSStorage.write("consoleVisible", true);
            GBSControl.ui.toggleConsole.setAttribute("active", "");
            document.body.classList.remove("gbs-output-hide");
        } else {
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
const updateDeveloperMode = (developerMode: boolean) => {
    const el = document.querySelector(
        '[gbs-section="developer"]'
    ) as HTMLElement;
    if (developerMode) {
        GBSStorage.write("consoleVisible", true);
        el.removeAttribute("hidden");
        GBSControl.ui.developerSwitch.setAttribute("active", "");
        document.body.classList.remove("gbs-output-hide");
    } else {
        el.setAttribute("hidden", "");
        GBSControl.ui.developerSwitch.removeAttribute("active");
        document.body.classList.add("gbs-output-hide");
    }

    GBSControl.ui.developerSwitch.querySelector(".gbs-icon").innerText =
        developerMode ? "toggle_on" : "toggle_off";
};

/**
 * Description placeholder
 *
 * @param {boolean} [customFilters=GBSStorage.read("customSlotFilters") === true]
 */
// const updateCustomSlotFilters = (
//     customFilters: boolean = GBSStorage.read("customSlotFilters") === true
// ) => {
//     if (customFilters) {
//         GBSControl.ui.customSlotFilters.setAttribute("active", "");
//     } else {
//         GBSControl.ui.customSlotFilters.removeAttribute("active");
//     }

//     GBSControl.ui.customSlotFilters.querySelector(
//         ".gbs-icon"
//     ).innerText = customFilters ? "toggle_on" : "toggle_off";
// };

/**
 * Description placeholder
 *
 * @type {({ lsObject: {}; write(key: string, value: any): void; read(key: string): string | number | boolean; })}
 */
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
const checkFetchResponseStatus = (response: Response) => {
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
// const readLocalFile = (file: File) => {
//     const reader = new FileReader();
//     reader.addEventListener("load", (event) => {
//         doRestore(reader.result as ArrayBuffer);
//     });
//     reader.readAsArrayBuffer(file);
// };

/** backup / restore */

const doBackup = () => {
    window.location.href = `http://${GBSControl.serverIP}/data/backup?ts=${new Date().getTime()}`;
    // let backupFiles: string[];
    // let done = 0;
    // let total = 0;
    // fetch(`http://${GBSControl.serverIP}/data/dir`)
    //     .then((r) => r.json())
    //     .then((files: string[]) => {
    //         backupFiles = files;
    //         total = files.length;
    //         const funcs = files.map((path: string) => () => {
    //             return fetch(`http://${GBSControl.serverIP}/data/download?file=${path}&${+new Date()}`).then(
    //                 (response) => {
    //                     GBSControl.ui.progressBackup.setAttribute(
    //                         "gbs-progress",
    //                         `${done}/${total}`
    //                     );
    //                     done++;
    //                     return checkFetchResponseStatus(response) && response.arrayBuffer();
    //                 }
    //             );
    //         });

    //         return serial(funcs);
    //     })
    //     .then((files: ArrayBuffer[]) => {
    // console.log("after download backup complete");
    //         const headerDescriptor = files.reduce((acc, f, index) => {
    //             acc[backupFiles[index]] = f.byteLength;
    //             return acc;
    //         }, {});

    //         const backupFilesJSON = JSON.stringify(headerDescriptor);
    //         const backupFilesJSONSize = backupFilesJSON.length;

    //         const mainHeader = [
    //             (backupFilesJSONSize >> 24) & 255, // size
    //             (backupFilesJSONSize >> 16) & 255, // size
    //             (backupFilesJSONSize >> 8) & 255, // size
    //             (backupFilesJSONSize >> 0) & 255,
    //         ];

    //         const outputArray: number[] = [
    //             ...mainHeader,
    //             ...backupFilesJSON.split("").map((c) => c.charCodeAt(0)),
    //             ...files.reduce((acc, f, index) => {
    //                 acc = acc.concat(Array.from(new Uint8Array(f)));
    //                 return acc;
    //             }, []),
    //         ];

    //         downloadBlob(
    //             new Blob([new Uint8Array(outputArray)]),
    //             `gbs-control.backup-${+new Date()}.bin`
    //         );
    //         GBSControl.ui.progressBackup.setAttribute("gbs-progress", ``);
    //     });
};

/**
 * Restore SLOTS from backup
 *
 * @param {ArrayBuffer} file
 */
// const doRestore = (file: ArrayBuffer, f: File) => {
const doRestore = (f: File) => {
    const { backupInput } = GBSControl.ui;
    // const fileBuffer = new Uint8Array(file);
    // const headerCheck = fileBuffer.slice(4, 6);

    const bkpTs = f.name.substring(
        f.name.lastIndexOf("-") + 1,
        f.name.lastIndexOf(".")
    );
    const backupDate = new Date(parseInt(bkpTs));

    // if (headerCheck[0] !== 0x7b || headerCheck[1] !== 0x22) {
    backupInput.setAttribute("disabled", "");
    const formData = new FormData();
    formData.append("gbs-backup.bin", f, f.name);
    const setAlertBody = () => {
        const fsize = f.size / 1024;
        return (
            "<p>Backup File:</p><p>Backup date: " +
            backupDate.toLocaleString() +
            "</p><p>Size: " +
            fsize.toFixed(2) +
            " kB</p>"
        );
    };
    gbsAlert(
        setAlertBody() as string,
        '<div class="gbs-icon">close</div><div>Reject</div>',
        '<div class="gbs-icon">done</div><div>Restore</div>'
    )
        .then(
            () => {
                backupInput.removeAttribute("disabled");
            },
            () => {
                return fetch(`http://${GBSControl.serverIP}/data/restore`, {
                    method: "POST",
                    body: formData,
                }).then((response) => {
                    // backupInput.removeAttribute("disabled");
                    window.setTimeout(() => {
                        window.location.reload();
                    }, 2000);
                    return response;
                });
            }
        )
        .catch(() => {
            backupInput.removeAttribute("disabled");
        });
    //    return;
    // }
    // const b0 = fileBuffer[0],
    //     b1 = fileBuffer[1],
    //     b2 = fileBuffer[2],
    //     b3 = fileBuffer[3];
    // const headerSize = (b0 << 24) + (b1 << 16) + (b2 << 8) + b3;
    // const headerString = Array.from(fileBuffer.slice(4, headerSize + 4))
    //     .map((c) => String.fromCharCode(c))
    //     .join("");

    // const headerObject = JSON.parse(headerString);
    // const files = Object.keys(headerObject);
    // let pos = headerSize + 4;
    // let total = files.length;
    // let done = 0;
    // const funcs = files.map((fileName) => async () => {
    //     const fileContents = fileBuffer.slice(pos, pos + headerObject[fileName]);
    //     const formData = new FormData();
    //     formData.append(
    //         "file",
    //         new Blob([fileContents], { type: "application/octet-stream" }),
    //         fileName.substr(1)
    //     );

    //     return await fetch(`http://${GBSControl.serverIP}/data/restore`, {
    //         method: "POST",
    //         body: formData,
    //     }).then((response) => {
    //         GBSControl.ui.progressRestore.setAttribute(
    //             "gbs-progress",
    //             `${done}/${total}`
    //         );
    //         done++;
    //         pos += headerObject[fileName];
    //         return response;
    //     });
    // });

    // serial(funcs).then(() => {
    //     //   serial(funcs).then(() => {
    //     GBSControl.ui.progressRestore.setAttribute("gbs-progress", ``);
    //     loadUser("a").then(() => {
    //         gbsAlert(
    //             "Restarting GBSControl.\nPlease wait until wifi reconnects then click OK"
    //         )
    //             .then(() => {
    //                 window.location.reload();
    //             })
    //             .catch(() => { });
    //     });
    // });
};

/**
 * Description placeholder
 *
 * @param {Blob} blob
 * @param {string} [name="file.txt"]
 */
// const downloadBlob = (blob: Blob, name = "file.txt") => {
//     let wnav: any = window.navigator;
//     // IE10+
//     if (wnav && wnav.msSaveOrOpenBlob) {
//         wnav.msSaveOrOpenBlob(blob, name);
//         return;
//     }
//     /// normal browsers:
//     // Convert your blob into a Blob URL (a special url that points to an object in the browser's memory)
//     const blobUrl = URL.createObjectURL(blob);

//     // Create a link element
//     const link = document.createElement("a");

//     // Set link's href to point to the Blob URL
//     link.href = blobUrl;
//     link.download = name;

//     // Append link to the body
//     document.body.appendChild(link);

//     // Dispatch click event on the link
//     // This is necessary as link.click() does not work on the latest firefox
//     link.dispatchEvent(
//         new MouseEvent("click", {
//             bubbles: true,
//             cancelable: true,
//             view: window,
//         })
//     );
//     window.URL.revokeObjectURL(blobUrl);
//     // Remove link from body
//     document.body.removeChild(link);
// };

/** button click management */
const controlClick = (control: HTMLButtonElement) => () => {
    const controlKey = control.getAttribute("gbs-control-key");
    const target =
        GBSControl.controlKeysMobile[GBSControl.controlKeysMobileMode];

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
const controlMouseDown = (control: HTMLButtonElement) => () => {
    clearInterval(control["__interval"]);

    const click = controlClick(control);
    click();
    control["__interval"] = window.setInterval(click, 300);
};

/**
 * Description placeholder
 *
 * @param {HTMLButtonElement} control
 * @returns {() => void}
 */
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

/**
 * Description placeholder
 */
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
                action(message, button);
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
                    button["__interval"] = window.setInterval(callback, 300);
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
            GBSControl.controlKeysMobileMode =
                control.getAttribute("gbs-control-target");
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

/**
 * Description placeholder
 */
const initLegendHelpers = () => {
    nodelistToArray<HTMLElement>(
        document.querySelectorAll(".gbs-fieldset__legend--help")
    ).forEach((e) => {
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
            if (
                GBSControl.ws.readyState == 0 ||
                GBSControl.ws.readyState == 1
            ) {
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
    GBSControl.ui.slotButtonList = nodelistToArray(
        document.querySelectorAll('[gbs-role="slot"]')
    ) as HTMLElement[];
};

/**
 * Description placeholder
 */
const initGeneralListeners = () => {
    window.addEventListener("resize", () => {
        updateViewPort();
    });

    GBSControl.ui.backupInput.addEventListener("change", (event) => {
        const fileList: FileList = event.target["files"];
        doRestore(fileList[0]);
        // readLocalFile(fileList[0]);
        GBSControl.ui.backupInput.value = "";
    });

    GBSControl.ui.backupButton.addEventListener("click", doBackup);
    GBSControl.ui.wifiListTable.addEventListener("click", wifiSelectSSID);
    GBSControl.ui.wifiConnectButton.addEventListener("click", wifiConnect);
    GBSControl.ui.wifiApButton.addEventListener("click", wifiSetAPMode);
    GBSControl.ui.wifiStaButton.addEventListener("click", wifiScanSSID);
    GBSControl.ui.developerSwitch.addEventListener(
        "click",
        toggleDeveloperMode
    );
    // GBSControl.ui.customSlotFilters.addEventListener(
    //     "click",
    //     toggleCustomSlotFilters
    // );
    GBSControl.ui.removeSlotButton.addEventListener("click", () => {
        removePreset();
    });

    GBSControl.ui.alertAck.addEventListener("click", () => {
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

/**
 * Description placeholder
 */
const initDeveloperMode = () => {
    const devMode = GBSStorage.read("developerMode") as boolean;
    if (devMode === undefined) {
        GBSStorage.write("developerMode", false);
        updateDeveloperMode(false);
    } else {
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
    let help = GBSStorage.read("help") as boolean;
    if (help === undefined) {
        help = false;
        GBSStorage.write("help", help);
    }
    updateHelp(help);
};

/**
 * Description placeholder
 */
const initUI = () => {
    // updateCustomSlotFilters();
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
        .setAttribute(
            "src",
            document.head
                .querySelector('[rel="apple-touch-icon"]')
                .getAttribute("href")
        );
    fetchSlotNamesAndInit();
};

main();
