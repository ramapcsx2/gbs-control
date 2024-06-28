/*
 * DEVELOPER MEMO:
 *   1. WebUI icons: https://jossef.github.io/material-design-icons-iconfont
 *   2. prettier config: https://prettier.io/docs/en/options
 *
 */

interface String {
    format(...params: string[]): string;
}

if (!String.prototype.format) {
    String.prototype.format = function(...arg) {
        const a = arg
        return this.replace(/\[(\d+)\]/g, function(val, i) {
            return undefined !== typeof(a[i]) ? a[i] : val
        });
    }
}

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
    name: string
    /**
     * Description placeholder
     *
     * @type {("byte" | "string")}
     */
    type: 'byte' | 'string'
    /**
     * Description placeholder
     *
     * @type {number}
     */
    size: number
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
    [key: string]: Struct[]
}

/**
 * Must be aligned with slots.h -> SlotMeta structure
 *
 * @type {StructDescriptors}
 */
const Structs: StructDescriptors = {
    slots: [
        { name: 'name', type: 'string', size: 25 },
        { name: 'slot', type: 'byte', size: 1 },
        { name: 'resolutionID', type: 'byte', size: 1 },

        { name: 'scanlines', type: 'byte', size: 1 },
        { name: 'scanlinesStrength', type: 'byte', size: 1 },
        { name: 'vdsLineFilter', type: 'byte', size: 1 },
        { name: 'stepResponse', type: 'byte', size: 1 },
        { name: 'peaking', type: 'byte', size: 1 },
        { name: 'adcAutoGain', type: 'byte', size: 1 },
        { name: 'frameTimeLock', type: 'byte', size: 1 },

        { name: 'frameTimeLockMethod', type: 'byte', size: 1 },
        { name: 'motionAdaptive', type: 'byte', size: 1 },
        { name: 'bob', type: 'byte', size: 1 },
        { name: 'fullHeight', type: 'byte', size: 1 },
        // { name: 'matchPreset', type: 'byte', size: 1 },
        { name: 'palForce60', type: 'byte', size: 1 },
    ],
}

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
        const currentStruct = structsDescriptors[struct]

        this.pos = 0
        buff = new Uint8Array(buff)

        if (currentStruct) {
            const structSize = StructParser.getSize(structsDescriptors, struct)

            return [...Array(buff.byteLength / structSize)].map(() => {
                return currentStruct.reduce((acc, structItem) => {
                    acc[structItem.name] = this.getValue(buff, structItem)
                    return acc
                }, {})
            })
        }

        return null
    },
    getValue(
        buff: any[],
        structItem: { type: 'byte' | 'string'; size: number }
    ) {
        switch (structItem.type) {
            case 'byte':
                return buff[this.pos++]

            case 'string':
                const currentPos = this.pos
                this.pos += structItem.size

                return [...Array(structItem.size)]
                    .map(() => ' ')
                    .map((_char, index) => {
                        if (buff[currentPos + index] > 31) {
                            return String.fromCharCode(buff[currentPos + index])
                        }
                        return ''
                    })
                    .join('')
                    .trim()
        }
    },
    getSize(structsDescriptors: StructDescriptors, struct: string) {
        const currentStruct = structsDescriptors[struct]
        return currentStruct.reduce((acc, prop) => {
            acc += prop.size
            return acc
        }, 0)
    },
}

/* GBSControl Global Object*/
const GBSControl = {
    buttonMapping: {
        0: 'button240p',
        2: 'button960p',
        3: 'button960p', // 50Hz
        4: 'button1024p',
        5: 'button1024p', // 50Hz
        6: 'button720p',
        7: 'button720p', // 50Hz
        8: 'button480p',
        9: 'button480p', // 50Hz
        10: 'button1080p',
        11: 'button1080p', // 50Hz
        12: 'button15kHz',
        13: 'button15kHz', // 50Hz
        15: 'button576p', // 50Hz
        16: 'buttonSourcePassThrough',
        // 18: 'buttonSourcePassThrough', // OutputHdBypass
        // 20: 'buttonSourcePassThrough', // OutputRGBHVBypass
    },
    controlKeysMobileMode: 'move',
    controlKeysMobile: {
        move: {
            type: 'loadDoc',
            left: '7',
            up: '*',
            right: '6',
            down: '/',
        },
        scale: {
            type: 'loadDoc',
            left: 'h',
            up: '4',
            right: 'z',
            down: '5',
        },
        borders: {
            type: 'loadUser',
            left: 'B',
            up: 'C',
            right: 'A',
            down: 'D',
        },
    },
    activeResolution: '',
    dataQueued: 0,
    isWsActive: false,
    maxSlots: 50,
    queuedText: '',
    scanSSIDDone: false,
    serverIP: '',
    structs: null,
    // timeOutWs: 0,
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
        registerCmdRegSection: null,
        registerCmdRegOp: null,
        registerCmdRegData: null,
        registerCmdRegSubmit: null,
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
        createSlotButton: null,
    },
    updateTerminalTimer: 0,
    webSocketServerUrl: '',
    wifi: {
        mode: 'ap',
        ssid: '',
    },
    ws: null,
    wsCheckTimer: 0,
    // wsConnectCounter: 0,
    wsNoSuccessConnectingCounter: 0,
    developerMode: false,
    wsHeartbeatInterval: null
    // wsTimeout: 0
}

/**
 * Description placeholder
 *
 * @type {({ lsObject: {}; write(key: string, value: any): void; read(key: string): string | number | boolean; })}
 */
const GBSStorage = {
    lsObject: {},
    write(key: string, value: any) {
        GBSStorage.lsObject = GBSStorage.lsObject || {}
        GBSStorage.lsObject[key] = value
        localStorage.setItem(
            'GBSControlSlotNames',
            JSON.stringify(GBSStorage.lsObject)
        )
    },
    read(key: string): string | number | boolean {
        GBSStorage.lsObject = JSON.parse(
            localStorage.getItem('GBSControlSlotNames') || '{}'
        )
        return GBSStorage.lsObject[key]
    },
}


/**
 * Reset current section(tab) to slots
 */
const resetCurrentPageSection = () => {
    GBSStorage.write('section', 'presets')
}


/**
 * Make active/visible the section which is currently in
 * GBSStorage.read('section')
 */
const syncSectionTab = () => {
    const menuButtons = nodelistToArray<HTMLButtonElement>(
        document.querySelector('.gbs-menu').querySelectorAll('button')
    )
    menuButtons.forEach((button) => {
        const sectionName = button.getAttribute('gbs-section')
        // get back to the last section after reload
        if(GBSStorage.read('section') === sectionName)
            button.dispatchEvent(new Event("click"))
    })
}

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
                    GBSControl.ws.close()
                    break
                case 3:
                    GBSControl.ws = null
                    break
            }
        }
        if (!GBSControl.ws) {
            createWebSocket()
        }
    }
}

/**
 * Description placeholder
 */
// const timeOutWs = () => {
//     console.log('timeOutWs')

//     if (GBSControl.ws) {
//         GBSControl.ws.close()
//     }

//     GBSControl.isWsActive = false
//     displayWifiWarning(true)
// }

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
    return Array.prototype.slice.call(nodelist)
}

