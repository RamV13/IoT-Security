var express = require('express');
var app = express();
var bodyParser = require('body-parser');
app.use(bodyParser.json());

var net = require('net');

var fs = require('fs');
var wav = require('node-wav');

var _ = require('lodash');

// TODO authorization

function error(msg) {
  console.error(msg);
  return msg;
}

var songs = fs.readdirSync('./songs/');
var num_songs = _.size(songs);
var song_index = 0;
var playing = false;

// SOCKET

const CHUNK_SIZE = 1; // TODO calculate optimal chunk size

/**
 * Retrieve an array of data bytes from a WAV file given a song in a format
 * suitable for a 12-bit DAC
 */
function get_song_bytes(song, callback) {
  fs.readFile('./songs/' + song, (err, data) => {
    if (err) return callback(null, error('failed to read song: ' + song));
    data = _.split(data, ',');
    callback(data);
  });
}

var server = net.createServer((socket) => {
  socket.setEncoding('utf8');
  socket.on('data', (data) => {
    get_song_bytes(_.trimEnd(data), (data, err) => {
      if (err) {
        socket.write('failure');
      } else {
        function send_data(data) {
          if (playing) socket.write('' + _.head(_.head(data)));
          if (_.size(data) > 1) setTimeout(() => { send_data(playing ? _.tail(data) : data); }, 0);
        }
        send_data(_.chunk(data, CHUNK_SIZE));
      }
    });
  });
});

server.listen(3002);

// APPLICATION

app.get('/skip', (req, res) => {
  song_index++;
  if (song_index >= num_songs) song_index = 0;
  res.send();
});

app.get('/prev', (req, res) => {
  song_index--;
  if (song_index < 0) song_index = num_songs - 1;
  res.send();
});

app.get('/pause', (req, res) => {
  playing = false;
  res.send();
});

app.get('/play', (req, res) => {
  playing = true;
  res.send();
});

app.listen(3001);

