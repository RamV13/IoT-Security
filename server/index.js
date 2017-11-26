var express = require('express');
var app = express();
var bodyParser = require('body-parser');
app.use(bodyParser.json());

var net = require('net');

// TODO authorization

var server = net.createServer((socket) => {
  socket.pipe(socket);
});

server.listen(3002);

app.get('/skip', (req, res) => {
  
});

app.get('/prev', (req, res) => {

});

app.get('/pause', (req, res) => {

});

app.get('/play', (req, res) => {

});

app.listen(3001);