/**
 * Description placeholder
 *
 * @param {string} id
 * @returns {(button: HTMLElement, _index: any, _array: any) => void}
 */
const toggleButtonActive = (id: string) => (button: HTMLElement, _index: any, _array: any) => {
    button.removeAttribute('active')

    if (button.getAttribute('gbs-element-ref') === id) {
        button.setAttribute('active', '')
    }
}

/**
 * Description placeholder
 *
 * @type {{ resolve: any; reject: any; }}
 */
const gbsAlertPromise = {
    resolve: null,
    reject: null,
}

/**
 * Description placeholder
 *
 * @param {*} event
 */
const alertKeyListener = (event: any) => {
    if (event.keyCode === 13) {
        gbsAlertPromise.resolve()
    }
    if (event.keyCode === 27) {
        gbsAlertPromise.reject()
    }
}

const alertActEventListener = (e: any) => {
    gbsAlertPromise.reject()
}

/**
 * Description placeholder
 *
 * @param {string} text
 * @returns {*}
 */
const gbsAlert = (text: string, ackText: string = '', actText: string = '') => {
    GBSControl.ui.alertContent.insertAdjacentHTML('afterbegin', text)
    GBSControl.ui.alert.removeAttribute('hidden')
    document.addEventListener('keyup', alertKeyListener)
    if (ackText !== '') {
        GBSControl.ui.alertAck.insertAdjacentHTML('afterbegin', ackText)
    } else
        GBSControl.ui.alertAck.insertAdjacentHTML(
            'afterbegin',
            '<div class="gbs-icon">done</div><div>L{JS_YES}</div>'
        )

    if (actText !== '') {
        GBSControl.ui.alertAct.insertAdjacentHTML('afterbegin', actText)
        GBSControl.ui.alertAct.removeAttribute('disabled')
        GBSControl.ui.alertAct.addEventListener('click', alertActEventListener)
    }
    return new Promise((resolve, reject) => {
        const gbsAlertClean = () => {
            document.removeEventListener('keyup', alertKeyListener)
            GBSControl.ui.alertAct.removeEventListener(
                'click',
                alertActEventListener
            )
            GBSControl.ui.alertAct.setAttribute('disabled', '')
            GBSControl.ui.alertAck.textContent = ''
            GBSControl.ui.alertAct.textContent = ''
            GBSControl.ui.alertContent.textContent = ''
            GBSControl.ui.alert.setAttribute('hidden', '')
        }
        gbsAlertPromise.resolve = (e) => {
            gbsAlertClean()
            return resolve(e)
        }
        gbsAlertPromise.reject = () => {
            gbsAlertClean()
            return reject()
        }
    })
}

/**
 * Description placeholder
 *
 * @type {{ resolve: any; reject: any; }}
 */
const gbsPromptPromise = {
    resolve: null,
    reject: null,
}

/**
 * Description placeholder
 *
 * @param {string} text
 * @param {string} [defaultValue=""]
 * @returns {*}
 */
const gbsPrompt = (text: string, defaultValue = '') => {
    GBSControl.ui.promptContent.textContent = text
    GBSControl.ui.prompt.removeAttribute('hidden')
    GBSControl.ui.promptInput.value = defaultValue

    return new Promise<string>((resolve, reject) => {
        gbsPromptPromise.resolve = resolve
        gbsPromptPromise.reject = reject
        GBSControl.ui.promptInput.focus()
    })
}

/**
 * Description placeholder
 *
 * @param {boolean} mode
 */
const displayWifiWarning = (mode: boolean) => {
    GBSControl.ui.webSocketConnectionWarning.style.display = mode
        ? 'block'
        : 'none'
}

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
    if (button.tagName === 'TD') {
        button.innerText = mode ? 'toggle_on' : 'toggle_off'
    }
    button = button.tagName !== 'TD' ? button : button.parentElement
    if (mode) {
        button.setAttribute('active', '')
    } else {
        button.removeAttribute('active')
    }
}

/**
 * Enable / Disable 'remove slot' button depending on active slot
 *
 * @param {HTMLElement} button this is a slot button HTMLElement
 */
const removeSlotButtonCheck = (button: Element) => {
    if (button.hasAttribute('active')) {
        const currentName = button.getAttribute('gbs-name')
        if (currentName && currentName.trim() !== 'Empty') {
            GBSControl.ui.removeSlotButton.removeAttribute('disabled')
        } else {
            GBSControl.ui.removeSlotButton.setAttribute('disabled', '')
        }
    }
}


/**
 * Enable / Disable 'create slot' button depending on active slot
 *
 * @param {Element} button
 */
const createSlotButtonCheck = (button: Element) => {
    if (button.hasAttribute('active')) {
        const currentName = button.getAttribute('gbs-name')
        if (currentName && currentName.trim() === 'Empty') {
            GBSControl.ui.createSlotButton.removeAttribute('disabled')
        } else {
            GBSControl.ui.createSlotButton.setAttribute('disabled', '')
        }
    }
}

/**
 * Description placeholder
 */
const updateTerminal = () => {
    if (GBSControl.queuedText.length > 0) {
        requestAnimationFrame(() => {
            GBSControl.ui.terminal.value += GBSControl.queuedText
            GBSControl.ui.terminal.scrollTop =
                GBSControl.ui.terminal.scrollHeight
            GBSControl.queuedText = ''
        })
    }
}

/**
 * Description placeholder
 */
const updateViewPort = () => {
    document.documentElement.style.setProperty(
        '--viewport-height',
        window.innerHeight + 'px'
    )
}

/**
 * Handle webSocket response
 */
