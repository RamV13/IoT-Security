var express = require('express');
var app = express();
var bodyParser = require('body-parser');
app.use(bodyparser.json());

// TODO API key authorization

app.get('/skip', (req, res) => {
  
});

app.get('/prev', (req, res) => {

});

app.get('/pause', (req, res) => {

});

app.get('/play', (req, res) => {

});

app.listen(3001);

