console.log("Hello from js");
const PORT = "6969"
var button = document.getElementById("button");
var textInput = document.getElementById("textInput");
var ws = new WebSocket("ws://localhost:"+PORT);

button.addEventListener('click', () => {
    var msg = textInput.value
    if (msg == ""){
        alert("Can't send empty message!")
        return;
    }
    if (ws.readyState != ws.OPEN){
        alert("WebSocket not open!")
        return;
    }
    ws.send(msg);
    console.log("Message sent!");
})