const createWebSocket = () => {
    if (GBSControl.ws && checkReadyState()) {
        return
    }

    GBSControl.wsNoSuccessConnectingCounter = 0
    GBSControl.ws = new WebSocket(GBSControl.webSocketServerUrl, ['arduino'])
    // change message data type
    GBSControl.ws.binaryType = "arraybuffer";

    if(!GBSControl.ws) {
        displayWifiWarning(true)
        return
    }

    GBSControl.ws.onopen = () => {
        console.log("ws.open")
        displayWifiWarning(false)
        GBSControl.isWsActive = true
        GBSControl.wsNoSuccessConnectingCounter = 0
    }

    GBSControl.ws.onclose = () => {
        console.log("ws.close")
        displayWifiWarning(true)
        GBSControl.isWsActive = false
    }

    GBSControl.ws.onmessage = (message: any) => {
        // GBSControl.wsTimeout = window.setTimeout(timeOutWs, 2700)
        GBSControl.isWsActive = true
        // let buf = null
        // try {
        //     buf = await message.data.arrayBuffer()
        // } catch (err) {
        //     // must not exit here since we're filtering out
        //     // terminal data and system state data with '#'
        // }
        if (!(message.data instanceof ArrayBuffer))
            return;
        // into array of DEC values
        const bufUint8Arr = new Uint8Array(message.data)
        const bufArr = Array.from(bufUint8Arr)
        const [
            optionByte0, // always #
            optionByte1, // current slot ID (int)
            optionByte2, // current resolution ()
            // system preferences (preference file values)
            optionByte3, // wantScanlines & wantVdsLineFilter & wantStepResponse & wantPeaking & enableAutoGain & enableFrameTimeLock
            optionByte4, // deintMode & wantTap6 & wantFullHeight & matchPresetSource & PalForce60
            optionByte5, // wantOutputComponent & enableCalibrationADC & preferScalingRgbhv & disableExternalClockGenerator
            // developer tab
            optionByte6, // developerMode, printInfos, invertSync, oversampling, ADC Filter, debugView, freezeCapture
            // system tab
            optionByte7, // enableOTA
            optionByte8, // videoMode
        ] = bufArr

        if (optionByte0 != '#'.charCodeAt(0)) {
            if (!("TextDecoder" in window))
                GBSControl.queuedText = 'L{DEVELOPER_JS_ALERT_TEXTDECODER}';
            const decoder = new TextDecoder("utf-8")

            GBSControl.queuedText += decoder.decode(bufUint8Arr)
            GBSControl.dataQueued += message.data.length

            if (GBSControl.dataQueued >= 70000) {
                GBSControl.ui.terminal.value = ''
                GBSControl.dataQueued = 0
            }
            return
        }

        // current slot
        // const slotId = `slot-${String.fromCharCode(optionByte1)}`
        const slotId = `slot-${optionByte1}`
        const activeSlotButton = document.querySelector(
            `[gbs-element-ref="${slotId}"]`
        )
        if (activeSlotButton) {
            GBSControl.ui.slotButtonList.forEach(toggleButtonActive(slotId))
            // control slot remove button
            removeSlotButtonCheck(activeSlotButton)
            createSlotButtonCheck(activeSlotButton)
        }
        // curent resolution
        // const resID = GBSControl.buttonMapping[String.fromCharCode(optionByte2)]
        const resID = GBSControl.buttonMapping[optionByte2]
        // somehow indicate that this is passthrough which is scaled
        // (?) but not a downscale option
        const passThroughButton = document.querySelector('[gbs-message="W"][gbs-message-type="user"]')
        if((optionByte5 & 0x04) == 0x04 && optionByte2 != 12 && optionByte2 != 13) {
            // we have RGB/HV and scale RGB/HV option enabled, so
            // reflect this in UI
            passThroughButton.setAttribute('active', '')
            passThroughButton.setAttribute('disabled', '')
        } else {
            passThroughButton.removeAttribute('active')
            passThroughButton.removeAttribute('disabled')
        }
        const resEl = document.querySelector(`[gbs-element-ref="${resID}"]`)
        const activePresetButton = resEl
            ? resEl.getAttribute('gbs-element-ref')
            : 'none'
        GBSControl.ui.presetButtonList.forEach(
            toggleButtonActive(activePresetButton)
        )
        // settings tab & system preferences
        const optionButtonList = [
            ...nodelistToArray<HTMLButtonElement>(GBSControl.ui.toggleList),
            ...nodelistToArray<HTMLButtonElement>(
                GBSControl.ui.toggleSwichList
            ),
        ]

        optionButtonList.forEach((button) => {
            const toggleData =
                button.getAttribute('gbs-toggle') ||
                button.getAttribute('gbs-toggle-switch')

            if (toggleData !== null) {
                switch (toggleData) {
                    /** 0: settings */
                    // case "scanlinesStrength":
                    /** 1 */
                    case 'scanlines':
                        // disable scanlines control button if enabled MotionAdaptive
                        if((optionByte4 & 0x01) == 0x01
                            && (optionByte8 == 1 || optionByte8 == 2 || optionByte8 == 14))
                            // MAD enabled + specific video mode
                            button.setAttribute('disabled', '')
                        else
                            toggleButtonCheck(button, (optionByte3 & 0x01) == 0x01)
                        break
                    case 'vdsLineFilter':
                        toggleButtonCheck(button, (optionByte3 & 0x02) == 0x02)
                        break
                    case 'stepResponse':
                        toggleButtonCheck(button, (optionByte3 & 0x04) == 0x04)
                        break
                    case 'peaking':
                        toggleButtonCheck(button, (optionByte3 & 0x08) == 0x08)
                        break
                    case 'adcAutoGain':
                        toggleButtonCheck(button, (optionByte3 & 0x10) == 0x10)
                        break
                    case 'frameTimeLock':
                        toggleButtonCheck(button, (optionByte3 & 0x20) == 0x20)
                        break
                    /** 2 */
                    // case "fameTimeLockMethod":
                    /** 3 */
                    case 'motionAdaptive':
                        toggleButtonCheck(button, (optionByte4 & 0x01) == 0x01)
                        break
                    case 'bob':
                        toggleButtonCheck(button, (optionByte4 & 0x02) == 0x02)
                        break
                    case 'fullHeight':
                        toggleButtonCheck(button, (optionByte4 & 0x04) == 0x04)
                        break
                    case 'palForce60':
                        toggleButtonCheck(button, (optionByte4 & 0x08) == 0x08)
                        break
                    // case 'matchPreset':
                    //     toggleButtonCheck(button, (optionByte4 & 0x08) == 0x08)
                    //     break
                    /** 4: system preferences tab */
                    case 'wantOutputComponent':
                        toggleButtonCheck(button, (optionByte5 & 0x01) == 0x01)
                        break
                    case 'enableCalibrationADC':
                        toggleButtonCheck(button, (optionByte5 & 0x02) == 0x02)
                        break
                    case 'preferScalingRgbhv':
                        toggleButtonCheck(button, (optionByte5 & 0x04) == 0x04)
                        break
                    case 'disableExternalClockGenerator':
                        toggleButtonCheck(button, (optionByte5 & 0x08) == 0x08)
                        break
                }
            }
        })
        // developer tab
        const printInfoButton = document.querySelector(
            `button[gbs-message="i"][gbs-message-type="action"]`
        )
        const invertSync = document.querySelector(
            `button[gbs-message="8"][gbs-message-type="action"]`
        )
        const oversampling = document.querySelector(
            `button[gbs-message="o"][gbs-message-type="action"]`
        )
        const adcFilter = document.querySelector(
            `button[gbs-message="F"][gbs-message-type="action"]`
        )
        const debugView = document.querySelector(
            `button[gbs-message="D"][gbs-message-type="action"]`
        )
        const freezeCaptureButton = document.querySelector(
            `button[gbs-message="F"][gbs-message-type="user"]`
        )
        const syncWatcherButton = document.querySelector(
            `button[gbs-message="m"][gbs-message-type="action"]`
        )
        if ((optionByte6 & 0x01) == 0x01)
            GBSControl.developerMode = true;
        else
            GBSControl.developerMode = false;
        toggleDeveloperMode()

        if ((optionByte6 & 0x02) == 0x02)
            printInfoButton.setAttribute('active', '')
        else
            printInfoButton.removeAttribute('active')
        if ((optionByte6 & 0x04) == 0x04)
            invertSync.setAttribute('active', '')
        else
            invertSync.removeAttribute('active')
        if ((optionByte6 & 0x08) == 0x08)
            oversampling.setAttribute('active', '')
        else
            oversampling.removeAttribute('active')
        if ((optionByte6 & 0x10) == 0x10)
            adcFilter.setAttribute('active', '')
        else
            adcFilter.removeAttribute('active')
        if ((optionByte6 & 0x20) == 0x20)
            debugView.setAttribute('active', '')
        else
            debugView.removeAttribute('active')
        if ((optionByte6 & 0x40) == 0x40)
            freezeCaptureButton.setAttribute('active', '')
        else
            freezeCaptureButton.removeAttribute('active')
        if ((optionByte6 & 0x80) == 0x80)
            syncWatcherButton.setAttribute('active', '')
        else
            syncWatcherButton.removeAttribute('active')

        // system tab
        const enableOTAButton = document.querySelector(
            `button[gbs-message="c"][gbs-message-type="action"]`
        )
        if ((optionByte7 & 0x01) == 0x01)
            enableOTAButton.setAttribute('active', '')
        else enableOTAButton.removeAttribute('active')
    }
}

