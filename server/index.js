var express = require('express');
var app = express();
var bodyParser = require('body-parser');
app.use(bodyParser.json());

var net = require('net');

var fs = require('fs');
var lineReader = require('readline');

var _ = require('lodash');

var AsyncLock = require('async-lock');

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
const PLAYING_DELAY = 1000;
const DATA_DELAY = 100;
const CHUNK_DELAY = 1000;

/**
 * Retrieve an array of data bytes from a WAV file given a song in a format
 * suitable for a 12-bit DAC
 */
function get_song_bytes(song, callback) {
  var buffer = [];
  lineReader
    .createInterface({input: fs.createReadStream('./songs/' + song)})
    .on('line', (line) => {
      buffer.push(line);
      if (_.size(buffer) == CHUNK_SIZE) {
        callback(buffer);
        buffer = [];
      }
    });
}

var datas = [];
var datas_lock = new AsyncLock();
const DATAS_KEY = 'DATAS_KEY';

function run() {
  datas_lock.acquire(DATAS_KEY, (unlock) => {
    function send_data(data) {
      if (playing) socket.write('' + _.head(data));
      if (_.size(data) > 1) setTimeout(() => { send_data(playing ? _.tail(data) : data); }, playing ? DATA_DELAY : PLAYING_DELAY);
    }
    send_data(_.head(datas));
    datas = _.tail(datas);
    setTimeout(run, CHUNK_DELAY);
    unlock();
  });
}
run();

var server = net.createServer((socket) => {
  socket.setEncoding('utf8');
  socket.on('data', (data) => {
    console.log('From PIC: ' + data);
    get_song_bytes(_.trimEnd('duwc.wavs'), (data, err) => {
      if (err) return console.error('song byte conversion failure');
      datas_lock.acquire(DATAS_KEY, (unlock) => {
        datas.push(data);
        unlock();
      });
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

