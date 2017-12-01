var express = require('express');
var app = express();
var bodyParser = require('body-parser');
app.use(bodyParser.json());

var net = require('net');

var fs = require('fs');
var lineReader = require('readline');

var _ = require('lodash');

var AsyncLock = require('async-lock');
var sleep = require('system-sleep');

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

// TODO calculate optimal values
const CHUNK_SIZE = 10;
const PLAYING_DELAY = 1;
const DATA_DELAY = 1;
const CHUNK_DELAY = 10;

var read_stream;

/**
 * Retrieve an array of data bytes from a WAV file given a song in a format
 * suitable for a 12-bit DAC
 */
function get_song_bytes(callback) {
  var buffer = [];

  song = 'score.wavs';

  read_stream = fs.createReadStream('./songs/' + song);
  var rl = lineReader.createInterface({input: read_stream});
  rl.on('line', (line) => {
    var data = _.split(line, ',');
    if (data[0] == 10) data[0] = 11;
    buffer.push(String.fromCharCode(data[0]) + String.fromCharCode(data[1]));
    if (_.size(buffer) == CHUNK_SIZE) {
      callback(buffer);
      buffer = [];
      rl.pause();
      sleep(CHUNK_DELAY);
      rl.resume();
    }
  });
/*
  _.forEach(SCORE_SOUND, (x) => {
    buffer.push(x);
    if (_.size(buffer) == CHUNK_SIZE) {
      callback(buffer);
      buffer = [];
    }
  });
*/
/*
  function push(x) {
    buffer.push(x);
    if (_.size(buffer) == CHUNK_SIZE) {
      callback(buffer);
      buffer = [];
    }
    setTimeout(() => { push(x+1); }, 1);
  }
  push(0);
*/
}

var datas = [];
var datas_lock = new AsyncLock();
const DATAS_KEY = 'DATAS_KEY';

var primary_socket;

var count = 0;

function run() {
  datas_lock.acquire(DATAS_KEY, (unlock) => {
    function send_data(data) {
      if (playing) {
        count++;
        console.log(count + ': ' + _.head(data));
        primary_socket.write(_.head(data));
      }
      if (_.size(data) > 1) {
        setTimeout(() => { send_data(playing ? _.tail(data) : data); }, playing ? DATA_DELAY : PLAYING_DELAY);
      }
    }

    if (!_.isEmpty(datas)) {
      var head = _.head(datas);
      setTimeout(() => { send_data(head); }, 0);
      datas = _.tail(datas);
    }

    unlock();
    setTimeout(run, _.isEmpty(datas) ? CHUNK_DELAY * 1000 : CHUNK_DELAY);
  });
}

var server = net.createServer((socket) => {
  socket.setEncoding('ascii');
  primary_socket = socket;
  run();
  socket.on('data', (data) => {
    console.log('PIC: ' + data);
    get_song_bytes((data, err) => {
      if (err) return console.error('song byte conversion failure');
      datas_lock.acquire(DATAS_KEY, (unlock) => {
        datas.push(data);
        unlock();
      });
    });
  });
  socket.on('close', (data) => {
    console.log('socket closed');
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