/**
 * Description placeholder
 *
 * @returns {boolean}
 */
const checkReadyState = () => {
    if (GBSControl.ws.readyState == 2) {
        GBSControl.wsNoSuccessConnectingCounter++

        if (GBSControl.wsNoSuccessConnectingCounter >= 7) {
            console.log('ws still closing, force close')
            GBSControl.ws = null
            GBSControl.wsNoSuccessConnectingCounter = 0
            /* fall through */
            createWebSocket()
            return false
        } else {
            return true
        }
    } else if (GBSControl.ws.readyState == 0) {
        GBSControl.wsNoSuccessConnectingCounter++

        if (GBSControl.wsNoSuccessConnectingCounter >= 14) {
            console.log('ws still connecting, retry')
            GBSControl.ws.close()
            GBSControl.wsNoSuccessConnectingCounter = 0
        }
        return true
    } else {
        return true
    }
}

/**
 * Description placeholder
 */
const createIntervalChecks = () => {
    GBSControl.wsCheckTimer = window.setInterval(checkWebSocketServer, 500)
    GBSControl.updateTerminalTimer = window.setInterval(updateTerminal, 50)
}

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
    ).catch(() => {
        // do something
    })
}

/**
 * user command handler
 *
 * @param {string} link
 * @returns {*}
 */
const loadUser = (link: string) => {
    if (link == 'a' || link == '1') {
        GBSControl.isWsActive = false
        GBSControl.ui.terminal.value += '\nL{DEVICE_RESTARTING_CONSOLE_MESSAGE}\n'
        GBSControl.ui.terminal.scrollTop = GBSControl.ui.terminal.scrollHeight
        resetCurrentPageSection()
    }

    return fetch(
        `http://${GBSControl.serverIP}/uc?${link}&nocache=${new Date().getTime()}`
    ).catch(() => {
        // do something
    })
}

/** WIFI management */
const wifiGetStatus = () => {
    return fetch(`http://${GBSControl.serverIP}/wifi/status?${+new Date()}`)
        .then((r) => r.json())
        .then((wifiStatus: { mode: string; ssid: string }) => {
            GBSControl.wifi = wifiStatus
            if (GBSControl.wifi.mode === 'ap' || GBSControl.wifi.ssid.length == 0) {
                GBSControl.ui.wifiApButton.setAttribute('active', '')
                GBSControl.ui.wifiApButton.setAttribute('disabled', '')
                GBSControl.ui.wifiApButton.classList.add(
                    'gbs-button__secondary'
                )
                GBSControl.ui.wifiStaButton.removeAttribute('active', '')
                GBSControl.ui.wifiStaButton.classList.remove(
                    'gbs-button__secondary'
                )
                GBSControl.ui.wifiStaSSID.innerHTML = 'STA | Scan Network'
            } else {
                GBSControl.ui.wifiApButton.removeAttribute('active', '')
                GBSControl.ui.wifiApButton.classList.remove(
                    'gbs-button__secondary'
                )
                GBSControl.ui.wifiStaButton.setAttribute('active', '')
                GBSControl.ui.wifiStaButton.classList.add(
                    'gbs-button__secondary'
                )
                GBSControl.ui.wifiStaSSID.innerHTML = `${GBSControl.wifi.ssid}`
            }
        }).catch(() => {
            // do something
        })
}

/**
 * Does connect to selected WiFi network
 */
const wifiConnect = () => {
    const ssid = GBSControl.ui.wifiSSDInput.value
    const password = GBSControl.ui.wifiPasswordInput.value

    if (!password.length) {
        GBSControl.ui.wifiPasswordInput.classList.add('gbs-wifi__input--error')
        return
    }

    const formData = new FormData()
    formData.append('n', ssid)
    formData.append('p', password)

    fetch(`http://${GBSControl.serverIP}/wifi/connect`, {
        method: 'POST',
        body: formData,
    }).then(() => {
        gbsAlert(
            'L{CONNECT_JS_ALERT_MESSAGE}'.format(ssid)
            // `GBSControl will restart and will connect to ${ssid}. Please wait few seconds then press OK`
        )
            .then(() => {
                window.location.href = 'http://gbscontrol.local/'
            })
            .catch(() => {})
    }).catch(() => {
        // do something
    })
}

/**
 * Query WiFi networks
 */
const wifiScanSSID = () => {
    GBSControl.ui.wifiStaButton.setAttribute('disabled', '')
    GBSControl.ui.wifiListTable.innerHTML = ''

    if (!GBSControl.scanSSIDDone) {
        fetch(`http://${GBSControl.serverIP}/wifi/list?${+new Date()}`, {
            method: 'POST',
        }).then(() => {
            GBSControl.scanSSIDDone = true
            window.setTimeout(wifiScanSSID, 3000)
        }).catch(() => {
            // do something
        })
        return
    }

    fetch(`http://${GBSControl.serverIP}/wifi/list?${+new Date()}`, {
        method: 'POST',
    })
        .then((e) => e.text())
        .then((result) => {
            GBSControl.scanSSIDDone = false
            return result.length
                ? result
                    .split('\n')
                    .map((line) => line.split(','))
                    .map(([strength, encripted, ssid]) => {
                        return { strength, encripted, ssid }
                    })
                : []
        })
        .then((ssids) => {
            return ssids.reduce((acc, ssid) => {
                return `${acc}<tr gbs-ssid="${ssid.ssid}">
                <td class="gbs-icon" style="opacity:${
                    parseInt(ssid.strength, 10) / 100
                }">wifi</td>
                <td>${ssid.ssid}</td>
                <td class="gbs-icon">${ssid.encripted ? 'lock' : 'lock_open'}</td>
                </tr>`
            }, '')
        })
        .then((html) => {
            GBSControl.ui.wifiStaButton.removeAttribute('disabled')

            if (html.length) {
                GBSControl.ui.wifiListTable.innerHTML = html
                GBSControl.ui.wifiList.removeAttribute('hidden')
                GBSControl.ui.wifiConnect.setAttribute('hidden', '')
            }
        }).catch(() => {
            // do something
        })
}

/**
 * Description placeholder
 *
 * @param {Event} event
 */
const wifiSelectSSID = (event: Event) => {
    ;(GBSControl.ui.wifiSSDInput as HTMLInputElement).value = (
        event.target as HTMLElement
    ).parentElement.getAttribute('gbs-ssid')
    GBSControl.ui.wifiPasswordInput.classList.remove('gbs-wifi__input--error')
    GBSControl.ui.wifiList.setAttribute('hidden', '')
    GBSControl.ui.wifiConnect.removeAttribute('hidden')
}

