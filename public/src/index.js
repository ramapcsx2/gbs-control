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
        19: "slot9"
    },
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
    }
};
var toggleButtonsFor = function (id) { return function (button, _index, _array) {
    button.removeAttribute("active");
    if (button.getAttribute("id") === id) {
        button.setAttribute("active", "");
    }
}; };
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
    document.getElementById("overlayNoWs").style.display = "block"; /*show*/
};
var createWebSocket = function () {
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
    GBSControl.ws.onclose = function () {
        console.log("ws.onclose");
        clearTimeout(GBSControl.wsTimeout);
        GBSControl.isWsActive = false;
    };
    GBSControl.ws.onmessage = function (e) {
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
        }
        else {
            var presetId = GBSControl.buttonMapping[e.data[1]];
            var presetEl = document.getElementById(presetId);
            var activePresetButton = presetEl ? presetEl.getAttribute("id") : null;
            if (activePresetButton) {
                var presetButtonList = toArray(document.querySelectorAll("[role='presset']"));
                presetButtonList.forEach(toggleButtonsFor(activePresetButton));
            }
            var slotId = GBSControl.buttonMapping["1" + e.data[2]];
            var activeSlotButton = document.getElementById(slotId);
            if (activeSlotButton) {
                var slotButtonList = toArray(document.querySelectorAll('[role="slot"]'));
                slotButtonList.forEach(toggleButtonsFor(slotId));
            }
            if (e.data[3] && e.data[4] && e.data[5]) {
                var optionByte0_1 = e.data[3].charCodeAt(0);
                var optionByte1_1 = e.data[4].charCodeAt(0);
                var optionByte2_1 = e.data[5].charCodeAt(0);
                var optionButtonList = __spreadArrays(toArray(document.querySelectorAll("[toggle]")), toArray(document.querySelectorAll("[toggle-switch]")));
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
                    var toggleData = button.getAttribute("toggle") ||
                        button.getAttribute("toggle-switch");
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
var updateTerminal = function () {
    if (GBSControl.queuedText.length > 0) {
        requestAnimationFrame(function () {
            GBSControl.theTerminal.value += GBSControl.queuedText;
            GBSControl.theTerminal.scrollTop = GBSControl.theTerminal.scrollHeight;
            GBSControl.queuedText = "";
        });
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
        GBSControl.theTerminal.value += " Restart\n";
        GBSControl.theTerminal.scrollTop = GBSControl.theTerminal.scrollHeight;
    }
};
var initializeMenuButtons = function () {
    var menuButtons = toArray(document.querySelector(".menu").querySelectorAll("button"));
    var sections = toArray(document.querySelectorAll("section"));
    var scroll = document.querySelector(".scroll");
    menuButtons.forEach(function (e) {
        return e.addEventListener("click", function () {
            var section = this.getAttribute("section");
            sections.forEach(function (e) { return e.setAttribute("hidden", ""); });
            document
                .querySelector("section[name=\"" + section + "\"]")
                .removeAttribute("hidden");
            menuButtons.forEach(function (b) { return b.removeAttribute("active"); });
            this.setAttribute("active", "");
            scroll.scrollTo(0, 1);
        });
    });
};
var initializeUserButtons = function () {
    var userButtons = toArray(document.querySelectorAll("[ws-user]"));
    userButtons.forEach(function (e) {
        return e.addEventListener("click", function () {
            var wsUser = this.getAttribute("ws-user");
            loadUser(wsUser);
        });
    });
};
var initializeActionButtons = function () {
    var actionButtons = toArray(document.querySelectorAll("[ws-action]"));
    actionButtons.forEach(function (e) {
        return e.addEventListener("click", function () {
            var wsAction = this.getAttribute("ws-action");
            loadDoc(wsAction);
        });
    });
};
var initializeClearButton = function () {
    document.querySelector(".clear").addEventListener("click", function () {
        GBSControl.theTerminal.value = "";
    });
};
var controlClick = function (control) { return function () {
    var controlKey = control.getAttribute("controls-key");
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
    control["__interval"] = setInterval(click, 300);
}; };
var controlMouseUp = function (control) { return function () {
    clearInterval(control["__interval"]);
}; };
var gainClick = function (control) { return function () {
    var gainKey = control.getAttribute("gain-key");
    switch (gainKey) {
        case "+":
            loadUser("n");
            break;
        case "-":
            loadUser("o");
            break;
    }
}; };
var gainMouseDown = function (control) { return function () {
    var click = gainClick(control);
    click();
    control["__interval"] = setInterval(click, 300);
}; };
var gainMouseUp = function (control) { return function () {
    clearInterval(control["__interval"]);
}; };
var initializeControlMobileKeys = function () {
    document.querySelectorAll("[controls]").forEach(function (control) {
        control.addEventListener("click", function () {
            document.querySelectorAll("[controls]").forEach(function (control) {
                control.removeAttribute("active");
            });
            GBSControl.controlKeysMobileMode = control.getAttribute("controls");
            control.setAttribute("active", "");
        });
    });
    document.querySelectorAll("[controls-key]").forEach(function (control) {
        !("ontouchstart" in window) &&
            control.addEventListener("mousedown", controlMouseDown(control));
        "ontouchstart" in window &&
            control.addEventListener("touchstart", controlMouseDown(control));
        !("ontouchstart" in window) &&
            control.addEventListener("mouseup", controlMouseUp(control));
        "ontouchstart" in window &&
            control.addEventListener("touchend", controlMouseUp(control));
    });
};
var initializeGainKeys = function () {
    document.querySelectorAll("[gain-key]").forEach(function (control) {
        !("ontouchstart" in window) &&
            control.addEventListener("mousedown", gainMouseDown(control));
        "ontouchstart" in window &&
            control.addEventListener("touchstart", gainMouseDown(control));
        !("ontouchstart" in window) &&
            control.addEventListener("mouseup", gainMouseUp(control));
        "ontouchstart" in window &&
            control.addEventListener("touchend", gainMouseUp(control));
    });
};
var savePresset = function () {
    var currentSlot = document.querySelector('[role="slot"][active]');
    var key = currentSlot.getAttribute("id");
    var currentName = prompt("Assign a slot name", GBSStorage.read(key) || key);
    GBSStorage.write(key, currentName);
    currentSlot.setAttribute("name", currentName);
    loadUser("4");
};
var initSlotNames = function () {
    for (var i = 1; i < 10; i++) {
        var slot = "slot" + i;
        document
            .getElementById(slot)
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
var initialize = function () {
    var ip = location.hostname;
    GBSControl.serverIP = ip;
    GBSControl.webSocketServerUrl = "ws://" + ip + ":81/";
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
