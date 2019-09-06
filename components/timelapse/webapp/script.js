'use strict';

//let host = "10.1.1.25";
let host = "localhost";
let ws;
let rdws;
let lastImageUrl = "";
let takePhotoPeriodS = 10;
let fetchStatusTask;
let lastStatusUpdate = 0;
$(document).ready(function () {

    $('#nav-profile-tab').on("click", function (e) {
        ws.send("settings", {});
    });

    $('#nav-about-tab').on("click", function (e) {
        ws.send("status", {});
    });

    $("#nav-home-tab").on("click", updateLastImage);

    $("#connectButtons").find("input[value=0]").on('change', function (e) {
        if (ws) {
            ws.close();
            $('#servermessage').text("Disconnecting");
        }
    });
    $("#connectButtons").find("input[value=1]").on('change', function (e) {
        if (ws && ws.readyState === 0 || ws.readyState === 1) {
            // CONNECTING OR CONNECTED
            return;
        }
        $('#servermessage').text("Connecting")
        connect();
    });


    if ("WebSocket" in window) {
        // Already open?
        connect();
    } else {
        alert("WebSocket NOT supported by your Browser!");
    }


    $(".reloaderClass").on("click", function (e) {
        window.location.reload(false);
    });
    $("#refreshLastImage").on("click", updateLastImage);
    $("#debugSendText").on("keyup", function (e) {
        if (e.keyCode === 13) {
            if (rdws) rdws.send($("#debugSendText")[0].value);
            //e.preventDefault();

            $("#debugSendText")[0].value = "";
        }
    });
    $("#reset").on("click", function (e) {
        if (ws) ws.send("reset", {});
        //e.preventDefault();
    });


    document.getElementById("logrefresh").addEventListener("click", logrefresh);
    //document.getElementById("refreshLastImage").addEventListener("click", updateLastImage);

    document.getElementById("settingssubmit").addEventListener("click", sendFormWS);
    document.getElementById("statusupdate").addEventListener("click", fetchStatus);

    document.getElementById('showDebugOn').addEventListener('click', function (e) {
        var loc = window.location, new_uri;
        if (loc.protocol === "https:") {
            new_uri = "wss:";
        } else {
            new_uri = "ws:";
        }
        new_uri += "//" + loc.host + ":8232";
        //new_uri += loc.pathname;
        rdws = new WebSocket(new_uri);
        //rdws = new WebSocket("ws://" + host + ":8232");

        rdws.onopen = function (e) {
            console.log("Connected to remotedebug");
        };
        rdws.onmessage = function (e) {
            console.log(e.data);
            let tb = document.getElementById('remotedebug');
            var date = (new Date()).toISOString().slice(0, 19) + " : "
            var total = ((tb.value ? tb.value + "\n" : "") + date + e.data).split("\n");

            if (total.length > 20)
                total = total.slice(total.length - 20);

            tb.value = total.join("\n");
        };
        rdws.onclose = function (e) {
            console.log("Close remotedebug");
            if (fetchStatusTask) fetchStatusTask.clearTimeout();
        }
        rdws.onerror = function (e) {
            console.log("Error remotedebug");
        }
    });
    document.getElementById('showDebugOff').addEventListener('click', function (e) {
        if (rdws) {
            rdws.close();
        }
        rdws = null;
    });

});

//            document.getElementById("test").appendChild(renderjson.set_icons('+', '-')(result));

function logrefresh() {
    let lines = $('#loglines').val();
    ws.send("log", {"lines": lines});
}

// Create a request variable and assign a new XMLHttpRequest object to it.
// doesn't work
/*
let xmlhttp = new XMLHttpRequest();
xmlhttp.onreadystatechange = function () {
    if (xmlhttp.readyState == XMLHttpRequest.DONE) {   // XMLHttpRequest.DONE == 4
        if (xmlhttp.status == 200) {
            document.getElementById("logtext").innerHTML = xmlhttp.responseText;
        } else {
            document.getElementById("logtext").innerHTML = "Unavailable. Status: " + xmlhttp.status;
        }
    }
};
xmlhttp.open("GET", "timelapse.log", false);
xmlhttp.send();


}
*/
function sendFormWS(evt) {
    let o = formToObject(evt.target.form.id);
    ws.send(evt.target.form.id, o);
    evt.preventDefault();
}