/**
 * Description placeholder
 *
 * @returns {*}
 */
const wifiSetAPMode = () => {
    if (GBSControl.wifi.mode === 'ap') {
        return
    }

    return fetch(`http://${GBSControl.serverIP}/wifi/ap`, {
        method: 'POST',
        // body: formData,
    }).then((response) => {
        gbsAlert(
            'L{ACCESS_POINT_MODE_JS_ALERT_MESSAGE}'
            // 'Switching to AP mode. Please connect to gbscontrol SSID and then click OK'
        )
            .then(() => {
                window.location.href = 'http://192.168.4.1'
            })
            .catch(() => {})
        return response
    }).catch(() => {
        // do something
    })
}

/** SLOT management */

/**
 * Description placeholder
 */
const fetchSlotNamesErrorRetry = () => {
    window.setTimeout(fetchSlotNamesAndInit, 1000)
}

/**
 * Description placeholder
 */
const initUIElements = () => {
    GBSControl.ui = {
        terminal: document.getElementById('outputTextArea'),
        webSocketConnectionWarning: document.getElementById('websocketWarning'),
        presetButtonList: nodelistToArray(
            document.querySelectorAll("[gbs-role='preset']")
        ) as HTMLElement[],
        slotButtonList: nodelistToArray(
            document.querySelectorAll('[gbs-role="slot"]')
        ) as HTMLElement[],
        registerCmdRegSection: document.querySelector('[gbs-register-section]'),
        registerCmdRegOp: document.querySelector('[gbs-register-operation]'),
        registerCmdRegData: document.querySelector('[gbs-register-data]'),
        registerCmdRegSubmit: document.querySelector('[gbs-register-submit]'),
        toggleConsole: document.querySelector('[gbs-output-toggle]'),
        toggleList: document.querySelectorAll('[gbs-toggle]'),
        toggleSwichList: document.querySelectorAll('[gbs-toggle-switch]'),
        wifiList: document.querySelector('[gbs-wifi-list]'),
        wifiListTable: document.querySelector('.gbs-wifi__list'),
        wifiConnect: document.querySelector('.gsb-wifi__connect'),
        wifiConnectButton: document.querySelector('[gbs-wifi-connect-button]'),
        wifiSSDInput: document.querySelector('[gbs-input="ssid"]'),
        wifiPasswordInput: document.querySelector('[gbs-input="password"]'),
        wifiApButton: document.querySelector('[gbs-wifi-ap]'),
        wifiStaButton: document.querySelector('[gbs-wifi-station]'),
        wifiStaSSID: document.querySelector('[gbs-wifi-station-ssid]'),
        loader: document.querySelector('.gbs-loader'),
        progressBackup: document.querySelector('[gbs-progress-backup]'),
        progressRestore: document.querySelector('[gbs-progress-restore]'),
        outputClear: document.querySelector('[gbs-output-clear]'),
        slotContainer: document.querySelector('[gbs-slot-html]'),
        backupButton: document.querySelector('.gbs-backup-button'),
        backupInput: document.querySelector('.gbs-backup-input'),
        developerSwitch: document.querySelector('[gbs-dev-switch]'),
        // customSlotFilters: document.querySelector("[gbs-slot-custom-filters]"),
        alert: document.querySelector('section[name="alert"]'),
        alertAck: document.querySelector('[gbs-alert-ack]'),
        alertAct: document.querySelector('[gbs-alert-act]'),
        alertContent: document.querySelector('[gbs-alert-content]'),
        prompt: document.querySelector('section[name="prompt"]'),
        promptOk: document.querySelector('[gbs-prompt-ok]'),
        promptCancel: document.querySelector('[gbs-prompt-cancel]'),
        promptContent: document.querySelector('[gbs-prompt-content]'),
        promptInput: document.querySelector('[gbs-input="prompt-input"]'),
        removeSlotButton: document.querySelector(
            '[gbs-element-ref="buttonRemoveSlot"]'
        ),
        createSlotButton: document.querySelector(
            '[gbs-element-ref="buttonCreateSlot"]'
        ),
    }
}

/**
 * Description placeholder
 */
const fetchSlotNamesAndInit = () => {
    fetchSlotNames()
        .then((success) => {
            if (!success) {
                fetchSlotNamesErrorRetry()
                return
            }
            initUIElements()
            wifiGetStatus().then(() => {
                initUI()
                updateSlotNames()
                createWebSocket()
                createIntervalChecks()
                window.setTimeout(hideLoading, 1000)
            })
        }, fetchSlotNamesErrorRetry)
        .catch(fetchSlotNamesErrorRetry)
}

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
                StructParser.getSize(Structs, 'slots') * GBSControl.maxSlots
            ) {
                GBSControl.structs = {
                    slots: StructParser.parseStructArray(
                        arrayBuffer,
                        Structs,
                        'slots'
                    ),
                }
                return true
            }
            return false
        }).catch(() => {
            // do something
        })
}

/**
 * Remove slot handler
 */
const removePreset = () => {
    const currentSlot = document.querySelector('[gbs-role="slot"][active]')

    if (!currentSlot) {
        return
    }

    const currentIndex = currentSlot.getAttribute('gbs-message')
    const currentName = currentSlot.getAttribute('gbs-name')
    if (currentName && currentName.trim() !== 'Empty') {
        gbsAlert(
            'L{REMOVE_SLOT_JS_ALERT_MESSAGE}'.format(currentName),
            // `<p>Are you sure to remove slot: ${currentName}?</p><p>This action also removes all related presets.</p>`,
            '<div class="gbs-icon">done</div><div>L{JS_YES}</div>',
            '<div class="gbs-icon">close</div><div>L{JS_NO}</div>'
        )
            .then(() => {
                return fetch(
                    `http://${GBSControl.serverIP}/slot/remove?index=${currentIndex}&${+new Date()}`
                ).then(() => {
                    console.log('slot removed, reloadng slots...')
                    fetchSlotNames().then((success: boolean) => {
                        if (success) {
                            updateSlotNames()
                        } else {
                            fetchSlotNamesErrorRetry()
                        }
                    })
                }).catch(() => {
                    // do something
                })
            })
            .catch(() => {})
    }
}

/**
 *
 * @returns
 */
const savePreset = () => {
    const currentSlot = document.querySelector('[gbs-role="slot"][active]')

    if (!currentSlot) {
        return
    }

    const key = currentSlot.getAttribute('gbs-element-ref')
    const currentIndex = currentSlot.getAttribute('gbs-message')
    gbsPrompt(
        'Assign a slot name',
        GBSControl.structs.slots[currentIndex].name || key
    )
        .then((currentName: string) => {
            if (currentName && currentName.trim() !== 'Empty') {
                currentSlot.setAttribute('gbs-name', currentName)
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
                                updateSlotNames()
                            }
                        })
                    }, 500)
                    // });
                }).catch(() => {
                    // do something
                })
            }
        })
        .catch(() => {})
}

/**
 * Description placeholder
 *
 * @returns {*}
 */
