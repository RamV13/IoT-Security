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

const CHUNK_SIZE = 11025; // TODO calculate optimal chunk size

/**
 * Retrieve an array of data bytes from a WAV file given a song in a format
 * suitable for a 12-bit DAC
 */
function get_song_bytes(song, callback) {
  var file;
  try {
    file = fs.readFileSync('./songs/' + song);
  } catch (err) {
    return callback(null, error('failed to read song: ' + song));
  }
  if (!file) return callback(null, error('failed to read song: ' + song));
  var result;
  try {
    result = wav.decode(file);
  } catch (err) {
    return callback(null, error('failed to decode song: ' + song));
  }
  if (!result) return callback(null, error('failed to decode song: ' + song));
  var data = result.channelData[0];
  var min = _.min(data);
  var max = _.max(data);
  data = _.map(data, (x) => Math.trunc((x - min) * 4095.5 * (max - min)));
  // TODO bitwise OR with DAC CS
  callback(data);
}

var server = net.createServer((socket) => {
  socket.setEncoding('utf8');
  socket.on('data', (data) => {
    get_song_bytes(_.trimEnd(data), (data, err) => {
      if (err) {
        socket.write('failure');
      } else {
        function send_data(data) {
          if (playing) {
            socket.write(_.reduce(_.head(data), (acc, x) => acc + x + ',', ''));
          }
          if (_.size(data) > 1) setTimeout(() => { send_data(playing ? _.tail(data) : data); }, 500);
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

