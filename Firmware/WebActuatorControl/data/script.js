var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onLoad);

function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}
function onOpen(event) {
    console.log('Connection opened');
}
function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}
function onMessage(event) {
    console.log(event);
    
    // parse incoming message
    const msg = JSON.parse(event.data);

    if (msg.type == "report") {
        document.getElementById('status').innerHTML = msg.status;
        document.getElementById('m_vel').innerHTML = msg.m_vel;
        document.getElementById('m_start').innerHTML = msg.m_start;
        document.getElementById('m_end').innerHTML = msg.m_stop;
        document.getElementById('m_pos').innerHTML = msg.m_pos;
    }

    if (msg.type == "serial") {
        document.getElementById('serial-box').value += msg.text + "\n";
    }
}
function onLoad(event) {
    initWebSocket();
    initButtons();
}
function initButtons() {
    document.getElementById('home').addEventListener("click", function(){ click(event); });
    document.getElementById('goHome').addEventListener("click", function(){ click(event); });
    document.getElementById('goStart').addEventListener("click", function(){ click(event); });
    document.getElementById('start').addEventListener("click", function(){ click(event); });
    document.getElementById('stp').addEventListener("click", function(){ click(event); });
}
function click(e) {
    // e is event. e.target will give the thing that was clicked, I hope
    console.log(e);
    console.log(e.target.id)

    const msg = {
        type: "button",
        action: e.target.id
    };

    console.log(msg);

    websocket.send(JSON.stringify(msg));
}
function formSubmit() {
    // https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_client_applications
    // create msg containing form submission and send as JSON
    const msg = {
        type: "settings",
        velocity: document.getElementById("velocity").value,
        start_pos: document.getElementById("start_pos").value,
        end_pos: document.getElementById("end_pos").value
    };

    console.log(msg);

    websocket.send(JSON.stringify(msg));
    
    //alert("form submitted");
}