const getSlotsHTML = () => {
    // TODO: 'i' max. rely on SLOTS_TOTAL which is ambigous
    let str = ``
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
        ></button>`
    }
    return str
}

/**
 * Description placeholder
 *
 * @param {string} slot
 */
const setSlot = (slot: string, el: HTMLElement) => {
    fetch(`http://${GBSControl.serverIP}/slot/set?index=${slot}&${+new Date()}`)
    .catch(() => {
        // do something
    })
}

/**
 * Description placeholder
 */
const updateSlotNames = () => {
    for (let i = 0; i < GBSControl.maxSlots; i++) {
        const el = document.querySelector(
            `[gbs-message="${i}"][gbs-role="slot"]`
        )

        el.setAttribute('gbs-name', GBSControl.structs.slots[i].name)
        el.setAttribute(
            'gbs-meta',
            getSlotPresetName(GBSControl.structs.slots[i].resolutionID)
        )
    }
}


/**
 * Must be aligned with options.h -> OutputResolution
 *
 * @param {string} resolutionID
 * @returns {("1280x960" | "1280x1024" | "1280x720" | "720x480" | "1920x1080" | "DOWNSCALE" | "768×576" | "BYPASS (HD)" | "BYPASS (RGBHV)" | "240p" | "NONE")}
 */
const getSlotPresetName = (resolutionID: number) => {
    switch (resolutionID) {
        case 2: //'c':
        case 3: //'d':
            // case 0x011:
            return '1280x960'
        case 4: //'e':
        case 5: //'f':
            // case 0x012:
            return '1280x1024'
        case 6: //'g':
        case 7: //'h':
            // case 0x013:
            return '1280x720'
        case 8: //'i':
        case 9: //'j':
            // case 0x015:
            return '720x480'
        case 10: //'k':
        case 11: //'l':
            return '1920x1080'
        case 12: //'m':
        case 13: //'n':
            // case 0x016:
            return 'DOWNSCALE'
        case 15: //'p':
            return '768×576'
        case 18: //'s':
            return 'BYPASS (HD)'
        case 20: //'u':
            return 'BYPASS (RGBHV)'
        case 0: //'a':
            return '240p'
        default:
            return 'NONE'
    }
}

/** Promises */
const serial = (funcs: (() => Promise<any>)[]) =>
    funcs.reduce(
        (promise, func) =>
            promise.then((result) =>
                func().then(Array.prototype.concat.bind(result))
            ),
        Promise.resolve([])
    )

/** helpers */

const toggleHelp = () => {
    let help = GBSStorage.read('help') || false

    GBSStorage.write('help', !help)
    updateHelp(!help)
}

/**
 * Description placeholder
 *
 * @param {boolean} help
 */
const updateHelp = (help: boolean) => {
    if (help) {
        document.body.classList.remove('gbs-help-hide')
    } else {
        document.body.classList.add('gbs-help-hide')
    }
}


/**
 * A simple functionality that changes visual represetation of register data
 * dependint on the operation
 */
const switchRegisterCmdOp = () => {
    const dta = GBSControl.ui.registerCmdRegData.value
    if(GBSControl.ui.registerCmdRegOp.value === '0') {
        // write
        GBSControl.ui.registerCmdRegData.value = dta.replaceAll('\u21E5', '\u2190')
    } else {
        // read
        GBSControl.ui.registerCmdRegData.value = dta.replaceAll('\u2190', '\u21E5')
    }
}

/**
 * Prepare and submit register data
 */
const submitRegisterCmd = () => {
    const formData = new FormData();
    GBSControl.ui.registerCmdRegSubmit.setAttribute('disabled', '')
    formData.append('s', GBSControl.ui.registerCmdRegSection.value.trim())
    formData.append('o', GBSControl.ui.registerCmdRegOp.value.trim())
    const dataStringRaw = GBSControl.ui.registerCmdRegData.value.trim()
    if(dataStringRaw.length <= 1) {
        GBSControl.ui.registerCmdRegData.focus()
        GBSControl.ui.registerCmdRegData.classList.add('gbs-focus-form-element')
        GBSControl.ui.registerCmdRegSubmit.removeAttribute('disabled')
        console.log('data cannot be empty')
        return false
    }
    const dataRaw = dataStringRaw.split(' ')
    var data = new Array()
    dataRaw.map((val: string) => {
        // there are different delimiters for read and write
        const reg = val.split((GBSControl.ui.registerCmdRegOp.value === '0' ? '\u2190' : '\u21E5'))
        // only pairs [address <- value | -> length]
        if(reg[0] !== undefined && reg[1] !== undefined) {
            const a = parseInt(reg[0], 16)
            const v = parseInt(reg[1], 16)
            data.push(a)
            data.push(v)
        }
    })
    formData.append('d', data.join(','))

    fetch(`http://${GBSControl.serverIP}/data/cmd`, {
        method: 'POST',
        body: formData,
    }).then((response) => {
        console.log('data sent, response: ', response.statusText)
        GBSControl.ui.registerCmdRegSubmit.removeAttribute('disabled')
    })
}

/**
 * Toggle console visibility (see corresponding button)
 *
 */
const updateConsoleVisibility = () => {
    // const developerMode = GBSStorage.read('developerMode') || false
    if (GBSControl.developerMode) {
        const consoleStatus = GBSStorage.read('consoleVisible') as boolean
        if (consoleStatus != true) {
            GBSStorage.write('consoleVisible', true)
            GBSControl.ui.toggleConsole.removeAttribute('active')
            document.body.classList.remove('gbs-output-hide')
        } else {
            GBSStorage.write('consoleVisible', false)
            GBSControl.ui.toggleConsole.setAttribute('active', '')
            document.body.classList.add('gbs-output-hide')
        }
    }
}

/**
 * Toggle developer mode (see WS heartbeat)
 */
const toggleDeveloperMode = () => {
    const el = document.querySelector('[gbs-section="developer"]') as HTMLElement
    const consoleStatus = GBSStorage.read('consoleVisible') as boolean
    if (GBSControl.developerMode) {
        el.removeAttribute('hidden')
        if(consoleStatus == true) {
            document.body.classList.remove('gbs-output-hide')
        }
        GBSControl.ui.developerSwitch.setAttribute('active', '')
        GBSControl.ui.developerSwitch.querySelector('.gbs-icon').innerText = "toggle_on"
    } else {
        if(GBSStorage.read('section') === 'developer') {
            resetCurrentPageSection()
            syncSectionTab()
        }
        el.setAttribute('hidden', '')
        GBSStorage.write('consoleVisible', true)
        document.body.classList.add('gbs-output-hide')
        GBSControl.ui.developerSwitch.removeAttribute('active')
        GBSControl.ui.developerSwitch.querySelector('.gbs-icon').innerText = "toggle_off"
        window.clearInterval(GBSControl.wsHeartbeatInterval);
    }
};


/**
 * Description placeholder
 */
const hideLoading = () => {
    GBSControl.ui.loader.setAttribute('style', 'display:none')
}

/**
 * Description placeholder
 *
 * @param {Response} response
 * @returns {Response}
 */
const checkFetchResponseStatus = (response: Response) => {
    if (!response.ok) {
        throw new Error(`HTTP ${response.status} - ${response.statusText}`)
    }
    return response
}

/** backup / restore */

