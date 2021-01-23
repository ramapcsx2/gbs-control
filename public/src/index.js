var __spreadArrays = (this && this.__spreadArrays) || function () {
    for (var s = 0, i = 0, il = arguments.length; i < il; i++) s += arguments[i].length;
    for (var r = Array(s), k = 0, i = 0; i < il; i++)
        for (var a = arguments[i], j = 0, jl = a.length; j < jl; j++, k++)
            r[k] = a[j];
    return r;
};
var toArray = function (nodelist) {
    return Array.prototype.slice.call(nodelist);
};
var GBSStorage = {
    lsObject: {},
    write: function (key, value) {
        GBSStorage.lsObject = GBSStorage.lsObject || {};
        GBSStorage.lsObject[key] = value;
        localStorage.setItem("GBSControlSlotNames", JSON.stringify(GBSStorage.lsObject));
    },
    read: function (key) {
        GBSStorage.lsObject = JSON.parse(localStorage.getItem("GBSControlSlotNames") || "{}");
        return GBSStorage.lsObject[key];
    }
};
var GBSControl = {
    ui: {
        presetButtonList: null,
        slotButtonList: null,
        terminal: null,
        toggleList: null,
        toggleSwichList: null,
        webSocketConnectionWarning: null
    },
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
        19: "slot9"
    },
    controlKeysMobileMode: "move",
    controlKeysMobile: {
        move: {
            type: "loadDoc",
            left: "7",
            up: "*",
            right: "6",
            down: "/"
        },
        scale: {
            type: "loadDoc",
            left: "h",
            up: "4",
            right: "z",
            down: "5"
        },
        borders: {
            type: "loadUser",
            left: "B",
            up: "C",
            right: "A",
            down: "D"
        }
    },
    dataQueued: 0,
    isWsActive: false,
    queuedText: "",
    serverIP: "",
    timeOutWs: 0,
    updateTerminalTimer: 0,
    webSocketServerUrl: "",
    ws: null,
    wsCheckTimer: 0,
    wsConnectCounter: 0,
    wsNoSuccessConnectingCounter: 0,
    wsTimeout: 0
};
/** services */
var checkWebSocketServer = function () {
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
var timeOutWs = function () {
    console.log("timeOutWs");
    if (GBSControl.ws) {
        GBSControl.ws.close();
    }
    GBSControl.isWsActive = false;
    displayWifiWarning(true);
};
var createWebSocket = function () {
    if (GBSControl.ws && checkReadyState()) {
        return;
    }
    GBSControl.wsNoSuccessConnectingCounter = 0;
    GBSControl.ws = new WebSocket(GBSControl.webSocketServerUrl, ["arduino"]);
    GBSControl.ws.onopen = function () {
        console.log("ws onopen");
        displayWifiWarning(false);
        GBSControl.wsConnectCounter++;
        clearTimeout(GBSControl.wsTimeout);
        GBSControl.wsTimeout = setTimeout(timeOutWs, 6000);
        GBSControl.isWsActive = true;
        GBSControl.wsNoSuccessConnectingCounter = 0;
    };
    GBSControl.ws.onclose = function () {
        console.log("ws.onclose");
        clearTimeout(GBSControl.wsTimeout);
        GBSControl.isWsActive = false;
    };
    GBSControl.ws.onmessage = function (message) {
        clearTimeout(GBSControl.wsTimeout);
        GBSControl.wsTimeout = setTimeout(timeOutWs, 2700);
        GBSControl.isWsActive = true;
        var _a = message.data, messageDataAt0 = _a[0], messageDataAt1 = _a[1], messageDataAt2 = _a[2], messageDataAt3 = _a[3], messageDataAt4 = _a[4], messageDataAt5 = _a[5];
        if (messageDataAt0 != "#") {
            GBSControl.queuedText += message.data;
            GBSControl.dataQueued += message.data.length;
            if (GBSControl.dataQueued >= 70000) {
                GBSControl.ui.terminal.value = "";
                GBSControl.dataQueued = 0;
            }
        }
        else {
            var presetId = GBSControl.buttonMapping[messageDataAt1];
            var presetEl = document.querySelector("[gbs-element-ref=\"" + presetId + "\"]");
            var activePresetButton = presetEl
                ? presetEl.getAttribute("gbs-element-ref")
                : null;
            if (activePresetButton) {
                GBSControl.ui.presetButtonList.forEach(toggleButtonActive(activePresetButton));
            }
            var slotId = GBSControl.buttonMapping["1" + messageDataAt2];
            var activeSlotButton = document.querySelector("[gbs-element-ref=\"" + slotId + "\"]");
            if (activeSlotButton) {
                GBSControl.ui.slotButtonList.forEach(toggleButtonActive(slotId));
            }
            if (messageDataAt3 && messageDataAt4 && messageDataAt5) {
                var optionByte0_1 = messageDataAt3.charCodeAt(0);
                var optionByte1_1 = messageDataAt4.charCodeAt(0);
                var optionByte2_1 = messageDataAt5.charCodeAt(0);
                var optionButtonList = __spreadArrays(toArray(GBSControl.ui.toggleList), toArray(GBSControl.ui.toggleSwichList));
                var toggleMethod_1 = function (button, mode) {
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
                optionButtonList.forEach(function (button) {
                    var toggleData = button.getAttribute("gbs-toggle") ||
                        button.getAttribute("gbs-toggle-switch");
                    switch (toggleData) {
                        case "adcAutoGain":
                            toggleMethod_1(button, (optionByte0_1 & 0x01) == 0x01);
                            break;
                        case "scanlines":
                            toggleMethod_1(button, (optionByte0_1 & 0x02) == 0x02);
                            break;
                        case "vdsLineFilter":
                            toggleMethod_1(button, (optionByte0_1 & 0x04) == 0x04);
                            break;
                        case "peaking":
                            toggleMethod_1(button, (optionByte0_1 & 0x08) == 0x08);
                            break;
                        case "palForce60":
                            toggleMethod_1(button, (optionByte0_1 & 0x10) == 0x10);
                            break;
                        case "wantOutputComponent":
                            toggleMethod_1(button, (optionByte0_1 & 0x20) == 0x20);
                            break;
                        /** 1 */
                        case "matched":
                            toggleMethod_1(button, (optionByte1_1 & 0x01) == 0x01);
                            break;
                        case "frameTimeLock":
                            toggleMethod_1(button, (optionByte1_1 & 0x02) == 0x02);
                            break;
                        case "motionAdaptive":
                            toggleMethod_1(button, (optionByte1_1 & 0x04) == 0x04);
                            break;
                        case "bob":
                            toggleMethod_1(button, (optionByte1_1 & 0x04) != 0x04);
                            break;
                        // case "tap6":
                        //   toggleMethod(button, (optionByte1 & 0x08) != 0x04);
                        //   break;
                        case "step":
                            // TODO CHECK
                            toggleMethod_1(button, (optionByte1_1 & 0x10) == 0x10);
                            break;
                        case "fullHeight":
                            toggleMethod_1(button, (optionByte1_1 & 0x20) == 0x20);
                            break;
                        /** 2 */
                        case "enableCalibrationADC":
                            toggleMethod_1(button, (optionByte2_1 & 0x01) == 0x01);
                            break;
                        case "preferScalingRgbhv":
                            toggleMethod_1(button, (optionByte2_1 & 0x02) == 0x02);
                            break;
                    }
                });
            }
        }
    };
};
var checkReadyState = function () {
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
        }
        else {
            return true;
        }
    }
    else if (GBSControl.ws.readyState == 0) {
        GBSControl.wsNoSuccessConnectingCounter++;
        /*console.log(wsNoSuccessConnectingCounter); */
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
var loadDoc = function (link) {
    var xhttp = new XMLHttpRequest();
    xhttp.open("GET", "http://" + GBSControl.serverIP + "/sc?" + link + "&nocache=" + new Date().getTime(), true);
    xhttp.send();
};
var loadUser = function (link) {
    var xhttp = new XMLHttpRequest();
    xhttp.open("GET", "http://" + GBSControl.serverIP + "/uc?" + link + "&nocache=" + new Date().getTime(), true);
    xhttp.send();
    if (link == "a" || link == "1") {
        GBSControl.isWsActive = false;
        GBSControl.ui.terminal.value += "\nRestart\n";
        GBSControl.ui.terminal.scrollTop = GBSControl.ui.terminal.scrollHeight;
    }
};
/** helpers */
var toggleButtonActive = function (id) { return function (button, _index, _array) {
    button.removeAttribute("active");
    if (button.getAttribute("gbs-element-ref") === id) {
        button.setAttribute("active", "");
    }
}; };
var displayWifiWarning = function (mode) {
    GBSControl.ui.webSocketConnectionWarning.style.display = mode
        ? "block"
        : "none";
};
var updateTerminal = function () {
    if (GBSControl.queuedText.length > 0) {
        requestAnimationFrame(function () {
            GBSControl.ui.terminal.value += GBSControl.queuedText;
            GBSControl.ui.terminal.scrollTop = GBSControl.ui.terminal.scrollHeight;
            GBSControl.queuedText = "";
        });
    }
};
var savePresset = function () {
    var currentSlot = document.querySelector('[role="slot"][active]');
    if (!currentSlot) {
        return;
    }
    var key = currentSlot.getAttribute("gbs-element-ref");
    var currentName = prompt("Assign a slot name", GBSStorage.read(key) || key);
    GBSStorage.write(key, currentName);
    currentSlot.setAttribute("name", currentName);
    loadUser("4");
};
/** button click management */
var controlClick = function (control) { return function () {
    var controlKey = control.getAttribute("gbs-control-key");
    var target = GBSControl.controlKeysMobile[GBSControl.controlKeysMobileMode];
    switch (target.type) {
        case "loadDoc":
            loadDoc(target[controlKey]);
            break;
        case "loadUser":
            loadUser(target[controlKey]);
            break;
    }
}; };
var controlMouseDown = function (control) { return function () {
    var click = controlClick(control);
    click();
    clearInterval(control["__interval"]);
    control["__interval"] = setInterval(click, 300);
}; };
var controlMouseUp = function (control) { return function () {
    clearInterval(control["__interval"]);
}; };
/** inits */
var initMenuButtons = function () {
    var menuButtons = toArray(document.querySelector(".menu").querySelectorAll("button"));
    var sections = toArray(document.querySelectorAll("section"));
    var scroll = document.querySelector(".scroll");
    menuButtons.forEach(function (button) {
        return button.addEventListener("click", function () {
            var section = this.getAttribute("gbs-section");
            sections.forEach(function (section) { return section.setAttribute("hidden", ""); });
            document
                .querySelector("section[name=\"" + section + "\"]")
                .removeAttribute("hidden");
            menuButtons.forEach(function (btn) { return btn.removeAttribute("active"); });
            this.setAttribute("active", "");
            scroll.scrollTo(0, 1);
        });
    });
};
var initGBSButtons = function () {
    var buttons = toArray(document.querySelectorAll("[gbs-click]"));
    buttons.forEach(function (button) {
        var clickMode = button.getAttribute("gbs-click");
        if (clickMode === "normal") {
            button.addEventListener("click", function () {
                var message = this.getAttribute("gbs-message");
                var messageType = this.getAttribute("gbs-message-type");
                var action = messageType === "user" ? loadUser : loadDoc;
                action(message);
            });
        }
        if (clickMode === "repeat") {
            var callback_1 = function () {
                var message = button.getAttribute("gbs-message");
                var messageType = button.getAttribute("gbs-message-type");
                var action = messageType === "user" ? loadUser : loadDoc;
                action(message);
            };
            button.addEventListener(!("ontouchstart" in window) ? "mousedown" : "touchstart", function () {
                callback_1();
                clearInterval(button["__interval"]);
                button["__interval"] = setInterval(callback_1, 300);
            });
            button.addEventListener(!("ontouchstart" in window) ? "mouseup" : "touchend", function () {
                clearInterval(button["__interval"]);
            });
        }
    });
};
var initClearButton = function () {
    document.querySelector(".clear").addEventListener("click", function () {
        GBSControl.ui.terminal.value = "";
    });
};
var initControlMobileKeys = function () {
    var controls = document.querySelectorAll("[gbs-control-target]");
    var controlsKeys = document.querySelectorAll("[gbs-control-key]");
    controls.forEach(function (control) {
        control.addEventListener("click", function () {
            controls.forEach(function (control) {
                control.removeAttribute("active");
            });
            GBSControl.controlKeysMobileMode = control.getAttribute("gbs-control-target");
            control.setAttribute("active", "");
        });
    });
    controlsKeys.forEach(function (control) {
        control.addEventListener(!("ontouchstart" in window) ? "mousedown" : "touchstart", controlMouseDown(control));
        control.addEventListener(!("ontouchstart" in window) ? "mouseup" : "touchend", controlMouseUp(control));
    });
};
var initSlotNames = function () {
    for (var i = 1; i < 10; i++) {
        var slot = "slot" + i;
        document
            .querySelector("[gbs-element-ref=\"" + slot + "\"]")
            .setAttribute("name", GBSStorage.read(slot) || slot);
    }
};
var initLegendHelpers = function () {
    toArray(document.querySelectorAll("legend")).forEach(function (e) {
        e.addEventListener("click", function () {
            document.body.classList.toggle("hide-help");
        });
    });
};
var initUnloadListener = function () {
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
var initUI = function () {
    GBSControl.ui.terminal = document.getElementById("outputTextArea");
    GBSControl.ui.webSocketConnectionWarning = document.getElementById("websocketWarning");
    GBSControl.ui.presetButtonList = toArray(document.querySelectorAll("[role='presset']"));
    GBSControl.ui.slotButtonList = toArray(document.querySelectorAll('[role="slot"]'));
    GBSControl.ui.toggleList = document.querySelectorAll("[gbs-toggle]");
    GBSControl.ui.toggleSwichList = document.querySelectorAll("[gbs-toggle-switch]");
    initMenuButtons();
    // initUserButtons();
    // initActionButtons();
    initGBSButtons();
    initClearButton();
    initControlMobileKeys();
    // initGainKeys();
    initSlotNames();
    initLegendHelpers();
    initUnloadListener();
};
var createIntervalChecks = function () {
    GBSControl.wsCheckTimer = setInterval(checkWebSocketServer, 500);
    GBSControl.updateTerminalTimer = setInterval(updateTerminal, 50);
};
var main = function () {
    var ip = location.hostname;
    GBSControl.serverIP = ip;
    GBSControl.webSocketServerUrl = "ws://" + ip + ":81/";
    initUI();
    createWebSocket();
    createIntervalChecks();
};
main();
