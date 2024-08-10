console.log("Hello from js");
const PORT = "6969"
var button = document.getElementById("button");
var ws = new WebSocket("ws://localhost:"+PORT);

button.addEventListener('click', () => {
    console.log("Button clicked");
    ws.send("Idegas");
})