const doBackup = () => {
    window.location.href = `http://${GBSControl.serverIP}/data/backup?ts=${new Date().getTime()}`
}

/**
 * Restore SLOTS from backup
 *
 * @param {ArrayBuffer} file
 */
// const doRestore = (file: ArrayBuffer, f: File) => {
const doRestore = (f: File) => {
    const { backupInput } = GBSControl.ui

    const bkpTs = f.name.substring(
        f.name.lastIndexOf('-') + 1,
        f.name.lastIndexOf('.')
    )
    const backupDate = new Date(parseInt(bkpTs))

    backupInput.setAttribute('disabled', '')
    const formData = new FormData()
    formData.append('gbs-backup.bin', f, f.name)
    const setAlertBody = () => {
        const fsize = f.size / 1024
        return (
            'L{RESTORE_JS_ALERT_MESSAGE}'.format(backupDate.toLocaleString(), fsize.toFixed(2))
            // '<p>Backup File:</p><p>Backup date: ' +
            // backupDate.toLocaleString() +
            // '</p><p>Size: ' +
            // fsize.toFixed(2) +
            // ' kB</p>'
        )
    }
    gbsAlert(
        setAlertBody() as string,
        '<div class="gbs-icon">close</div><div>L{ALERT_BUTTON_JS_REJECT}</div>',
        '<div class="gbs-icon">done</div><div>L{ALERT_BUTTON_JS_RESTORE}</div>'
    )
        .then(
            () => {
                backupInput.removeAttribute('disabled')
            },
            () => {
                return fetch(`http://${GBSControl.serverIP}/data/restore`, {
                    method: 'POST',
                    body: formData,
                }).then((response) => {
                    // backupInput.removeAttribute("disabled");
                    // start with 1st tab
                    resetCurrentPageSection()
                    window.setTimeout(() => {
                        window.location.reload()
                    }, 4000)
                    return response
                }).catch(() => {
                    // do something
                })
            }
        )
        .catch(() => {
            backupInput.removeAttribute('disabled')
        })
}

/** button click management */
const controlClick = (control: HTMLButtonElement) => () => {
    const controlKey = control.getAttribute('gbs-control-key')
    const target =
        GBSControl.controlKeysMobile[GBSControl.controlKeysMobileMode]

    switch (target.type) {
        case 'loadDoc':
            loadDoc(target[controlKey])
            break
        case 'loadUser':
            loadUser(target[controlKey])
            break
    }
}

/**
 * Description placeholder
 *
 * @param {HTMLButtonElement} control
 * @returns {() => void}
 */
const controlMouseDown = (control: HTMLButtonElement) => () => {
    clearInterval(control['__interval'])

    const click = controlClick(control)
    click()
    control['__interval'] = window.setInterval(click, 300)
}

/**
 * Description placeholder
 *
 * @param {HTMLButtonElement} control
 * @returns {() => void}
 */
const controlMouseUp = (control: HTMLButtonElement) => () => {
    clearInterval(control['__interval'])
}

/** inits */
const initMenuButtons = () => {
    const menuButtons = nodelistToArray<HTMLButtonElement>(
        document.querySelector('.gbs-menu').querySelectorAll('button')
    )
    const sections = nodelistToArray<HTMLElement>(
        document.querySelectorAll('section')
    )
    const scroll = document.querySelector('.gbs-scroll')

    let currentPage = GBSStorage.read('section') || 'presets'
    if(!GBSControl.developerMode && currentPage === 'developer')
        currentPage = 'presets'
    menuButtons.forEach((button) => {
        const sectionName = button.getAttribute('gbs-section')
        button.addEventListener('click', () => {
            const section = sectionName;
            GBSStorage.write('section', section)
            sections.forEach((section) => section.setAttribute('hidden', ''))
            document
                .querySelector(`section[name="${section}"]`)
                .removeAttribute('hidden')

            menuButtons.forEach((btn) => btn.removeAttribute('active'))
            button.setAttribute('active', '')
            scroll.scrollTo(0, 1)
        })
        // get back to the last section after reload
        if(currentPage === sectionName)
            button.dispatchEvent(new Event("click"))
    })
}

/**
 * Description placeholder
 */
const initGBSButtons = () => {
    const actions = {
        user: loadUser,
        action: loadDoc,
        setSlot,
    }

    const buttons = nodelistToArray<HTMLElement>(
        document.querySelectorAll('[gbs-click]')
    )

    buttons.forEach((button) => {
        const clickMode = button.getAttribute('gbs-click')
        const message = button.getAttribute('gbs-message')
        const messageType = button.getAttribute('gbs-message-type')
        const action = actions[messageType]

        if (clickMode === 'normal') {
            // custom events applied for some buttons
            button.addEventListener('click', () => {
                if(message == '1' && messageType == 'user') {
                    // reset to defaults (factory) button
                    gbsAlert(
                        'L{RESET_FACTORY_BUTTON_JS_ALERT_MESSAGE}',
                        '<div class="gbs-icon">close</div><div>L{JS_NO}</div>',
                        '<div class="gbs-icon">done</div><div>L{ALERT_BUTTON_JS_ACK}</div>'
                    )
                    .then(
                        () => {
                            // do nothing
                        },
                        () => {
                            button.setAttribute('disabled', '')
                            action(message, button)
                            window.setTimeout(() => {
                                window.location.reload()
                            }, 5000)
                        }
                    ).catch(() => {
                    })
                } else if(message == '2' && messageType == 'user') {
                    // reset active slot
                    gbsAlert(
                        'L{RESET_ACTIVE_SLOT_JS_ALERT_MESSAGE}',
                        '<div class="gbs-icon">close</div><div>L{JS_NO}</div>',
                        '<div class="gbs-icon">done</div><div>L{ALERT_BUTTON_JS_ACK}</div>'
                    )
                    .then(
                        () => {
                            // do nothing
                        },
                        () => {
                            button.setAttribute('disabled', '')
                            action(message, button)
                            window.setTimeout(() => {
                                window.location.reload()
                            }, 5000)
                        }
                    ).catch(() => {
                    });
                } else if(message == 'a' && messageType == 'user') {
                    // RESTART button
                    button.setAttribute('disabled', '')
                    action(message, button)
                    // restart device and reload page after countdown
                    let countdown = 10;
                    const buttonMessage = (val: string) => {
                        button.innerHTML = `<div class="gbs-text-center">L{PAGE_RELOAD_IN}</div><div class="gbs-text-center">${val} L{SECONDS_SHORT}</div>`
                    }
                    window.setInterval(() => {
                        buttonMessage(countdown.toString())
                        countdown--;
                        if(countdown == 0)
                            window.location.reload()
                    }, 1000)
                } else {
                    // all other buttons
                    action(message, button)
                }
            });
        }

        if (clickMode === 'repeat') {
            const callback = () => {
                action(message)
            }

            button.addEventListener(
                !('ontouchstart' in window) ? 'mousedown' : 'touchstart',
                () => {
                    callback()
                    clearInterval(button['__interval'])
                    button['__interval'] = window.setInterval(callback, 300)
                }
            )
            button.addEventListener(
                !('ontouchstart' in window) ? 'mouseup' : 'touchend',
                () => {
                    clearInterval(button['__interval'])
                }
            )
        }
    })
}