function updateLastImage(e) {
    // spawn, so can wait for new image
    if (e) e.preventDefault();
    fetchStatus(null); // async!
    setTimeout(displayLastImage, 1000); // simplistic - need to spawn and wait
}

function displayLastImage() {
    if (lastImageUrl) {
        document.getElementById("lastImage").src = lastImageUrl;
        document.getElementById("lastImageCaption").innerHTML = lastImageUrl;
    }
}

function fetchStatus(e) {
    ws.send("status", {});
    if (e) e.preventDefault();
    //fetchStatusTask = setTimeout(fetchStatus, 10000); // add a button
}

function connectButtonRefresh() {
    setTimeout(connectButtonRefresh, 1000);
    var state;
    if (!ws) {
        state = "Not connected"
    } else {
        switch (ws.readyState) {
            case 0:
                state = "Connecting";
                break;
            case 1:
                state = "Connected";
                break;
            case 2:

                state = "Closing";
                break;
            case 3:

                state = "Closed";
                break;
        }
    }
}


function selectRadioBS(element, value) {

    try {
        for (let j = 0; j < element.length; j++) {

            if (element[j].value == value) {
                element[j].checked = true;  // radio
                element[j].parentElement.classList.add("active");  // bootstrap buttons (which are 2x radio buttons
            } else {
                element[j].checked = false;
                element[j].parentElement.classList.remove("active");
            }
        }
    } catch (e) {
        console.log(e);
    }
}

function connect() {
    if (ws) {
        if (ws.readyState === 0 || ws.readyState === 1) {
            // CONNECTING OR CONNECTED
            return;
        }
    }
    var loc = window.location, new_uri;
    if (loc.protocol === "https:") {
        new_uri = "wss:";
    } else {
        new_uri = "ws:";
    }
    new_uri += "//" + loc.host +  ":80/ws";
    //new_uri += loc.pathname;

    //print(new_uri);
    // ws = new FancyWebSocket("ws://" + host + ":80/ws");
    ws = new FancyWebSocket(new_uri);

    ws.bind('settings', function (msg) {
        populate(document.getElementById('settings'), msg);
    });

    ws.bind('log', function (msg) {
        $('#loglines').val(msg.lines);
        let logta = $('#logtext')
        logta.html(msg.log);
        if (logta.length) {
            logta.scrollTop(logta[0].scrollHeight - logta.height());
        }
    });

    ws.bind('status', function (msg) {
        console.log("Status received..." + msg);
        lastStatusUpdate = Date.now();
        //let msg = JSON.parse(evt.data);
        if (msg.hasOwnProperty("lastImage")) {
            lastImageUrl = msg["lastImage"];
        }
        if (msg.hasOwnProperty("takePhotoPeriodS")) {
            takePhotoPeriodS = msg["takePhotoPeriodS"];
        }


        for (let p in msg) {
            if (msg.hasOwnProperty(p)) {
                let t = document.getElementById("statustable");
                let r = document.getElementById(p + "-statustable");
                if (!r && t) {
                    let row = t.insertRow();
                    row.id = p + "-statustable";
                    let n = row.insertCell();
                    let text = document.createTextNode(p);
                    n.appendChild(text);
                    let v = row.insertCell();
                    text = document.createTextNode(msg[p]);
                    v.appendChild(text);
                } else {
                    r.lastChild.textContent = msg[p];
                }
            }
        }
    });

    ws.bind('message', function (msg) {
        $('#servermessage').text(msg.text);
    });

    ws.bind('close', function () {
        console.log("Connection is closed...");
        $('#servermessage').text("Connection closed.");
        //  $('#connectedLabel')[0].innerText = "Connect";
        //  $('#disconnectedLabel')[0].innerText = "(Disconnected)";

        selectRadioBS($('#connectButtons').find("input[type=radio]"), 0);
    });

    ws.bind('open', function () {
        console.log("Connection open.");
        $('#servermessage').text("Connection open.");
        ws.send("message", {"text": "TimeLapseBlob WebApp in da house!!!"});

        // $('#connectButtons').children("label")[0].innerText = "(Connected)";
        //$('#connectButtons').children("label")[1].innerText = "Disconnect";

        selectRadioBS($('#connectButtons').find("input[type=radio]"), 1);
    });
    ws.bind('error', function () {
        $('#servermessage').text("Connection error.");
        console.log("Connection is error(!)...");
    });


}