/**
 * Description placeholder
 */
const initClearButton = () => {
    GBSControl.ui.outputClear.addEventListener('click', () => {
        GBSControl.ui.terminal.value = ''
    })
}

/**
 * Description placeholder
 */
const initControlMobileKeys = () => {
    const controls = document.querySelectorAll('[gbs-control-target]')
    const controlsKeys = document.querySelectorAll('[gbs-control-key]')

    controls.forEach((control) => {
        control.addEventListener('click', () => {
            GBSControl.controlKeysMobileMode =
                control.getAttribute('gbs-control-target')
            controls.forEach((crtl) => {
                crtl.removeAttribute('active')
            })
            control.setAttribute('active', '')
        })
    })

    controlsKeys.forEach((control) => {
        control.addEventListener(
            !('ontouchstart' in window) ? 'mousedown' : 'touchstart',
            controlMouseDown(control as HTMLButtonElement)
        )
        control.addEventListener(
            !('ontouchstart' in window) ? 'mouseup' : 'touchend',
            controlMouseUp(control as HTMLButtonElement)
        )
    })
}

/**
 * Description placeholder
 */
const initLegendHelpers = () => {
    nodelistToArray<HTMLElement>(
        document.querySelectorAll('.gbs-fieldset__legend--help')
    ).forEach((e) => {
        e.addEventListener('click', toggleHelp)
    })
}

/**
 * Description placeholder
 */
const initUnloadListener = () => {
    window.addEventListener('unload', () => {
        clearInterval(GBSControl.wsCheckTimer)
        if (GBSControl.ws) {
            if (
                GBSControl.ws.readyState == 0 ||
                GBSControl.ws.readyState == 1
            ) {
                GBSControl.ws.close()
            }
        }
    })
}

/**
 * Description placeholder
 */
const initSlotButtons = () => {
    GBSControl.ui.slotContainer.innerHTML = getSlotsHTML()
    GBSControl.ui.slotButtonList = nodelistToArray(
        document.querySelectorAll('[gbs-role="slot"]')
    ) as HTMLElement[]
}

/**
 * Description placeholder
 */
const initGeneralListeners = () => {
    window.addEventListener('resize', () => {
        updateViewPort()
    })

    GBSControl.ui.backupInput.addEventListener('change', (event) => {
        const fileList: FileList = event.target['files']
        doRestore(fileList[0])
        // readLocalFile(fileList[0]);
        GBSControl.ui.backupInput.value = ''
    })

    GBSControl.ui.backupButton.addEventListener('click', doBackup)
    GBSControl.ui.wifiListTable.addEventListener('click', wifiSelectSSID)
    GBSControl.ui.wifiConnectButton.addEventListener('click', wifiConnect)
    GBSControl.ui.wifiApButton.addEventListener('click', wifiSetAPMode)
    GBSControl.ui.wifiStaButton.addEventListener('click', wifiScanSSID)
    // GBSControl.ui.developerSwitch.addEventListener('click', () => {
    //     const developerMode = GBSStorage.read('developerMode') || false
    //     GBSStorage.write('developerMode', !developerMode)
    //     updateConsoleVisibility()
    // })
    GBSControl.ui.removeSlotButton.addEventListener('click', () => {
        removePreset()
    })
    GBSControl.ui.createSlotButton.addEventListener('click', () => {
        savePreset()
    })

    GBSControl.ui.alertAck.addEventListener('click', () => {
        GBSControl.ui.alert.setAttribute('hidden', '')
        gbsAlertPromise.resolve()
    })

    GBSControl.ui.promptOk.addEventListener('click', () => {
        GBSControl.ui.prompt.setAttribute('hidden', '')
        const value = GBSControl.ui.promptInput.value
        if (value !== undefined || value.length > 0) {
            gbsPromptPromise.resolve(GBSControl.ui.promptInput.value)
        } else {
            gbsPromptPromise.reject()
        }
    })

    GBSControl.ui.promptCancel.addEventListener('click', () => {
        GBSControl.ui.prompt.setAttribute('hidden', '')
        gbsPromptPromise.reject()
    })

    GBSControl.ui.promptInput.addEventListener('keydown', (event: any) => {
        if (event.keyCode === 13) {
            GBSControl.ui.prompt.setAttribute('hidden', '')
            const value = GBSControl.ui.promptInput.value
            if (value !== undefined || value.length > 0) {
                gbsPromptPromise.resolve(GBSControl.ui.promptInput.value)
            } else {
                gbsPromptPromise.reject()
            }
        }
        if (event.keyCode === 27) {
            gbsPromptPromise.reject()
        }
    })

    // register cmd data filtering function
    GBSControl.ui.registerCmdRegData.addEventListener('keydown', (e: KeyboardEvent) => {
        const cc = e.key.charCodeAt(0)
        const target = e.target as HTMLTextAreaElement;
        if(cc != 0x42) {
            // not the backspace key
            if((cc > 57 && cc < 97) || cc < 48 || cc > 102) { // && cc != 0x20) {
                e.preventDefault()
                return false
            }
            GBSControl.ui.registerCmdRegData.classList.remove('gbs-focus-form-element')
            const dtaLen = target.value.length
            if(dtaLen % 6 == 0) {
                target.value = `${target.value} `
            } else if(dtaLen % 3 == 0) {
                target.value = (GBSControl.ui.registerCmdRegOp.value === '0' ? `${target.value}\u2190` : `${target.value}\u21E5`)
            }
        } else {
            // backspace
            e.preventDefault();
            const lastChar = target.value.charAt(target.value.length-1)
            target.value = target.value.substring(0, target.value.length - ((lastChar == ' ' || lastChar == '\u2190' || lastChar == '\u21E5') ? 3 : 1))
        }
    })

    // register cmd switch operation
    GBSControl.ui.registerCmdRegOp.addEventListener('change', () => {
        switchRegisterCmdOp()
    })

    // register cmd submit button in developer tab
    GBSControl.ui.registerCmdRegSubmit.addEventListener('click', () => {
        submitRegisterCmd()
    })

    // toggle console visibility button
    GBSControl.ui.toggleConsole.addEventListener('click', () => {
        updateConsoleVisibility()
    })
}

/**
 * Description placeholder
 */
const initHelp = () => {
    let help = GBSStorage.read('help') as boolean
    if (help === undefined) {
        help = false
        GBSStorage.write('help', help)
    }
    updateHelp(help)
}

/**
 * Description placeholder
 */
const initUI = () => {
    // updateCustomSlotFilters();
    initGeneralListeners()
    updateViewPort()
    initSlotButtons()
    initLegendHelpers()
    initMenuButtons()
    initGBSButtons()
    initClearButton()
    initControlMobileKeys()
    initUnloadListener()
    initHelp()
}

/**
 * Description placeholder
 */
const main = () => {
    GBSControl.serverIP = location.hostname
    GBSControl.webSocketServerUrl = `ws://${GBSControl.serverIP}:81/`
    document
        .querySelector('.gbs-loader img')
        .setAttribute(
            'src',
            document.head
                .querySelector('[rel="apple-touch-icon"]')
                .getAttribute('href')
        )
    fetchSlotNamesAndInit()
}

main()